#include <Arduino.h>
#include <SPI.h>

#include "config.h"
#include "instrument.h"
#include "player.h"

player_t playerInstance;
player_t* get_instance() {
    return &playerInstance;
}

void setup() {
    // Serial.begin(9600);
    // while (!Serial) {
    //     // wait for connection
    // }
    // Serial.println("Connected!");

    pinMode(MOSI, OUTPUT);
    pinMode(SCK, OUTPUT);
    pinMode(SR_LATCH, OUTPUT);

    pinMode(A, OUTPUT);
    pinMode(B, OUTPUT);
    pinMode(C, OUTPUT);

    pinMode(HOLD, INPUT_PULLUP);
    pinMode(RETRIGGER, INPUT_PULLUP);

    pinMode(GATE, OUTPUT);
    pinMode(CLKOUT, OUTPUT);

    pinMode(KEYBOARD_IN1, INPUT);
    pinMode(KEYBOARD_IN2, INPUT);

    digitalWrite(SR_LATCH, LOW);
    digitalWrite(GATE, LOW);
    digitalWrite(CLKOUT, LOW);

    SPI.begin();

    init_dac();
    player_init(&playerInstance);
}

void loop() {
    // Serial.println("loop");
    player_update(&playerInstance);
}
