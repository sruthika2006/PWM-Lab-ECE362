#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/timer.h"

#define COL_MASK ((1u << 6) | (1u << 7) | (1u << 8) | (1u << 9))

// Global column variable
int col = -1;

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();

/********************************************************* */
// Implement the functions below.

void keypad_init_pins() {
    for (int i = 6; i <= 9; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);   // start LOW
    }

    // Rows: GP2–GP5
    for (int i = 2; i <= 5; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }
}

void keypad_init_timer() {
    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    irq_set_enabled(TIMER0_IRQ_0, true);

    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);
    irq_set_enabled(TIMER0_IRQ_1, true);

    timer_hw->inte |= (1u << 0) | (1u << 1);

    uint32_t now = timer_hw->timerawl;
    timer_hw->alarm[0] = now + 1000000;  // 1s
    timer_hw->alarm[1] = now + 1100000;  // 1.1s
}

void keypad_drive_column() {
    timer_hw->intr = (1u << 0);

    col = (col + 1) & 0x3;

    sio_hw->gpio_clr = COL_MASK;
    sio_hw->gpio_set = (1u << (6 + col));

    // Re-arm alarm
    timer_hw->alarm[0] = timer_hw->timerawl + 25000;
}

uint8_t keypad_read_rows() {
    return (gpio_get_all() >> 2) & 0xF;
}

void keypad_isr() {
    timer_hw->intr = (1u << 1);

    uint8_t rows = keypad_read_rows();

    for (int r = 0; r < 4; r++) {
        int idx = col * 4 + r;

        bool pressed = (rows >> r) & 1;

        if (pressed && !state[idx]) {
            state[idx] = true;
            uint16_t event = (1 << 8) | keymap[idx];
            key_push(event);
        }

        if (!pressed && state[idx]) {
            state[idx] = false;
            uint16_t event = keymap[idx];
            key_push(event);
        }
    }

    timer_hw->alarm[1] = timer_hw->timerawl + 25000;
}
