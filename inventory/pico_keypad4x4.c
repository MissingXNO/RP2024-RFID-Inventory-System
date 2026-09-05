#include "pico_keypad4x4.h"
#include "hardware/timer.h"

uint _columns[4];
uint _rows[4];
char _matrix_values[16];

uint all_columns_mask = 0x0;
uint column_mask[4];
absolute_time_t last_press_time;
const uint DEBOUNCE_MS = 50;

void pico_keypad_init(uint columns[4], uint rows[4], char matrix_values[16]) {
    for (int i = 0; i < 16; i++) {
        _matrix_values[i] = matrix_values[i];
    }

    for (int i = 0; i < 4; i++) {
        _columns[i] = columns[i];
        _rows[i] = rows[i];

        gpio_init(_columns[i]);
        gpio_init(_rows[i]);

        gpio_set_dir(_columns[i], GPIO_IN);   // Correct macro
        gpio_set_dir(_rows[i], GPIO_OUT);     // Correct macro

        gpio_put(_rows[i], 1);

        all_columns_mask = all_columns_mask + (1 << _columns[i]);
        column_mask[i] = 1 << _columns[i];
    }

    last_press_time = get_absolute_time();
}

bool debounce_time_passed() {
    return absolute_time_diff_us(last_press_time, get_absolute_time()) > DEBOUNCE_MS * 1000;
}

char pico_keypad_get_key(void) {
    int row;
    uint32_t cols;

    cols = gpio_get_all();
    cols = cols & all_columns_mask;

    if (cols == 0x0) {
        return 0;
    }

    for (int j = 0; j < 4; j++) {
        gpio_put(_rows[j], 0);
    }

    for (row = 0; row < 4; row++) {
        gpio_put(_rows[row], 1);

        busy_wait_us(10000);

        cols = gpio_get_all();
        gpio_put(_rows[row], 0);
        cols = cols & all_columns_mask;
        if (cols != 0x0) {
            break;
        }
    }

    for (int i = 0; i < 4; i++) {
        gpio_put(_rows[i], 1);
    }

    if (cols == column_mask[0]) {
        return (char)_matrix_values[row * 4 + 0];
    } else if (cols == column_mask[1]) {
        return (char)_matrix_values[row * 4 + 1];
    } else if (cols == column_mask[2]) {
        return (char)_matrix_values[row * 4 + 2];
    } else if (cols == column_mask[3]) {
        return (char)_matrix_values[row * 4 + 3];
    } else {
        return 0;
    }
}

void pico_keypad_irq_enable(bool enable, gpio_irq_callback_t callback) {
    for (int i = 0; i < 4; i++) {
        gpio_set_irq_enabled_with_callback(_columns[i], GPIO_IRQ_EDGE_RISE, enable, callback);
    }
}
