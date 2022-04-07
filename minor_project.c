/*
 * File:   minor_project.c
 * Author: ASHWIN-PC
 * 
 * Created on 26 March 2022, 7:26 PM
 */


#include <xc.h>
#include <avr/io.h>
#include <avr/interrupt.h>
// volatile variables that will change in ISR
volatile unsigned long millis = 0;
volatile unsigned long lastDebounceA1 = 0;
volatile unsigned long lastA1LongPress = 0;
volatile unsigned long lastDebounceA2 = 0;
volatile unsigned long lastDebounceA3 = 0;
volatile unsigned long lastA1A2 = 0;
// millis setpoint for comparison on a later time(hook).
unsigned long lastAlarmTime = 0;
unsigned long twoHzCycle = 0;

unsigned long lastBrightness = 0;
unsigned long lastADC = 0;
unsigned long lastAuto = 0;
unsigned long blinkLastMillis = 0;
unsigned long lastSoftTimer = 0;
//
unsigned alarmOnOFF = 0;
// Holds the mapped ADC values 0-20
unsigned adc = 20;
// Software time variables
unsigned secondSoftTimer = 0;
unsigned minuteSoftTimer = 0;
unsigned hourSoftTimer = 0;
unsigned alarmMinute = 0;
unsigned alarmHour = 0;

enum STATE {
    HHMM, MMSS, alarm
};
enum STATE clockDisplay = HHMM;

enum CLOCKMODE {
    twelveHR, twentyFourHR, alarmSet
};
enum CLOCKMODE clockMode = twelveHR;
enum CLOCKMODE lastClockMode = twelveHR;

enum time_set {
    normalMode, setMode
};
enum time_set timeSet = normalMode;

enum SET {
    HH, MM
};
enum SET set = HH;

// millis counter

ISR(TIMER2_COMPA_vect) {
    millis++;
}

enum button_state {
    pressed, unpressed
};
// button press holders
volatile enum button_state A1A2 = unpressed;
volatile enum button_state A1LongPress = unpressed;
volatile enum button_state A1 = unpressed;
volatile enum button_state A2 = unpressed;
volatile enum button_state A3 = unpressed;

ISR(PCINT1_vect) { // pin change interrupt for button press.
    A1 = (PINC & _BV(1)) ? unpressed : pressed;
    A2 = (PINC & _BV(2)) ? unpressed : pressed;
    A3 = (PINC & _BV(3)) ? unpressed : pressed;
    A1LongPress = (PINC & _BV(1)) ? unpressed : pressed;

    if (A1 == pressed && A2 == pressed) {
        A1A2 = pressed;
        lastA1A2 = millis;
    } else {
        A1A2 = unpressed;
    }
    if (A1LongPress == pressed) {
        lastA1LongPress = millis;
    }
    if (A1 == pressed) {
        lastDebounceA1 = millis;
    }
    if (A2 == pressed) {
        lastDebounceA2 = millis;
    }
    if (A3 == pressed) {
        lastDebounceA3 = millis;
    }
}

void setup() {
    DDRD = 0b10010000; //set pin d4= latch and d7=clk out
    PORTD = 0b00000000; //set all pins to low
    DDRB = 0b00001101; //set pin d8=data out and buzzer connected on pin 10
    PORTB = 0b00001000; //set all pins to low
    DDRC = 0b11110000; //set pin d8=data out

    // Setup ADC
    DIDR0 = 0b00000001;
    ADMUX = 0b01000000;
    ADCSRA = 0b10000110;
    ADCSRB = 0b00000000;

    //Setup button interrupts
    PCICR |= _BV(PCIE1);
    PCMSK1 |= 0b00001110;

    // 500 Hz buzzer sound
    TCCR1A = 0b00010000;
    TCCR1B = 0b00001011;
    OCR1A = 250;
    TIMSK1 = 0b00000000;

    // Setup millies timer 8bit timer 2 prescaler 128..
    TCCR2A = 0b00000010;
    TCCR2B = 0b00000101;
    OCR2A = 125;
    TIMSK2 = 0b00000010;
    /*
        // Read EEPROM DATA
        EEAR = 1; // Address of hour data
        hourSoftTimer = EEDR;
        EECR = 0b00000001;

        EEAR = 2; // Address of minutes data
        minuteSoftTimer = EEDR;
        EECR = 0b00000001;

        EEAR = 3; // Address of seconds data
        secondSoftTimer = EEDR;
        EECR = 0b00000001;

        EEAR = 4; // Address of 12hour/24hour state data
        if (EEDR == 0) {
            clockHR = twelveHR;
        } else {
            clockHR = twelveHR;
        }
        EECR = 0b00000001;

     */



    // Enable global interrupts
    sei();
}


// send the 8 bits of data in segments (top bit first)
// then the 8 bits of data in digits (top bit first)

void sendData(uint8_t segments, uint8_t digits) {
    uint16_t data = segments << 8;
    data |= digits;

    for (int i = 15; i >= 0; i--) {
        ((1 << i) & data) ? (PORTB |= _BV(0)) : (PORTB &= ~_BV(0));

        PORTD |= _BV(7); // high
        PORTD &= ~_BV(7); // low
    }
    PORTD |= _BV(4); // latch high
    PORTD &= ~_BV(4); // latch low
    //write the 16 bits of data using PD4 and PD7 then clock it to the output using PB0
}


const uint8_t SEGMENT_MAP[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0X80, 0X90,
    /* Continuing on for A (10) to F (15) */
    0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E,
    /* Then blank (16), dash (17), Dot(18) */
    0xFF, 0xBF, 0x7F
};

uint8_t segmentMap(uint8_t value) {
    return SEGMENT_MAP[value];
}


char digits[4];

unsigned oneHzDot = 0;
unsigned amPmDot = 0;

void showDigits() {
    for (int i = 0; i < 4; i++) {

        if (oneHzDot && i == 1) {

            sendData(segmentMap(digits[i]) & segmentMap(18), (1 << i));

        } else if (amPmDot && i == 3) {
            sendData(segmentMap(digits[i]) & segmentMap(18), (1 << i));
        } else {
            sendData(segmentMap(digits[i]), (1 << i));
        }
    }
    sendData(segmentMap(16), 0); // blank the display
}

uint16_t readADC() {
    ADCSRA |= 0b01000000;
    while (ADCSRA & (1 << 6));
    return ADC;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void brighness() {
    // read ADC sample every 50ms
    if (millis - lastADC >= 50) {
        adc = map(readADC(), 0, 1023, 0, 20);
        lastADC = millis;
    }
    //Call the show digit function based on ADC mapped readings.
    if (millis - lastBrightness >= adc) {
        if (adc < 20) {
            showDigits();
        }
        lastBrightness = millis;
    }
}
//Long press A1 and A2 buttons for 2 seconds

int twoSecondButtonReadA1A2() {
    if (millis - lastA1A2 > 2000 && A1A2 == pressed) {
        A1A2 = unpressed;
        return 1;
    }
    return 0;
}
//Long press A1 button for 2 seconds

int twoSecondButtonReadA1() {
    if (millis - lastA1LongPress > 2000 && A1LongPress == pressed && A2 == unpressed) {
        A1LongPress = unpressed;
        return 1;
    }
    return 0;
}

// Debounce for button A1 if pressed

int buttonReadA1() {
    if (millis - lastDebounceA1 > 50 && A1 == pressed && A2 == unpressed && A3 == unpressed) {
        A1 = unpressed;
        return 1;
    }
    return 0;
}

// Debounce for button A2 if pressed

int buttonReadA2() {
    if (millis - lastDebounceA2 > 50 && A1 == unpressed && A2 == pressed && A3 == unpressed) {
        A2 = unpressed;
        return 1;
    }
    return 0;
}

// Debounce for button A3 if pressed

int buttonReadA3() {
    if (millis - lastDebounceA3 > 50 && A1 == unpressed && A2 == unpressed && A3 == pressed) {
        A3 = unpressed;
        return 1;
    }
    return 0;
}
// This function will blink the decimal at 1HZ

void blinkOneHz() {
    if (millis - blinkLastMillis >= 500) {
        blinkLastMillis = millis;
        if (!oneHzDot) {
            oneHzDot = 1;
        } else {
            oneHzDot = 0;
        }
    }
}
// This function will blink the D3 led at 1HZ when setting alarm.

void blinkD3OneHz() {
    if (millis - blinkLastMillis >= 500) {
        blinkLastMillis = millis;
        if (PINB & ~_BV(3)) {
            PINB |= _BV(3);
        } else {
            PINB &= ~_BV(3);
        }
    }
}
// this function is used to auto increase/decrease value by 1 every second

unsigned autoInc(unsigned data, int PosNeg) {

    if (millis - lastAuto >= 1000) {
        data += PosNeg;
        lastAuto = millis;
    }
    return data;
}
// This function is to set alarm on and off also D3 led is updated.

void OnOffAlarm() {
    if (buttonReadA2()) {
        if (PINB & ~_BV(3)) {
            PINB |= _BV(3);
            alarmOnOFF = 1;
        } else {
            PINB &= ~_BV(3);
            alarmOnOFF = 0;
        }
    }

}
// This function is to convert 24hour to 12 hour

void show12HR(uint16_t left, uint16_t right) {
    if (left == 0) {
        left = 12;
        amPmDot = 0;
    } else if (left == 12) {
        left = 12;
        amPmDot = 1;
    }
    else if (left > 11) {
        amPmDot = 1;
        left = left - 12;
    } else {
        amPmDot = 0;
    }
    digits[0] = left / 10;
    digits[1] = left % 10;
    digits[2] = right / 10;
    digits[3] = right % 10;
}
// This function displays left and right digits raw in 24 hours.

void showRaw(uint16_t left, uint16_t right) {
    digits[0] = left / 10;
    digits[1] = left % 10;
    digits[2] = right / 10;
    digits[3] = right % 10;
}

// This function is used to set time and minute digits turn off.

void setHH(uint16_t data) {

    if (clockMode == twelveHR ) {

        if (data == 0) {
            data = 12;
            amPmDot = 0;
        } else if (data == 12) {
            data = 12;
            amPmDot = 1;
        }
        else if (data > 11) {
            amPmDot = 1;
            data = data - 12;
        } else {
            amPmDot = 0;
        }
    }
    digits[0] = data / 10;
    digits[1] = data % 10;
    digits[2] = 16;
    digits[3] = 16;

}
// This function is to set minutes and hour digits turn off.

void setMM(uint16_t data) {
    digits[0] = 16;
    digits[1] = 16;
    digits[2] = data / 10;
    digits[3] = data % 10;
}

int main(void) {
    setup(); // set up the physical hardware


    while (1) {
        brighness();
        // increment seconds every 1000 millis
        if (millis - lastSoftTimer >= 1000 && timeSet == normalMode) {
            lastSoftTimer = millis;
            secondSoftTimer++;
        }
        // increment minutes based on seconds and reset seconds
        if (secondSoftTimer >= 60) {
            secondSoftTimer = 0;
            minuteSoftTimer++;
        }
        // increment hours based on minutes and reset minutes
        if (minuteSoftTimer >= 60) {
            if (timeSet == normalMode) {
                hourSoftTimer++;
            }
            minuteSoftTimer = 0;
        }
        // Bounds hours to 24hours max
        if (hourSoftTimer >= 24) {
            hourSoftTimer = 0;
        }
        // This variable is used to set alarm minutes
        if (alarmMinute >= 60) {
            alarmMinute = 0;
        }
        // This variable is to set alarm hours
        if (alarmHour >= 24) {
            alarmHour = 0;
        }
        // This function is used to set alarm time by holding two buttons for two seconds
        if (twoSecondButtonReadA1A2() && clockMode != alarmSet) {
            lastClockMode = clockMode; // save the current mode to later restore.
            clockMode = alarmSet;
        }
        // This is used to change the clock from twelve hour to 24hour
        if (twoSecondButtonReadA1()) {
            timeSet = normalMode;
            if (clockMode == twentyFourHR) {
                clockMode = twelveHR;
            } else {
                clockMode = twentyFourHR;
            }
        }

        // state machine starts here
        switch (clockMode) {
            case twelveHR:
                switch (clockDisplay) {
                    case HHMM:
                        blinkOneHz(); // blink middle decimal point at 1hz
                        switch (timeSet) {
                            case normalMode:
                                show12HR(hourSoftTimer, minuteSoftTimer);
                                OnOffAlarm();
                                if (buttonReadA3()) {
                                    clockDisplay = MMSS;
                                }
                                if (buttonReadA1()) {
                                    timeSet = setMode;
                                }
                                break;
                            case setMode:
                                switch (set) {
                                    case HH:
                                        if (buttonReadA1()) {
                                            set = MM;
                                        }
                                        if (buttonReadA2()) {
                                            hourSoftTimer++; //increment
                                        }
                                        if (buttonReadA3()) {
                                            hourSoftTimer--; //Decrement
                                        }
                                        setHH(hourSoftTimer);

                                        break;
                                    case MM:
                                        if (buttonReadA1()) {
                                            set = HH;
                                            timeSet = normalMode;
                                        }
                                        if (buttonReadA2()) {
                                            minuteSoftTimer++; //increment
                                        }
                                        if (buttonReadA3()) {
                                            minuteSoftTimer--; //Decrement
                                        }
                                        setMM(minuteSoftTimer);
                                        break;
                                }
                                break;
                        }
                        break;
                    case MMSS:
                        //  middle decimal point should be solid on.
                        OnOffAlarm();
                        oneHzDot = 1;
                        if (buttonReadA3()) {
                            clockDisplay = alarm;
                        }
                        showRaw(minuteSoftTimer, secondSoftTimer);
                        break;

                    case alarm:
                        PORTB &= ~_BV(3);
                        amPmDot = 0;
                        oneHzDot = 0;
                        show12HR(alarmHour, alarmMinute);
                        if (buttonReadA3()) {
                            clockDisplay = HHMM;
                            PORTB |= _BV(3);
                        }
                        break;
                }
                break;
            case twentyFourHR:
                switch (clockDisplay) {
                    case HHMM:
                        blinkOneHz(); // blink middle decimal point at 1hz
                        amPmDot = 0;
                        switch (timeSet) {
                            case normalMode:
                                showRaw(hourSoftTimer, minuteSoftTimer);
                                OnOffAlarm();
                                if (buttonReadA3()) {
                                    clockDisplay = MMSS;
                                }
                                if (buttonReadA1()) {
                                    timeSet = setMode;
                                }
                                break;
                            case setMode:
                                switch (set) {
                                    case HH:
                                        if (buttonReadA1()) {
                                            set = MM;
                                        }
                                        if (A2 == pressed) {
                                            hourSoftTimer = autoInc(hourSoftTimer, 1);
                                        }
                                        if (A3 == pressed) {
                                            hourSoftTimer = autoInc(hourSoftTimer, -1);
                                        }
                                        setHH(hourSoftTimer);
                                        break;
                                    case MM:
                                        if (buttonReadA1()) {
                                            set = HH;
                                            timeSet = normalMode;
                                        }
                                        if (A2 == pressed) {
                                            minuteSoftTimer = autoInc(minuteSoftTimer, 1);
                                        }
                                        if (A3 == pressed) {
                                            minuteSoftTimer = autoInc(minuteSoftTimer, -1);
                                        }
                                        setMM(hourSoftTimer);
                                        break;
                                }
                                break;
                        }
                        break;
                    case MMSS:
                        //  middle decimal point should be solid on.
                        OnOffAlarm();
                        oneHzDot = 1;
                        if (buttonReadA3()) {
                            clockDisplay = alarm;
                        }
                        showRaw(minuteSoftTimer, secondSoftTimer);
                        break;
                    case alarm:
                        PORTB &= ~_BV(3);
                        amPmDot = 0;
                        oneHzDot = 0;
                        showRaw(alarmHour, alarmMinute);
                        if (buttonReadA3()) {
                            clockDisplay = HHMM;
                            PORTB |= _BV(3);
                        }
                        break;
                }
                break;
                /*Set alarm */
            case alarmSet:
                blinkD3OneHz();

                switch (set) {
                    case HH:
                        if (buttonReadA1()) {
                            set = MM;
                        }
                        if (A2 == pressed) {
                            alarmHour = autoInc(alarmHour, 1);
                        }
                        if (A3 == pressed) {
                            alarmHour = autoInc(alarmHour, -1);
                        }
                        setHH(alarmHour);
                        break;
                    case MM:
                        if (buttonReadA1()) {
                            set = HH;
                            clockMode = lastClockMode;
                        }
                        if (A2 == pressed) {
                            alarmMinute = autoInc(alarmMinute, 1);
                        }
                        if (A3 == pressed) {
                            alarmMinute = autoInc(alarmMinute, -1);
                        }
                        setMM(alarmMinute);
                        break;
                }
                break;
                break;
                break;
        }

    }
}
