#include "instrument.h"

#include <Adafruit_MCP4728.h>

#include "config.h"

Adafruit_MCP4728 mcp;

void init_dac() {
    mcp.begin();
}

// note and vel have MIDI scale
void synth_note_on(int note, int vel) {
    // TODO translate note into good voltage range

    int pitch = int(note * 1000L / 12L);
    int velocity = (127 - vel) * 32;

    mcp.setChannelValue(MCP4728_CHANNEL_B, pitch,
                        MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);

    mcp.setChannelValue(MCP4728_CHANNEL_A, velocity,
                        MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);

    digitalWrite(GATE, HIGH);
}

void synth_note_off() {
    digitalWrite(GATE, LOW);
}
