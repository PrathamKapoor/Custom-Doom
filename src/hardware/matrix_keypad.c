#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__linux__)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <errno.h>
#endif

#include "matrix_keypad.h"

typedef struct {
    int raw_state;             /* Current raw electrical state (0=released, 1=pressed) */
    int debounced_state;       /* Filtered stable state (0 or 1) */
    uint32_t last_change_time; /* Timestamp in ms of last debounced state transition */
} keypad_key_state_t;

typedef struct {
    int key_index;
    int is_down;
} keypad_event_t;

static int s_initialized = 0;
static int s_is_enabled = 0;
static keypad_config_t s_config;
static keypad_event_callback_t s_callback = NULL;

static keypad_key_state_t s_keys[MATRIX_KEYPAD_TOTAL_KEYS];
static int s_sim_matrix[MATRIX_KEYPAD_ROWS][MATRIX_KEYPAD_COLS];

static uint32_t s_current_time_ms = 1000;
static int s_manual_time_override = 0;

static pthread_t s_thread;
static volatile int s_thread_running = 0;
static int s_thread_started = 0;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

#if defined(__linux__)
static int s_hw_available = 0;
static int s_chip_fd = -1;
static int s_row_handle_fd = -1;
static int s_col_handle_fd = -1;
#endif

static uint32_t get_monotonic_ms(void) {
#if defined(_WIN32)
    return (uint32_t)GetTickCount();
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
    }
    return (uint32_t)(time(NULL) * 1000);
#else
    return (uint32_t)(time(NULL) * 1000);
#endif
}

#if defined(__linux__)
static void linux_gpio_cleanup(void) {
    if (s_row_handle_fd >= 0) {
        close(s_row_handle_fd);
        s_row_handle_fd = -1;
    }
    if (s_col_handle_fd >= 0) {
        close(s_col_handle_fd);
        s_col_handle_fd = -1;
    }
    if (s_chip_fd >= 0) {
        close(s_chip_fd);
        s_chip_fd = -1;
    }
    s_hw_available = 0;
}

static int linux_gpio_init(const keypad_config_t *cfg) {
    linux_gpio_cleanup();

    /* Try common Raspberry Pi GPIO chip devices */
    const char *chips[] = {"/dev/gpiochip0", "/dev/gpiochip4", NULL};
    for (int i = 0; chips[i] != NULL; i++) {
        s_chip_fd = open(chips[i], O_RDWR | O_CLOEXEC);
        if (s_chip_fd >= 0) {
            break;
        }
    }

    if (s_chip_fd < 0) {
        /* Not running on real RPi hardware or insufficient permissions; fallback */
        s_hw_available = 0;
        return 0;
    }

    /* Request 4 output lines for rows (active low, open-drain to avoid electrical contention) */
    struct gpiohandle_request row_req;
    memset(&row_req, 0, sizeof(row_req));
    for (int i = 0; i < MATRIX_KEYPAD_ROWS; i++) {
        row_req.lineoffsets[i] = (unsigned int)cfg->row_pins[i];
        row_req.default_values[i] = 1; /* Idle HIGH (released / floating in open-drain) */
    }
    row_req.lines = MATRIX_KEYPAD_ROWS;
    row_req.flags = GPIOHANDLE_REQUEST_OUTPUT;
#ifdef GPIOHANDLE_REQUEST_OPEN_DRAIN
    row_req.flags |= GPIOHANDLE_REQUEST_OPEN_DRAIN;
#endif
    snprintf(row_req.consumer_label, sizeof(row_req.consumer_label), "doom_keypad_rows");

    if (ioctl(s_chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &row_req) < 0) {
        linux_gpio_cleanup();
        return 0;
    }
    s_row_handle_fd = row_req.fd;

    /* Request 4 input lines for columns with internal pull-ups */
    struct gpiohandle_request col_req;
    memset(&col_req, 0, sizeof(col_req));
    for (int i = 0; i < MATRIX_KEYPAD_COLS; i++) {
        col_req.lineoffsets[i] = (unsigned int)cfg->col_pins[i];
    }
    col_req.lines = MATRIX_KEYPAD_COLS;
    col_req.flags = GPIOHANDLE_REQUEST_INPUT;
#ifdef GPIOHANDLE_REQUEST_BIAS_PULL_UP
    col_req.flags |= GPIOHANDLE_REQUEST_BIAS_PULL_UP;
#endif
    snprintf(col_req.consumer_label, sizeof(col_req.consumer_label), "doom_keypad_cols");

    if (ioctl(s_chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &col_req) < 0) {
        linux_gpio_cleanup();
        return 0;
    }
    s_col_handle_fd = col_req.fd;

    s_hw_available = 1;
    return 1;
}
#endif

int matrix_keypad_init(const keypad_config_t *config, keypad_event_callback_t callback) {
    if (config == NULL || callback == NULL) {
        return -1;
    }

    /* Stop any running scanning thread */
    matrix_keypad_stop();

    pthread_mutex_lock(&s_lock);

    /* CRITICAL PIN CONSTRAINT: Physical Pin 22 (GPIO 25) is strictly reserved for LCD D/C */
    for (int i = 0; i < MATRIX_KEYPAD_ROWS; i++) {
        if (config->row_pins[i] == MATRIX_KEYPAD_RESERVED_PIN_LCD_DC) {
            fprintf(stderr, "[KEYPAD ERROR] Row pin %d is GPIO 25 (reserved for LCD D/C)! Aborting init.\n", i);
            pthread_mutex_unlock(&s_lock);
            return -1;
        }
    }
    for (int i = 0; i < MATRIX_KEYPAD_COLS; i++) {
        if (config->col_pins[i] == MATRIX_KEYPAD_RESERVED_PIN_LCD_DC) {
            fprintf(stderr, "[KEYPAD ERROR] Column pin %d is GPIO 25 (reserved for LCD D/C)! Aborting init.\n", i);
            pthread_mutex_unlock(&s_lock);
            return -1;
        }
    }

    s_config = *config;
    s_callback = callback;
    s_is_enabled = config->keypad_enabled;
    s_initialized = 1;
    s_manual_time_override = 0;
    s_current_time_ms = 1000;

    /* Reset key states and simulation matrix */
    for (int i = 0; i < MATRIX_KEYPAD_TOTAL_KEYS; i++) {
        s_keys[i].raw_state = 0;
        s_keys[i].debounced_state = 0;
        s_keys[i].last_change_time = 0;
    }
    for (int r = 0; r < MATRIX_KEYPAD_ROWS; r++) {
        for (int c = 0; c < MATRIX_KEYPAD_COLS; c++) {
            s_sim_matrix[r][c] = 0;
        }
    }

#if defined(__linux__)
    if (s_is_enabled) {
        linux_gpio_init(&s_config);
    } else {
        linux_gpio_cleanup();
    }
#endif

    pthread_mutex_unlock(&s_lock);
    return 0;
}

void matrix_keypad_simulate_raw_state(int row, int col, int pressed) {
    if (row >= 0 && row < MATRIX_KEYPAD_ROWS && col >= 0 && col < MATRIX_KEYPAD_COLS) {
        pthread_mutex_lock(&s_lock);
        s_sim_matrix[row][col] = pressed ? 1 : 0;
        pthread_mutex_unlock(&s_lock);
    }
}

void matrix_keypad_set_simulated_time(uint32_t time_ms) {
    pthread_mutex_lock(&s_lock);
    s_manual_time_override = 1;
    s_current_time_ms = time_ms;
    pthread_mutex_unlock(&s_lock);
}

void matrix_keypad_advance_simulated_time(uint32_t delta_ms) {
    pthread_mutex_lock(&s_lock);
    s_manual_time_override = 1;
    s_current_time_ms += delta_ms;
    pthread_mutex_unlock(&s_lock);
}

void matrix_keypad_reset_simulated_time(void) {
    pthread_mutex_lock(&s_lock);
    s_manual_time_override = 0;
    pthread_mutex_unlock(&s_lock);
}

int matrix_keypad_is_key_down(int key_index) {
    if (key_index < 0 || key_index >= MATRIX_KEYPAD_TOTAL_KEYS) {
        return 0;
    }
    pthread_mutex_lock(&s_lock);
    int state = s_keys[key_index].debounced_state;
    pthread_mutex_unlock(&s_lock);
    return state;
}

char matrix_keypad_get_key_char(int key_index) {
    static const char key_chars[MATRIX_KEYPAD_TOTAL_KEYS] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        '*', '0', '#', 'D'
    };
    if (key_index >= 0 && key_index < MATRIX_KEYPAD_TOTAL_KEYS) {
        return key_chars[key_index];
    }
    return '\0';
}

void matrix_keypad_poll_once(void) {
    keypad_event_t events[MATRIX_KEYPAD_TOTAL_KEYS];
    int event_count = 0;

    pthread_mutex_lock(&s_lock);

    if (!s_initialized || !s_is_enabled) {
        pthread_mutex_unlock(&s_lock);
        return;
    }

    uint32_t now;
    if (s_thread_running) {
        now = get_monotonic_ms();
    } else if (s_manual_time_override) {
        now = s_current_time_ms;
    } else {
        /* In single-tick manual testing, advance time past the debounce window */
        uint32_t delta = (s_config.debounce_ms > 0) ? (uint32_t)s_config.debounce_ms : 10;
        s_current_time_ms += delta;
        now = s_current_time_ms;
    }

    /* 1. Hardware or Simulation Matrix Scanning */
#if defined(__linux__)
    if (s_hw_available && s_row_handle_fd >= 0 && s_col_handle_fd >= 0) {
        for (int r = 0; r < MATRIX_KEYPAD_ROWS; r++) {
            struct gpiohandle_data row_data;
            for (int i = 0; i < MATRIX_KEYPAD_ROWS; i++) {
                row_data.values[i] = (i == r) ? 0 : 1; /* Drive active row LOW */
            }
            ioctl(s_row_handle_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &row_data);

            /* Brief settling time for line capacitance */
            usleep(5);

            struct gpiohandle_data col_data;
            if (ioctl(s_col_handle_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &col_data) == 0) {
                for (int c = 0; c < MATRIX_KEYPAD_COLS; c++) {
                    int key_index = r * MATRIX_KEYPAD_COLS + c;
                    /* Active LOW: 0 = pressed, 1 = unpressed */
                    s_keys[key_index].raw_state = (col_data.values[c] == 0) ? 1 : 0;
                }
            }
        }

        /* Set all rows back to HIGH when idle */
        struct gpiohandle_data idle_data;
        for (int i = 0; i < MATRIX_KEYPAD_ROWS; i++) {
            idle_data.values[i] = 1;
        }
        ioctl(s_row_handle_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &idle_data);
    } else
#endif
    {
        /* Host / Simulation Scanning */
        for (int r = 0; r < MATRIX_KEYPAD_ROWS; r++) {
            for (int c = 0; c < MATRIX_KEYPAD_COLS; c++) {
                int key_index = r * MATRIX_KEYPAD_COLS + c;
                s_keys[key_index].raw_state = s_sim_matrix[r][c];
            }
        }
    }

    /* 2. Debouncing State Machine (Multi-key Rollover) */
    for (int k = 0; k < MATRIX_KEYPAD_TOTAL_KEYS; k++) {
        int raw = s_keys[k].raw_state;
        int cur = s_keys[k].debounced_state;

        if (raw != cur) {
            uint32_t elapsed = now - s_keys[k].last_change_time;
            if (s_config.debounce_ms <= 0 || elapsed >= (uint32_t)s_config.debounce_ms) {
                s_keys[k].debounced_state = raw;
                s_keys[k].last_change_time = now;

                events[event_count].key_index = k;
                events[event_count].is_down = raw;
                event_count++;
            }
        }
    }

    pthread_mutex_unlock(&s_lock);

    /* 3. Dispatch Callbacks Outside Mutex Lock */
    for (int i = 0; i < event_count; i++) {
        if (s_callback) {
            s_callback(events[i].key_index, events[i].is_down);
        }
    }
}

static void *matrix_keypad_thread_func(void *arg) {
    (void)arg;
    while (s_thread_running) {
        matrix_keypad_poll_once();
        usleep(MATRIX_KEYPAD_POLL_INTERVAL_US);
    }
    return NULL;
}

void matrix_keypad_start_thread(void) {
    pthread_mutex_lock(&s_lock);
    if (!s_initialized || !s_is_enabled || s_thread_started) {
        pthread_mutex_unlock(&s_lock);
        return;
    }

    s_thread_running = 1;
    s_thread_started = 1;

    if (pthread_create(&s_thread, NULL, matrix_keypad_thread_func, NULL) != 0) {
        s_thread_running = 0;
        s_thread_started = 0;
        fprintf(stderr, "[KEYPAD ERROR] Failed to spawn matrix keypad scanning thread\n");
    }
    pthread_mutex_unlock(&s_lock);
}

void matrix_keypad_stop(void) {
    pthread_mutex_lock(&s_lock);
    if (!s_thread_started) {
        pthread_mutex_unlock(&s_lock);
#if defined(__linux__)
        pthread_mutex_lock(&s_lock);
        linux_gpio_cleanup();
        pthread_mutex_unlock(&s_lock);
#endif
        return;
    }

    s_thread_running = 0;
    s_thread_started = 0;
    pthread_t thread_to_join = s_thread;
    pthread_mutex_unlock(&s_lock);

    pthread_join(thread_to_join, NULL);

#if defined(__linux__)
    pthread_mutex_lock(&s_lock);
    linux_gpio_cleanup();
    pthread_mutex_unlock(&s_lock);
#endif
}
