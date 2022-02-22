
#include <xc.h>

void delay_ms(uint16_t num) {
    while (num--) {
        for (volatile long x = 0; x < 468; x++) {
            ;
        }
    }
}

void setup() {
    DDRB  |= 0b00111000;    // PORTB pin 5 (D13) output
    DDRC  &= 0b11111011;    // PORTC pin 1 (A1) input
    PORTB |= 0b00111100;    // turn off LEDs
    PORTC |= 0b00001110;    // turn on internal pullup for Port C pins
}

int main(void) {
    setup();    // set up the physical hardware
    uint16_t delay = 1000;
    while (1) {
        while ((PINC & 0b00000010) == 0) {
            // busy_loop until button released
            ;
        }
        
        PORTB ^= 0b00100000;    // invert LED

        delay_ms(delay);
    }
}