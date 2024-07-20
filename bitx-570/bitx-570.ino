#include <Wire.h>
#include si570.h

// Pin Definitions
#define ENC_A (8)
#define ENC_B (7)
#define ENC_SWITCH (6)

#define VFO_SWAP_PIN (9) // Button for switching VFO A/B

#define ENC_RIT_A (10) // RIT encoder A pin
#define ENC_RIT_B (11) // RIT encoder B pin
#define RIT_BUTTON_PIN (12) // RIT enable/disable button

#define STEP_CHANGE_PIN (13) // Button for changing frequency step

#define ENC_MEM_A (2) // Encoder A pin for memory selection
#define ENC_MEM_B (3) // Encoder B pin for memory selection
#define MEM_BUTTON_PIN (4) // Button to save/recall memory
#define MEM_TO_VFO_PIN (15) // Button to copy memory to VFO

#define SPEAKER_PIN (5) // Pin for speaker output
#define MIC_PIN (6) // Pin for microphone input
#define SQUELCH_PIN (14) // Pin for squelch control

#define CW_KEY_PIN (16) // Pin for CW key input

// Mode Definitions
#define MODE_USB 0
#define MODE_LSB 1
#define MODE_CW 2

// Constants
#define MEMORY_SLOTS 20
#define SQUELCH_THRESHOLD 100 // Adjust this value as needed

const int32_t if_frequency = 26994100;
const int32_t BAND_LIMIT_LOW = 144000000;
const int32_t BAND_LIMIT_HIGH = 144310000;

// Frequency Variables
int32_t current_frequency_A = 144200000; // VFO A frequency in Hz
int32_t current_frequency_B = 144250000; // VFO B frequency in Hz
int32_t* current_frequency = &current_frequency_A; // Pointer to current VFO
int8_t current_mode = MODE_USB;
bool vfo_a_active = true; // Track active VFO

// Memory Variables
int32_t memories[MEMORY_SLOTS];
int current_memory_index = 0; // Index to track the current memory selection

// Encoder Movement Variables
int enc_movement = 0;
int mem_enc_movement = 0;
bool rit_enabled = false;
float rit_offset = 0.0;
int rit_enc_movement = 0;

const int32_t frequency_steps[] = {1000, 10000, 100000, 1000000, 10000000}; // Frequency steps in Hz
int current_step_index = 0; // Index to track current frequency step

bool cw_enabled = false;
int cw_key_pin = CW_KEY_PIN; // Pin for CW key input
int cw_speed = 20; // CW speed in WPM (Words Per Minute)

// Function Prototypes
uint8_t enc_state(void);
uint8_t rit_enc_state(void);
uint8_t mem_enc_state(void);
void encoder_isr();
void rit_encoder_isr();
void mem_encoder_isr();
void radio_tune(int32_t frequency);
int32_t do_tuning();
void switch_vfo();
void toggle_rit();
void change_step();
void memory_button_isr();
void save_memory();
void recall_memory();
void copyMemoryToVFO(); // Prototype for the new function
void copy_memory_button_isr(); // Prototype for the ISR
void update_rit();
void display_init();
void si570_init();
void print_line1(const char* line);
void si570_tune(int32_t lo);
void setup_audio();
void handle_audio();
void handle_squelch();
int read_squelch();
void setup_cw();
void handle_cw();
void toggle_cw();

// Implement the squelch control functions
int read_squelch() {
    return analogRead(SQUELCH_PIN);
}

void handle_squelch() {
    int squelch_level = read_squelch();
    if (squelch_level > SQUELCH_THRESHOLD) {
        // Mute the audio output if squelch level is above the threshold
        analogWrite(SPEAKER_PIN, 0);
    } else {
        // Pass through the audio signal if squelch level is below the threshold
        int audio_signal = analogRead(MIC_PIN);
        analogWrite(SPEAKER_PIN, audio_signal);
    }
}

uint8_t enc_state(void) {
    uint8_t state = digitalRead(ENC_A) | (digitalRead(ENC_B) << 1);
    return state;
}

uint8_t rit_enc_state(void) {
    uint8_t state = digitalRead(ENC_RIT_A) | (digitalRead(ENC_RIT_B) << 1);
    return state;
}

uint8_t mem_enc_state(void) {
    uint8_t state = digitalRead(ENC_MEM_A) | (digitalRead(ENC_MEM_B) << 1);
    return state;
}

void encoder_isr() {
    static uint8_t prev_enc_state = 0;
    uint8_t enc_state_now = enc_state();
    if (enc_state_now == prev_enc_state) return;

    if ((prev_enc_state == 2 && enc_state_now == 3) ||
        (prev_enc_state == 3 && enc_state_now == 1) ||
        (prev_enc_state == 1 && enc_state_now == 0) ||
        (prev_enc_state == 0 && enc_state_now == 2)) {
        enc_movement++;
    } else {
        enc_movement--;
    }
    prev_enc_state = enc_state_now;
}

void rit_encoder_isr() {
    static uint8_t prev_rit_enc_state = 0;
    uint8_t rit_enc_state_now = rit_enc_state();
    if (rit_enc_state_now == prev_rit_enc_state) return;

    if ((prev_rit_enc_state == 2 && rit_enc_state_now == 3) ||
        (prev_rit_enc_state == 3 && rit_enc_state_now == 1) ||
        (prev_rit_enc_state == 1 && rit_enc_state_now == 0) ||
        (prev_rit_enc_state == 0 && rit_enc_state_now == 2)) {
        rit_enc_movement++;
    } else {
        rit_enc_movement--;
    }
    prev_rit_enc_state = rit_enc_state_now;
}

void mem_encoder_isr() {
    static uint8_t prev_mem_enc_state = 0;
    uint8_t mem_enc_state_now = mem_enc_state();
    if (mem_enc_state_now == prev_mem_enc_state) return;

    if ((prev_mem_enc_state == 2 && mem_enc_state_now == 3) ||
        (prev_mem_enc_state == 3 && mem_enc_state_now == 1) ||
        (prev_mem_enc_state == 1 && mem_enc_state_now == 0) ||
        (prev_mem_enc_state == 0 && mem_enc_state_now == 2)) {
        mem_enc_movement++;
    } else {
        mem_enc_movement--;
    }
    prev_mem_enc_state = mem_enc_state_now;
}

void radio_tune(int32_t frequency) {
    int32_t lo;
    if (current_mode == MODE_LSB) {
        lo = frequency < if_frequency ? if_frequency - frequency : frequency - if_frequency;
    } else {
        lo = frequency + if_frequency;
    }
    si570_tune(lo); // Assuming si570_tune is defined elsewhere
}

int32_t do_tuning() {
    if (enc_movement == 0) return 0;
    *current_frequency += frequency_steps[current_step_index] * enc_movement;
    enc_movement = 0;

    // Ensure the frequency stays within the 2M band limits
    if (*current_frequency < BAND_LIMIT_LOW) {
        *current_frequency = BAND_LIMIT_LOW;
    } else if (*current_frequency > BAND_LIMIT_HIGH) {
        *current_frequency = BAND_LIMIT_HIGH;
    }

    radio_tune(*current_frequency);
    return *current_frequency;
}

void update_rit() {
    if (!rit_enabled) return;

    if (rit_enc_movement != 0) {
        rit_offset += 0.1 * rit_enc_movement;
        rit_enc_movement = 0;

        // Limit RIT offset between -9.9 and +9.9 kHz
        if (rit_offset < -9.9) rit_offset = -9.9;
        if (rit_offset > 9.9) rit_offset = 9.9;
    }
}

void switch_vfo() {
    if (vfo_a_active) {
        current_frequency = &current_frequency_B;
        vfo_a_active = false;
    } else {
        current_frequency = &current_frequency_A;
        vfo_a_active = true;
    }
    radio_tune(*current_frequency);
    char buff[100];
    sprintf(buff, "VFO %c: %d", vfo_a_active ? 'A' : 'B', *current_frequency);
    print_line1(buff); // Assuming print_line1 is defined elsewhere
}

void toggle_rit() {
    rit_enabled = !rit_enabled;
    if (!rit_enabled) {
        rit_offset = 0.0; // Reset RIT offset when disabled
    }
}

void change_step() {
    current_step_index = (current_step_index + 1) % 5;
    char buff[50];
    sprintf(buff, "Step: %d Hz", frequency_steps[current_step_index]);
    print_line1(buff);
}

void save_memory() {
    if (current_memory_index < 0 || current_memory_index >= MEMORY_SLOTS) return;
    memories[current_memory_index] = *current_frequency;
    char buff[50];
    sprintf(buff, "Memory %d Saved: %d", current_memory_index, *current_frequency);
    print_line1(buff);
}

void recall_memory() {
    if (current_memory_index < 0 || current_memory_index >= MEMORY_SLOTS) return;
    *current_frequency = memories[current_memory_index];
    radio_tune(*current_frequency);
    char buff[50];
    sprintf(buff, "Memory %d Recalled: %d", current_memory_index, *current_frequency);
    print_line1(buff);
}

void copyMemoryToVFO() {
    if (current_memory_index < 0 || current_memory_index >= MEMORY_SLOTS) return;
    int32_t memory_frequency = memories[current_memory_index];
    
    if (vfo_a_active) {
        current_frequency_A = memory_frequency;
        current_frequency = &current_frequency_A;
    } else {
        current_frequency_B = memory_frequency;
        current_frequency = &current_frequency_B;
    }
    radio_tune(*current_frequency);
    char buff[50];
    sprintf(buff, "Memory %d Copied to VFO %c: %d", current_memory_index, vfo_a_active ? 'A' : 'B', *current_frequency);
    print_line1(buff);
}

// ISR for copy memory to VFO button
void copy_memory_button_isr() {
    copyMemoryToVFO();
}

// Audio setup function
void setup_audio() {
    pinMode(SPEAKER_PIN, OUTPUT);
    pinMode(MIC_PIN, INPUT);
    pinMode(SQUELCH_PIN, INPUT); // Initialize squelch pin
}

// Audio handling function
void handle_audio() {
    handle_squelch(); // Integrate squelch control
}

void setup_cw() {
    pinMode(cw_key_pin, INPUT_PULLUP); // Set CW key pin as input with pull-up resistor
}

void handle_cw() {
    if (cw_enabled) {
        bool key_state = !digitalRead(cw_key_pin); // Read CW key pin (inverted logic)
        if (key_state) {
            // Transmit CW signal
            analogWrite(SPEAKER_PIN, 255); // Simulate CW transmission by setting speaker pin high
        } else {
            analogWrite(SPEAKER_PIN, 0); // End CW transmission by setting speaker pin low
        }
    }
}

void toggle_cw() {
    cw_enabled = !cw_enabled;
    char buff[50];
    sprintf(buff, "CW Mode: %s", cw_enabled ? "ON" : "OFF");
    print_line1(buff);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Pin Setup
    pinMode(ENC_SWITCH, INPUT_PULLUP);
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    pinMode(VFO_SWAP_PIN, INPUT_PULLUP);
    pinMode(ENC_RIT_A, INPUT_PULLUP);
    pinMode(ENC_RIT_B, INPUT_PULLUP);
    pinMode(RIT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(STEP_CHANGE_PIN, INPUT_PULLUP);
    pinMode(ENC_MEM_A, INPUT_PULLUP);
    pinMode(ENC_MEM_B, INPUT_PULLUP);
    pinMode(MEM_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MEM_TO_VFO_PIN, INPUT_PULLUP); // Initialize copy memory button pin

    // Initialize display and SI570
    display_init();
    si570_init();

    // Audio setup
    setup_audio();

    // CW setup
    setup_cw();

    // Interrupts
    attachInterrupt(digitalPinToInterrupt(ENC_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(VFO_SWAP_PIN), switch_vfo, FALLING);
    attachInterrupt(digitalPinToInterrupt(ENC_RIT_A), rit_encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_RIT_B), rit_encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIT_BUTTON_PIN), toggle_rit, FALLING);
    attachInterrupt(digitalPinToInterrupt(STEP_CHANGE_PIN), change_step, FALLING);
    attachInterrupt(digitalPinToInterrupt(ENC_MEM_A), mem_encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_MEM_B), mem_encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(MEM_BUTTON_PIN), memory_button_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(MEM_TO_VFO_PIN), copy_memory_button_isr, FALLING); // Attach ISR for copy memory button

    // CW mode interrupt
    pinMode(cw_key_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(cw_key_pin), toggle_cw, FALLING);

    radio_tune(*current_frequency);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    if (do_tuning() || (rit_enabled && rit_enc_movement != 0)) {
        update_rit();
        int32_t display_frequency = *current_frequency + (rit_enabled ? rit_offset * 1000 : 0);
        radio_tune(display_frequency);
        char buff[100];
        sprintf(buff, "VFO %c: %d RIT: %.1f", vfo_a_active ? 'A' : 'B', display_frequency, rit_offset);
        print_line1(buff);
    }

    if (mem_enc_movement != 0) {
        current_memory_index += mem_enc_movement;
        mem_enc_movement = 0;

        if (current_memory_index < 0) current_memory_index = 0;
        if (current_memory_index >= MEMORY_SLOTS) current_memory_index = MEMORY_SLOTS - 1;

        char buff[50];
        sprintf(buff, "Memory Select: %d", current_memory_index);
        print_line1(buff);
    }

    // Handle audio input and output
    handle_audio();

    // Handle CW transmission
    handle_cw();
}
