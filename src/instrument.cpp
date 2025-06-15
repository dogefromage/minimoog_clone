#include "instrument.h"

#include <Adafruit_MCP4728.h>

#include "config.h"

Adafruit_MCP4728 mcp;

void init_dac() {
    mcp.begin();
}

// note must be in range [NOTE_OF_LOWEST_KEY, NOTE_OF_LOWEST_KEY + 49] otherwise it will clip
// vel has MIDI scale
void synth_note_on(int note, int velocity) {
    int pitch = (note - NOTE_OF_LOWEST_KEY) * 1000L / 12L;
    if (pitch < 0) {
        pitch = 0;
    } else if (pitch >= 4096) {
        pitch = 4095;
    }

    uint16_t vel_data = (127 - velocity) * 32;

    mcp.setChannelValue(MCP4728_CHANNEL_B, pitch,
                        MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);

    mcp.setChannelValue(MCP4728_CHANNEL_A, vel_data,
                        MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);

    digitalWrite(GATE, HIGH);
}

void synth_note_off() {
    digitalWrite(GATE, LOW);
}
