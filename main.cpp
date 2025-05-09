#include "mbed.h"

// Use chrono time literals (1s, 1ms)
using namespace chrono;

// Pin definitions for Arduino Multifunction Shield (assumed standard mapping)
DigitalOut segLatch(PB_5);   // Latch pin for 74HC595
DigitalOut segClock(PA_8);   // Clock pin
DigitalOut segData(PA_9);    // Data pin

InterruptIn button1(PA_1);   // S1 - Reset button
InterruptIn button3(PB_0);   // S3 - Show voltage

AnalogIn pot(PA_0);          // Onboard potentiometer

// Segment patterns for digits 0-9 (common cathode)
const uint8_t SEGMENT_MAP[10] = {
    0xC0, 0xF9, 0xA4, 0xB0,
    0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90
};

// Digit select (active low): 4 digits
const uint8_t SELECT_MAP[4] = {
    0xF1, 0xF2, 0xF4, 0xF8
};

// Global variables
volatile int secondsCount = 0;
volatile bool showVoltage = false;

Ticker timerTicker;

// ISR: increment seconds
void tick() {
    secondsCount++;
}

// ISR: reset timer
void onResetPressed() {
    secondsCount = 0;
}

// ISR: S3 pressed → show voltage
void onS3Pressed() {
    showVoltage = true;
}

// ISR: S3 released → return to time
void onS3Released() {
    showVoltage = false;
}

// Shift out one byte MSB-first to the shift register
void shiftOutByte(uint8_t val) {
    for (int bit = 7; bit >= 0; bit--) {
        segClock = 0;
        segData = (val >> bit) & 0x1;
        segClock = 1;
    }
}

int main() {
    // Configure buttons with pull-up resistors
    button1.mode(PullUp);
    button3.mode(PullUp);
    button1.fall(&onResetPressed);
    button3.fall(&onS3Pressed);
    button3.rise(&onS3Released);

    // Start ticker (1s interval)
    timerTicker.attach(&tick, 1s);

    while (true) {
        int d0, d1, d2, d3;
        bool dp0 = false;
        bool dp2 = false;

        if (showVoltage) {
            // Read voltage from potentiometer
            float volt = pot.read() * 3.3f;
            int val = static_cast<int>(volt * 100 + 0.5f);  // e.g., 3.30V → 330

            if (val > 330) val = 330;
            d0 = val / 100;
            int dec = val % 100;
            d1 = dec / 10;
            d2 = dec % 10;
            d3 = 10; // blank
            dp0 = true;  // Show decimal after first digit (e.g., 3.30 → "3.30")
        } else {
            // Display MM.SS (time mode)
            int total = secondsCount % 6000;  // wrap after 99:59
            int minutes = total / 60;
            int seconds = total % 60;

            d0 = (minutes / 10) % 10;
            d1 = minutes % 10;
            d2 = seconds / 10;
            d3 = seconds % 10;

            dp2 = true;  // Show decimal point between MM and SS
        }

        // Display update (multiplexing all 4 digits)
        for (int pos = 0; pos < 4; pos++) {
            uint8_t segByte;
    if (pos == 0) segByte = (d0 <= 9 ? SEGMENT_MAP[d0] : 0xFF) & (dp0 ? 0x7F : 0xFF);
    if (pos == 1) segByte = (d1 <= 9 ? (SEGMENT_MAP[d1] & (dp2 ? 0x7F : 0xFF)) : 0xFF);  // DP between MM and SS
    if (pos == 2) segByte = (d2 <= 9 ? SEGMENT_MAP[d2] : 0xFF);
    if (pos == 3) segByte = (d3 <= 9 ? SEGMENT_MAP[d3] : 0xFF);

            segLatch = 0;
            shiftOutByte(segByte);
            shiftOutByte(SELECT_MAP[pos]);
            segLatch = 1;

            ThisThread::sleep_for(1ms);  // Allow digit to appear briefly
        }
    }
}
