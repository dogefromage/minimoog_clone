

typedef struct {
    bool isPressed;
    // key is held iff key is in arpeggiator sequence
    bool isHeld;
    bool pad1, pad2;
    long initialPressMillis;
    int velocity;
} keydata_t;

enum player_modes {
    MODE_MIDI,
    MODE_KEYS,
    MODE_ARP_UP,
    MODE_ARP_DOWN,
    MODE_ARP_UP_DOWN,
    MODE_ARP_RANDOM
};

#define NUM_KEYS 49

typedef struct {
    int clockInCounter, externalClockInLast;
    long lastHalfStep;
    bool lastArpStepRising;
    int lastArpKey;
    int arpUpDownDirection;
} arp_t;

typedef struct {
    player_modes mode, lastMode;
    keydata_t keys[NUM_KEYS];
    arp_t arp;

    int midiNotesPlayed;
    unsigned long retriggerMomentMillis;  // instant if 0, millis to wait otherwise
    int lastPB, lastMod;
} player_t;

player_t* get_instance();

void player_init(player_t* player);
void player_update(player_t* player);