#include <Wire.h>
#include si570.h

// Pin Definitions
#define ENC_A (8)
#define ENC_B (7)
#define ENC_SWITCH (6)
#define VFO_SWAP_PIN (9)
#define ENC_RIT_A (10)
#define ENC_RIT_B (11)
#define RIT_BUTTON_PIN (12)
#define STEP_CHANGE_PIN (13)
#define ENC_MEM_A (2)
#define ENC_MEM_B (3)
#define MEM_BUTTON_PIN (4)
#define MEM_TO_VFO_PIN (15)
#define SPEAKER_PIN (5)
#define MIC_PIN (6)
#define SQUELCH_PIN (14)
#define CW_KEY_PIN (16)
#define RELAY_PIN (17) // Define the relay pin

// Mode Definitions
#define MODE_USB 0
#define MODE_LSB 1
#define MODE_CW 2

// Constants
#define MEMORY_SLOTS 20
#define SQUELCH_THRESHOLD 100

const int32_t if_frequency = 26994100;
const int32_t BAND_LIMIT_LOW = 144000000;
const int32_t BAND_LIMIT_HIGH = 144310000;

// Frequency Variables
int32_t current_frequency_A = 144200000;
int32_t current_frequency_B = 144250000;
int32_t* current_frequency = &current_frequency_A;
int8_t current_mode = MODE_USB;
bool vfo_a_active = true;

// Memory Variables
int32_t memories[MEMORY_SLOTS];
int current_memory_index = 0;

// Encoder Movement Variables
int enc_movement = 0;
int mem_enc_movement = 0;
bool rit_enabled = false;
float rit_offset = 0.0;
int rit_enc_movement = 0;

const int32_t frequency_steps[] = {1000, 10000, 100000, 1000000, 10000000};
int current_step_index = 0;

bool cw_enabled = false;
int cw_key_pin = CW_KEY_PIN;
int cw_speed = 20;

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
void copyMemoryToVFO();
void copy_memory_button_isr();
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
void control_relay(bool state); // New function prototype

// Implement the squelch control functions
int read_squelch() {
    return analogRead(SQUELCH_PIN);
}

void handle_squelch() {
    int squelch_level = read_squelch();
    if (squelch_level > SQUELCH_THRESHOLD) {
        analogWrite(SPEAKER_PIN, 0);
    } else {
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
    si570_tune(lo);
}

int32_t do_tuning() {
    if (enc_movement == 0) return 0;
    *current_frequency += frequency_steps[current_step_index] * enc_movement;
    enc_movement = 0;

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
    print_line1(buff);
}

void toggle_rit() {
    rit_enabled = !rit_enabled;
    if (!rit_enabled) {
        rit_offset = 0.0;
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

void copy_memory_button_isr() {
    copyMemoryToVFO();
}

void setup_audio() {
    pinMode(SPEAKER_PIN, OUTPUT);
    pinMode(MIC_PIN, INPUT);
    pinMode(SQUELCH_PIN, INPUT);
}

void handle_audio() {
    handle_squelch();
}

void setup_cw() {
    pinMode(cw_key_pin, INPUT_PULLUP);
}

void handle_cw() {
    if (cw_enabled) {
        bool key_state = !digitalRead(cw_key_pin);
        if (key_state) {
            analogWrite(SPEAKER_PIN, 255);
            control_relay(true); // Turn relay on during transmission
        } else {
            analogWrite(SPEAKER_PIN, 0);
            control_relay(false); // Turn relay off when not transmitting
        }
    }
}

void toggle_cw() {
    cw_enabled = !cw_enabled;
    char buff[50];
    sprintf(buff, "CW Mode: %s", cw_enabled ? "ON" : "OFF");
    print_line1(buff);
}

void control_relay(bool state) {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

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
    pinMode(MEM_TO_VFO_PIN, INPUT_PULLUP);
    pinMode(RELAY_PIN, OUTPUT); // Initialize relay pin

    display_init();
    si570_init();
    setup_audio();
    setup_cw();

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
    attachInterrupt(digitalPinToInterrupt(MEM_TO_VFO_PIN), copy_memory_button_isr, FALLING);
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

    handle_audio();
    handle_cw();
}
