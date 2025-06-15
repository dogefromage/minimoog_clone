#include "player.h"

#include <Arduino.h>
#include <MIDI.h>
#include <SPI.h>
#include <stddef.h>

#include "config.h"
#include "instrument.h"

USE_MIDI(MIDI_CREATE_DEFAULT_INSTANCE());

#undef abs
// the arduino abs is a macro???? not good design choice
template <typename T>
const T& abs(const T& a) {
    return (a > 0) ? a : -a;
}

int calculateKeyVelocity(long dmillis) {
    // VELOCITY FIXPOINTS
    // C(   2 ms,   0 )
    // A(  15 ms,  63 )
    // B(  50 ms, 127 )

    int value;

    if (dmillis > 15) {
        value = (64 * (int)dmillis + 1245) / 35;
    } else {
        value = (63 * (int)dmillis - 126) / 13;
    }

    value = 127 - value;

    if (value > 127) {
        value = 127;
    } else if (value < 0) {
        value = 0;
    }
    return value;
}

void detectKeys(player_t* p, bool retrigger, bool hold) {
    // whether or not to clear the arp bool if key is not pressed
    bool clearHeld = !hold;

    SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));

    for (int j = 0; j < 9; j++) {
        digitalWrite(A, (j >> 0) & 1);
        digitalWrite(B, (j >> 1) & 1);  // SET MULTIPLEXERS
        digitalWrite(C, (j >> 2) & 1);

        bool isNineth = (j == 8);
        int loopLength = isNineth ? 2 : 12;

        for (int i = 0; i < loopLength; i++) {
            SPI.transfer16(1 << i);

            digitalWrite(SR_LATCH, HIGH);
            digitalWrite(SR_LATCH, LOW);

            delayMicroseconds(2);

            int analogPin = isNineth ? KEYBOARD_IN2 : KEYBOARD_IN1;
            bool currentKey = digitalRead(analogPin);

            int keyIndex = j * 6 + i / 2;
            keydata_t* keyData = &p->keys[keyIndex];

            bool isPad1 = i % 2 == 0;
            int note = NOTE_OF_LOWEST_KEY + keyIndex;

            bool lastKey;
            if (isPad1) {
                lastKey = keyData->pad1;
                keyData->pad1 = currentKey;
            } else {
                lastKey = keyData->pad2;
                keyData->pad2 = currentKey;
            }

            if (currentKey && !lastKey) {
                if (isPad1) {
                    keyData->initialPressMillis = millis();
                } else {
                    // Serial.print("key pressed");

                    keyData->isPressed = true;

                    clearHeld = true;  // a new key pressed, arp hold must be cleared if not already

                    long attackTime = millis() - keyData->initialPressMillis;

                    if (retrigger) {
                        p->retriggerMomentMillis = millis() + RETRIGGER_MILLIS;
                        digitalWrite(GATE, LOW);
                    } else {
                        p->retriggerMomentMillis = 0;
                    }

                    int vel = calculateKeyVelocity(attackTime);
                    p->keys[i].velocity = vel;

                    if (p->mode == MODE_KEYS || p->mode == MODE_MIDI) {
                        USE_MIDI(MIDI.sendNoteOn(note, vel, 1));
                    }
                }
            }

            else if (!currentKey && lastKey && i % 2 == 1) {
                keyData->isPressed = false;

                if (p->mode == MODE_KEYS || p->mode == MODE_MIDI) {
                    USE_MIDI(MIDI.sendNoteOff(note, 0, 1));
                }
            }
        }
    }

    // if HOLD then arp sequence (all held keys) only get updated if a new key is pressed (not released)
    // otherwise always copy isPressed to isHeld such that the arp plays the currently pressed keys
    if (clearHeld) {
        for (int i = 0; i < NUM_KEYS; i++) {
            p->keys[i].isHeld = p->keys[i].isPressed;
        }
    }
}

void handleMidiNoteOn(byte channel, byte pitch, byte velocity) {
    player_t* p = get_instance();
    p->midiNotesPlayed++;
    digitalWrite(GATE, HIGH);
    synth_note_on(pitch, velocity);
}

void handleMidiNoteOff(byte channel, byte pitch, byte velocity) {
    player_t* p = get_instance();
    p->midiNotesPlayed--;
    if (p->midiNotesPlayed <= 0) {
        p->midiNotesPlayed = 0;
        synth_note_off();
    }
}

void handleClock() {
    player_t* p = get_instance();
    p->arp.clockInCounter++;
}

player_modes readModeSwitch() {
    int sw = analogRead(MODESWITCH);

    if (sw < 102) {
        return MODE_KEYS;
    } else if (sw < 307) {
        return MODE_MIDI;
    }

    // ARP
    else if (sw < 512) {
        return MODE_ARP_UP;
    } else if (sw < 716) {
        return MODE_ARP_DOWN;
    } else if (sw > 716 && sw < 912) {
        return MODE_ARP_UP_DOWN;
    } else {
        return MODE_ARP_RANDOM;
    }
}

void player_init(player_t* player) {
    USE_MIDI(MIDI.begin(MIDI_CHANNEL_OMNI));
    USE_MIDI(MIDI.setHandleNoteOn(handleMidiNoteOn));
    USE_MIDI(MIDI.setHandleNoteOff(handleMidiNoteOff));
    USE_MIDI(MIDI.setHandleClock(handleClock));

    memset(player, 0, sizeof(player_t));

    player->arp.lastArpKey = -1;
    player->arp.arpUpDownDirection = 1;
}

int calcArpPeriod() {
    int rate = analogRead(RATE);
    // arp rate keyframes
    // ( 0 / 2000 )
    // ( 500 / 200 )
    // ( 1024 / 10 )
    if (rate > 500) {
        return (int)((-95L * rate + 99900L) / 262L);
    } else {
        return (int)((-18L * rate + 10000L) / 5L);
    }
}

int findLastArpBufferIndex(player_modes mode, int* arpKeys, int arpKeyCount, int lastArpKey) {
    // if last arp key is present in key sequence then choose its index
    for (int i = 0; i < arpKeyCount; i++) {
        if (arpKeys[i] == lastArpKey) {
            return i;
        }
    }

    // TODO improve this by acknoledging the direction of the arp
    return 0;  // dummy key
}

void performArpStepRising(player_t* p) {
    arp_t* arp = &p->arp;

    int arpKeys[MAX_ARP_KEYS];
    int arpKeyCount = 0;

    // collect notes
    for (int i = 0; i < NUM_KEYS; i++) {
        if (p->keys[i].isHeld) {
            arpKeys[arpKeyCount++] = i;
        }
        if (arpKeyCount == MAX_ARP_KEYS) {
            break;
        }
    }

    if (arpKeyCount == 0) {
        return;  // do nothing
    }

    // start from the last buffer index
    int bufferIndex = findLastArpBufferIndex(p->mode, arpKeys, arpKeyCount, arp->lastArpKey);
    // move to next key
    if (p->mode == MODE_ARP_UP) {
        bufferIndex++;
    } else if (p->mode == MODE_ARP_DOWN) {
        bufferIndex--;
    } else if (p->mode == MODE_ARP_UP_DOWN) {
        bufferIndex += arp->arpUpDownDirection;
        if (bufferIndex == 0 || bufferIndex == arpKeyCount - 1) {
            arp->arpUpDownDirection = -arp->arpUpDownDirection;
        }
    } else {
        // random
        bufferIndex = random(arpKeyCount);
    }

    // wrap around
    while (bufferIndex < 0) {
        bufferIndex += arpKeyCount;
    }
    bufferIndex %= arpKeyCount;

    int key = arpKeys[bufferIndex];
    synth_note_on(NOTE_OF_LOWEST_KEY + key, 127);  // full velocity
    arp->lastArpKey = key;
}

void player_update(player_t* p) {
    p->mode = readModeSwitch();

    bool retrigger = digitalRead(RETRIGGER);
    bool hold = !digitalRead(HOLD);

    detectKeys(p, retrigger, hold);

    if (p->mode != p->lastMode) {
        // changed mode

        if (p->lastMode >= MODE_ARP_UP) {
            // arp deselected
            int lastArpNote = NOTE_OF_LOWEST_KEY + p->arp.lastArpKey;
            USE_MIDI(MIDI.sendNoteOff(lastArpNote, 0, 1));
        }
        if (p->lastMode == MODE_MIDI) {
            // midi deselected
            p->midiNotesPlayed = 0;
            digitalWrite(GATE, LOW);
        } else if (p->lastMode == MODE_KEYS) {
            // normal play deselected
            for (int i = 0; i < NUM_KEYS; i++) {
                if (p->keys[i].isPressed) {
                    USE_MIDI(MIDI.sendNoteOff(NOTE_OF_LOWEST_KEY + i, 0, 1));
                }
            }
            digitalWrite(GATE, LOW);
        }
    }

    if (p->mode == MODE_KEYS) {
        // find highest note
        int highestKey = -1;

        for (int i = NUM_KEYS - 1; i >= 0; i--) {
            if (p->keys[i].isPressed) {
                highestKey = i;
                break;
            }
        }

        if (highestKey >= 0 && p->retriggerMomentMillis < millis()) {
            int note = NOTE_OF_LOWEST_KEY + highestKey;
            int vel = p->keys[highestKey].initialPressMillis;
            synth_note_on(note, vel);
        } else {
            synth_note_off();
        }
    }

    else if (p->mode == MODE_MIDI) {
        // MIDI IN
        USE_MIDI(MIDI.read());

        if (!digitalRead(HOLD)) {
            // panic switch
            p->midiNotesPlayed = 0;
            digitalWrite(GATE, LOW);
        }
    }

    else {
        // arp modes
        arp_t* arp = &p->arp;

        // external jack clock
        bool externalClockRead = digitalRead(CLKIN);
        if (!arp->externalClockInLast && externalClockRead) {
            arp->clockInCounter++;
        }
        arp->externalClockInLast = externalClockRead;

        unsigned long halfPeriod = calcArpPeriod();
        if (arp->lastHalfStep + halfPeriod < millis() || arp->clockInCounter >= 24) {
            if (arp->lastArpStepRising) {
                synth_note_off();
            } else {
                performArpStepRising(p);
            }

            arp->lastHalfStep = millis();
            arp->clockInCounter = 0;
            arp->lastArpStepRising = !arp->lastArpStepRising;
        }
    }

    if (p->mode != MODE_MIDI) {
        // PITCHBEND
        int pbRead = analogRead(PITCHBEND);
        if (abs(p->lastPB - pbRead) > 1) {
            p->lastPB = pbRead;
            int value = (pbRead - 512) * 16;
            USE_MIDI(MIDI.sendPitchBend(value, 1));
        }

        // MODULATION
        int modRead = analogRead(MODULATION);
        if (abs(p->lastMod - modRead) > 1) {
            p->lastMod = modRead;
            USE_MIDI(MIDI.sendControlChange(1, modRead / 8, 1));
        }
    }

    p->lastMode = p->mode;
}