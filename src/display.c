#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
// 7-segment display message buffer
// Declared as static to limit scope to this file only.
static char msg[8] = {
    0x3F, // seven-segment value of 0
    0x06, // seven-segment value of 1
    0x5B, // seven-segment value of 2
    0x4F, // seven-segment value of 3
    0x66, // seven-segment value of 4
    0x6D, // seven-segment value of 5
    0x7D, // seven-segment value of 6
    0x07, // seven-segment value of 7
};
extern char font[]; // Font mapping for 7-segment display
static int index = 0; // Current index in the message buffer

// We provide you with this function for directly displaying characters.
// This now accounts for the decimal point.
void display_char_print(const char message[]) {
    int dp_found = 0;
    for (int i = 0; i < 8; i++) {
        if (message[i] == '.') {
            // add it to the decimal point bit for prev digit if i > 0
            if (i > 0) {
                msg[i - 1] |= (1 << 7); // Set the decimal point bit
                dp_found = 1; // Mark that we found a decimal point
            }
        }
        else {
            msg[i - dp_found] = font[message[i] & 0xFF];
        }
    }
    if (dp_found) {
        msg[7] = font[message[8] & 0xFF]; // Clear the last character if we found a decimal point
    }
}

/********************************************************* */
// Implement the functions below.


void display_init_pins() {
    for (int i = 10; i <= 20; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
}

void display_isr() {
    timer1_hw->intr = (1u << 0);

    uint32_t out =
        ((uint32_t)index << 8) |
        msg[index];

    gpio_put_masked(0x7FF << 10, out << 10);

    index = (index + 1) & 0x7;

    timer1_hw->alarm[0] = timer1_hw->timerawl + 3000;
}

void display_init_timer() {
    irq_set_exclusive_handler(TIMER1_IRQ_0, display_isr);
    irq_set_enabled(TIMER1_IRQ_0, true);

    timer1_hw->inte |= (1u << 0);
    timer1_hw->alarm[0] = timer1_hw->timerawl + 3000;
}

void display_print(const uint16_t message[]) {
    for (int i = 0; i < 8; i++) {
        uint8_t ascii = message[i] & 0xFF;
        uint8_t pressed = (message[i] >> 8) & 1;

        uint8_t seg = font[ascii];
        if (pressed)
            seg |= 0x80;

        msg[i] = seg;
    }
}