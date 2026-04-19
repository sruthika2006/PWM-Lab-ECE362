#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "queue.h"
#include "support.h"

//////////////////////////////////////////////////////////////////////////////

const char* username = "shivaks";

//////////////////////////////////////////////////////////////////////////////

static int duty_cycle = 0;
static int dir = 0;
static int color = 0;

void display_init_pins();
void display_init_timer();
void display_char_print(const char message[]);
void keypad_init_pins();
void keypad_init_timer();
void init_wavetable(void);
void set_freq(int chan, float f);
extern KeyEvents kev;
void drum_machine();

//////////////////////////////////////////////////////////////////////////////

// When testing static duty-cycle PWM
#define STEP2
// When testing variable duty-cycle PWM
// #define STEP3
// When testing 8-bit audio synthesis
// #define STEP4
// When trying out drum machine
// #define DRUM_MACHINE

//////////////////////////////////////////////////////////////////////////////

void init_pwm_static(uint32_t period, uint32_t duty_cycle) {
    gpio_set_function(37, GPIO_FUNC_PWM);
    gpio_set_function(38, GPIO_FUNC_PWM);
    gpio_set_function(39, GPIO_FUNC_PWM);

    uint s37 = pwm_gpio_to_slice_num(37);
    uint s38 = pwm_gpio_to_slice_num(38);
    uint s39 = pwm_gpio_to_slice_num(39);

    pwm_set_clkdiv_int_frac(s37, 150, 0);
    pwm_set_clkdiv_int_frac(s38, 150, 0);
    pwm_set_clkdiv_int_frac(s39, 150, 0);

    pwm_set_wrap(s37, period - 1);
    pwm_set_wrap(s38, period - 1);
    pwm_set_wrap(s39, period - 1);

    // Common anode: level=0 fully ON, level=period fully OFF
    uint32_t level = period - duty_cycle;
    pwm_set_chan_level(s37, pwm_gpio_to_channel(37), level);
    pwm_set_chan_level(s38, pwm_gpio_to_channel(38), level);
    pwm_set_chan_level(s39, pwm_gpio_to_channel(39), level);

    pwm_set_enabled(s37, true);
    pwm_set_enabled(s38, true);
    pwm_set_enabled(s39, true);
}

static struct repeating_timer breathing_timer;

static bool pwm_breathing_callback(struct repeating_timer *t) {
    if (dir == 0)
        duty_cycle += 1;
    else
        duty_cycle -= 1;

    if (duty_cycle >= 100) {
        duty_cycle = 100;
        dir = 1;
    } else if (duty_cycle <= 0) {
        duty_cycle = 0;
        dir = 0;
        color = (color + 1) % 3;
    }

    uint s37 = pwm_gpio_to_slice_num(37);
    uint s38 = pwm_gpio_to_slice_num(38);
    uint s39 = pwm_gpio_to_slice_num(39);
    uint32_t p = pwm_hw->slice[s37].top + 1;
    uint32_t level = p - ((uint32_t)duty_cycle * p / 100);
    uint32_t off = p;

    pwm_set_chan_level(s37, pwm_gpio_to_channel(37), color == 0 ? level : off);
    pwm_set_chan_level(s38, pwm_gpio_to_channel(38), color == 1 ? level : off);
    pwm_set_chan_level(s39, pwm_gpio_to_channel(39), color == 2 ? level : off);
    return true;
}

void pwm_breathing() {
    // unused - timer handles animation
}

void init_pwm_irq() {
    duty_cycle = 0;
    dir = 0;
    color = 0;

    uint s37 = pwm_gpio_to_slice_num(37);
    uint s38 = pwm_gpio_to_slice_num(38);
    uint s39 = pwm_gpio_to_slice_num(39);
    uint32_t p = pwm_hw->slice[s37].top + 1;
    uint32_t off = p;

    pwm_set_chan_level(s37, pwm_gpio_to_channel(37), off);
    pwm_set_chan_level(s38, pwm_gpio_to_channel(38), off);
    pwm_set_chan_level(s39, pwm_gpio_to_channel(39), off);

    add_repeating_timer_ms(-10, pwm_breathing_callback, NULL, &breathing_timer);
}

void pwm_audio_handler() {
    uint s36 = pwm_gpio_to_slice_num(36);
    pwm_clear_irq(s36);

    offset0 += step0;
    offset1 += step1;

    if (offset0 >= N << 16) offset0 -= N << 16;
    if (offset1 >= N << 16) offset1 -= N << 16;

    int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];
    samp /= 2;

    uint32_t period = pwm_hw->slice[s36].top + 1;
    samp = samp * (int)period / (1 << 16);

    pwm_set_chan_level(s36, pwm_gpio_to_channel(36), (uint16_t)samp);
}

void init_pwm_audio() {
    gpio_set_function(36, GPIO_FUNC_PWM);

    uint s36 = pwm_gpio_to_slice_num(36);

    pwm_set_clkdiv_int_frac(s36, 150, 0);
    pwm_set_wrap(s36, (1000000 / RATE) - 1);
    pwm_set_chan_level(s36, pwm_gpio_to_channel(36), 0);

    init_wavetable();

    pwm_clear_irq(s36);
    pwm_set_irq_enabled(s36, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    pwm_set_enabled(s36, true);
}

//////////////////////////////////////////////////////////////////////////////

int main()
{
    // Configures our microcontroller to 
    // communicate over UART through the TX/RX pins
    stdio_init_all();

    // Uncomment when you need to run autotest.
    // Keep this commented out until you need it
    // since it adds a lot of time to the upload process.
    autotest();

    // Make sure to copy in the latest display.c and keypad.c from your previous labs.
    keypad_init_pins();
    keypad_init_timer();
    display_init_pins();
    display_init_timer();

    /*
    *******************************************************
    * Make sure to go through the code in the steps below.  
    * A lot of it can be very useful for your projects.
    *******************************************************
    */

    #ifdef STEP2
    init_pwm_static(100, 50); // Start out with 500/1000, 50%
    display_char_print("      50");
    uint16_t percent = 50; // Set initial percentage for duty cycle, displayed 
    uint16_t disp_buffer = 0;
    char buf[9];

    // Display initial duty cycle
    snprintf(buf, sizeof(buf), "      50");
    display_char_print(buf);

    bool new_entry = true;  // Flag to track if we're starting a new entry
    
    for (;;) {
        uint16_t keyevent = key_pop(); // Pop a key event from the queue
        if (keyevent & 0x100) {
            char key = keyevent & 0xFF;
            if (key >= '0' && key <= '9') {
                // If the key is a digit, check if we need to clear the buffer first
                if (new_entry) {
                    disp_buffer = 0;  // Clear the buffer for new entry
                    new_entry = false;  // No longer a new entry
                }
                // Shift into buffer
                disp_buffer = (disp_buffer * 10) + (key - '0');
                snprintf(buf, sizeof(buf), "%8d", disp_buffer);
                display_char_print(buf); // Display the new value
            } else if (key == '#') {
                // If the key is '#', set the duty cycle
                percent = disp_buffer;
                if (percent > 100) {
                    percent = 100; // Cap at 100%
                }
                init_pwm_static(100, percent); // Update PWM with new duty cycle
                snprintf(buf, sizeof(buf), "%8d", percent);
                display_char_print(buf); // Display the new duty cycle
                new_entry = true;  // Ready for new entry
            }
            else if (key == '*') {
                // If the key is '*', reset the buffer
                disp_buffer = 50;
                percent = 50;
                init_pwm_static(100, percent); // Reset PWM to 50% duty cycle
                snprintf(buf, sizeof(buf), "      50");
                display_char_print(buf); // Display reset
                new_entry = true;  // Ready for new entry
            }
            else {
                // Any other key also starts a new entry
                new_entry = true;
            }
        }
    }
    #endif

    #ifdef STEP3
    init_pwm_static(10000, 5000); // Start out with 500/1000, 50%
    init_pwm_irq(); // Initialize PWM IRQ for variable duty cycle

    for(;;) {
        // The handler manages everything from now on.
        // Use the CPU to do something else!
        tight_loop_contents();
    }
    #endif
    
    #ifdef STEP4
    char freq_buf[9] = {0};
    int pos = 0;
    bool decimal_entered = false;
    int decimal_pos = 0;
    int current_channel = 0;

    keypad_init_pins();
    keypad_init_timer();
    display_init_pins();
    display_init_timer();

    init_pwm_audio(); 

    // set_freq(0, 440.0f); // Set initial frequency to 440 Hz (A4 note)
    // set_freq(1, 0.0f); // Turn off channel 1 initially
    // set_freq(0, 261.626f);
    // set_freq(1, 329.628f);

    set_freq(0, 440.0f); // Set initial frequency for channel 0
    display_char_print(" 440.000 ");

    for(;;) {
        uint16_t keyevent = key_pop();

        if (keyevent & 0x100) {
            char key = keyevent & 0xFF;
            if (key == 'A') {
                current_channel = 0;
                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
                decimal_pos = 0;
                display_char_print("         ");
            } else if (key == 'B') {
                current_channel = 1;
                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
                decimal_pos = 0;
                display_char_print("         ");
            } else if (key >= '0' && key <= '9') {
                if (pos == 0) {
                    snprintf(freq_buf, sizeof(freq_buf), "        "); // Clear buffer on first digit
                    display_char_print(freq_buf);
                }
                if (pos < 8) {
                    freq_buf[pos++] = key;
                    freq_buf[pos] = '\0';
                    display_char_print(freq_buf);
                    if (decimal_entered) decimal_pos++;
                }
                } else if (key == '*') {
                if (!decimal_entered && pos < 7) {
                    freq_buf[pos++] = '.';
                    freq_buf[pos] = '\0';
                    display_char_print(freq_buf);
                    decimal_entered = true;
                    decimal_pos = 0;
                }
                } else if (key == '#') {
                float freq = 0.0f;
                if (decimal_entered) {
                    freq = strtof(freq_buf, NULL);
                } else {
                    freq = (float)atoi(freq_buf);
                }
                set_freq(current_channel, freq);
                snprintf(freq_buf, sizeof(freq_buf), "%8.3f", freq);
                display_char_print(freq_buf);
                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
                decimal_pos = 0;
            } else {
                // Reset on any other key
                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
                decimal_pos = 0;
                display_char_print("        ");
            }
        }
    }
    #endif

    #ifdef DRUM_MACHINE
        drum_machine();
    #endif

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }

    for(;;);
    return 0;
}
