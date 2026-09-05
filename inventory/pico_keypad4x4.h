#ifndef PICO_KEYPAD4X4_H
#define PICO_KEYPAD4X4_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"

extern uint _columns[4];
extern uint _rows[4];
extern char _matrix_values[16];

extern uint all_columns_mask;
extern uint column_mask[4];
extern absolute_time_t last_press_time;
extern const uint DEBOUNCE_MS;

void pico_keypad_init(uint columns[4], uint rows[4], char matrix_values[16]);
char pico_keypad_get_key(void);
void pico_keypad_irq_enable(bool enable, gpio_irq_callback_t callback);
bool debounce_time_passed(void);

#endif // PICO_KEYPAD4X4_H
