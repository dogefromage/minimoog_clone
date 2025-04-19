#include <Arduino.h>

// SHIFT REGISTERS
#define SR_LATCH 10
#define MOSI 11
#define SCK 13

// MULTIPLEXER
#define A 7
#define B 8
#define C 9

// MULTIPLEXED IN
#define KEYBOARD_IN1 6
#define KEYBOARD_IN2 5

#define RETRIGGER A0
#define PITCHBEND A2
#define MODULATION A3
#define RATE A6
#define MODESWITCH A7
#define HOLD 4

#define GATE 2
#define CLKOUT 3
#define CLKIN A1

#define NOTE_OF_LOWEST_KEY 24

// how long the gate goes low to retrigger a note
#define RETRIGGER_MILLIS 1

// buffer size for keeping arp sequence
#define MAX_ARP_KEYS 32
