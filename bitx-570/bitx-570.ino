/*
The first time,
press bootsel while plugging in the pico
when the drive shows, copy paste the uf2 compiled by Arduin0

from next time, it will program through the com port
unless the program is crashing, then you have to do the
bootsel all over again
*/

#include "Wire.h"

// Existing pin definitions
#define ENC_A (8)
#define ENC_B (7)
#define ENC_SWITCH (6) // switch vfo.

// Switch VFO A/B 
#define VFO_SWITCH (2)

#define S_METER_PIN (9)
#define CW_PIN (10)
#define SPLIT_SWITCH (11)
#define PTT_PIN (12)

// Mem Function Encoder
#define MEM_SAVE (3)
#define MEM_SELECT (4)
#define MEM_SCAN (5)

// RIT Function  Encoder
#define RIT_ENC_A (15) //+ rit
#define RIT_ENC_B (14) //- rit
#define RIT_ENC_SWITCH (13) // short push RIT reset long push enable/disable

// New pin definitions for microphone and speaker
#define MIC_PIN (A0) // Microphone input pin
#define SPEAKER_PIN (5) // Speaker output pin (PWM)
#define SQUELCH_THRESHOLD_PIN (A1) // Analog pin for squelch threshold adjustment

#define VFO_A_DEFAULT_FREQ 144200000
#define VFO_B_DEFAULT_FREQ 144100000
#define BAND_LIMIT_LOW 144000000
#define BAND_LIMIT_HIGH 144310000

int32_t ticks = 0;
const int32_t if_frequency = 26994100;
int32_t current_frequency = VFO_A_DEFAULT_FREQ;
int32_t vfo_a_frequency = VFO_A_DEFAULT_FREQ;
int32_t vfo_b_frequency = VFO_B_DEFAULT_FREQ;

bool split_enabled = false; // Split frequency mode state
bool ptt_active = false; // PTT active state
bool rit_enabled = false; // RIT enabled state
int32_t rit_offset = 0; // RIT offset

int squelch_threshold = 512; // Default squelch threshold

enum Mode { USB, LSB, CWU, CWL };
Mode current_mode = USB;

enum VFO { VFO_A, VFO_B };
VFO current_vfo = VFO_A;

int32_t memories[10] = {0};
int memory_index = 0;

int enc_movement = 0;
int32_t debounce_until = 0;

// Step sizes and index
const int32_t step_sizes[] = {1, 10, 100, 1000, 10000};
int current_step_index = 2; // Default step size is 100 Hz

uint8_t enc_state() {
    return digitalRead(ENC_A) + (2 * digitalRead(ENC_B));
}

void encoder_isr() {
    static uint8_t saved_enc = 0;
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();
    
    if (interrupt_time - last_interrupt_time > 5) { // 5ms debounce time
        uint8_t enc_now, enc_prev;

        enc_now = enc_state();
        if (enc_now == saved_enc) return;

        enc_prev = saved_enc;
        saved_enc = enc_now;

        if ((enc_prev == 2 && enc_now == 3) || (enc_prev == 3 && enc_now == 1) || (enc_prev == 1 && enc_now == 0) || (enc_prev == 0 && enc_now == 2))
            enc_movement++;
        if ((enc_prev == 3 && enc_now == 2) || (enc_prev == 2 && enc_now == 0) || (enc_prev == 0 && enc_now == 1) || (enc_prev == 1 && enc_now == 3))
            enc_movement--;

        if (rit_enabled) {
            if (enc_movement > 0) {
                increase_rit();
            } else {
                decrease_rit();
            }
            enc_movement = 0;
        }
    }
    last_interrupt_time = interrupt_time;
}

void radio_tune(int32_t frequency) {
    int32_t lo;
    int32_t rx_frequency = frequency + (rit_enabled ? rit_offset : 0); // Adjust for RIT
    if (current_mode == LSB || current_mode == CWL) {
        lo = (rx_frequency < if_frequency) ? if_frequency - rx_frequency : rx_frequency - if_frequency;
    } else {
        lo = rx_frequency + if_frequency;
    }
    si570_tune(lo);
}

int32_t do_tuning() {
    if (!enc_movement) return 0;
    current_frequency += step_sizes[current_step_index] * enc_movement;
    enc_movement = 0;
    radio_tune(current_frequency);
    return current_frequency;
}

void display_frequency(int32_t frequency) {
    char buff[100];
    sprintf(buff, "%d", frequency);
    print_line1(buff);
}

void display_mode() {
    switch (current_mode) {
        case USB:
            print_line2("USB");
            break;
        case LSB:
            print_line2("LSB");
            break;
        case CWU:
            print_line2("CWU");
            break;
        case CWL:
            print_line2("CWL");
            break;
    }
}

void display_s_meter() {
    int s_meter_value = analogRead(S_METER_PIN);
    char buff[100];
    sprintf(buff, "S-meter: %d", s_meter_value);
    print_line3(buff);
}

void display_vfo_frequencies() {
    if (split_enabled) {
        char buff[100];
        sprintf(buff, "RX: %d TX: %d", vfo_a_frequency, vfo_b_frequency);
        print_line4(buff);
    } else {
        display_frequency(current_frequency);
    }
}

void display_rit_status() {
    if (rit_enabled) {
        char buff[100];
        sprintf(buff, "RIT: %+d", rit_offset); // Show RIT offset with sign
        print_line5(buff);
    } else {
        print_line5("RIT Off");
    }
}

void display_ptt_status() {
    if (ptt_active) {
        print_line6("PTT Active");
    } else {
        print_line6("PTT Inactive");
    }
}

void display_squelch_status() {
    char buff[100];
    sprintf(buff, "Squelch: %d", squelch_threshold); // Show squelch threshold
    print_line7(buff); // Adjust to the appropriate display line
}

void handle_vfo_switch() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(VFO_SWITCH) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        if (current_vfo == VFO_A) {
            vfo_a_frequency = current_frequency;
            current_frequency = vfo_b_frequency;
            current_vfo = VFO_B;
        } else {
            vfo_b_frequency = current_frequency;
            current_frequency = vfo_a_frequency;
            current_vfo = VFO_A;
        }
        radio_tune(current_frequency);
        last_press_time = press_time;
    }
}

void handle_memory_save() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(MEM_SAVE) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        memories[memory_index] = current_frequency;
        last_press_time = press_time;
    }
}

void handle_memory_select() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(MEM_SELECT) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        current_frequency = memories[memory_index];
        radio_tune(current_frequency);
        last_press_time = press_time;
    }
}

void handle_memory_scan() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(MEM_SCAN) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        for (int i = 0; i < 10; i++) {
            if (memories[i] != 0) {
                current_frequency = memories[i];
                radio_tune(current_frequency);
                delay(1000);
            }
        }
        last_press_time = press_time;
    }
}

void handle_mode_switch() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(CW_PIN) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        current_mode = static_cast<Mode>((current_mode + 1) % 4); // Cycle through modes
        display_mode();
        radio_tune(current_frequency);
        last_press_time = press_time;
    }
}

void handle_split_switch() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(SPLIT_SWITCH) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        split_enabled = !split_enabled; // Toggle split mode
        if (split_enabled) {
            // Switch to VFO A for receive and VFO B for transmit
            vfo_a_frequency = current_frequency;
            current_frequency = vfo_b_frequency;
        } else {
            // Switch back to single VFO mode
            current_frequency = vfo_a_frequency;
        }
        radio_tune(current_frequency);
        display_vfo_frequencies();
        last_press_time = press_time;
    }
}

void handle_ptt() {
    ptt_active = (digitalRead(PTT_PIN) == LOW);
    display_ptt_status();
}

void handle_rit_switch() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(RIT_SWITCH) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        rit_enabled = !rit_enabled; // Toggle RIT mode
        display_rit_status();
        last_press_time = press_time;
    }
}

void handle_rit_adjust() {
    if (rit_enabled) {
        int adjustment = analogRead(RIT_ADJUST);
        rit_offset = map(adjustment, 0, 1023, -5000, 5000); // Map to desired range
        radio_tune(current_frequency);
        display_rit_status();
    }
}

void handle_rit_reset() {
    static unsigned long last_press_time = 0;
    unsigned long press_time = millis();
    
    if (digitalRead(RIT_RESET_PIN) == LOW && press_time - last_press_time > 200) { // 200ms debounce time
        reset_rit();
        last_press_time = press_time;
    }
}

void reset_rit() {
    rit_offset = 0;
    radio_tune(current_frequency);
    display_rit_status();
}

// New function to process microphone input and output to speaker
void process_audio() {
    int mic_value = analogRead(MIC_PIN); // Read microphone input
    int threshold_value = analogRead(SQUELCH_THRESHOLD_PIN); // Read squelch threshold
    squelch_threshold = map(threshold_value, 0, 1023, 0, 1023); // Map to desired range

    if (mic_value > squelch_threshold) {
        int speaker_value = map(mic_value, 0, 1023, 0, 255); // Map to PWM range
        analogWrite(SPEAKER_PIN, speaker_value); // Output to speaker
    } else {
        analogWrite(SPEAKER_PIN, 0); // Mute speaker if below squelch threshold
    }
}

// Functions to change step size
void increase_step_size() {
    if (current_step_index < 4) {
        current_step_index++;
    }
}

void decrease_step_size() {
    if (current_step_index > 0) {
        current_step_index--;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(ENC_SWITCH, INPUT_PULLUP);
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    pinMode(VFO_SWITCH, INPUT_PULLUP);
    pinMode(MEM_SAVE, INPUT_PULLUP);
    pinMode(MEM_SELECT, INPUT_PULLUP);
    pinMode(MEM_SCAN, INPUT_PULLUP);
    pinMode(S_METER_PIN, INPUT);
    pinMode(CW_PIN, INPUT_PULLUP); // Setup CW pin as input
    pinMode(SPLIT_SWITCH, INPUT_PULLUP); // Setup split switch as input
    pinMode(PTT_PIN, INPUT_PULLUP); // Setup PTT pin as input
    pinMode(RIT_SWITCH, INPUT_PULLUP); // Setup RIT switch as input
    pinMode(RIT_ADJUST, INPUT); // Setup RIT adjust as input
    pinMode(RIT_RESET_PIN, INPUT_PULLUP); // Setup RIT reset pin as input

    // Setup microphone and speaker pins
    pinMode(MIC_PIN, INPUT);
    pinMode(SPEAKER_PIN, OUTPUT);
    pinMode(SQUELCH_THRESHOLD_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(ENC_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encoder_isr, CHANGE);

    display_init();
    si570_init();
}

void loop() {
    handle_vfo_switch();
    handle_memory_save();
    handle_memory_select();
    handle_memory_scan();
    handle_mode_switch();
    handle_split_switch();
    handle_ptt();
    handle_rit_switch();
    handle_rit_adjust();
    handle_rit_reset(); // Handle RIT reset

    // Add step size handling
    if (digitalRead(STEP_INCREASE_PIN) == LOW) {
        increase_step_size();
    }
    if (digitalRead(STEP_DECREASE_PIN) == LOW) {
        decrease_step_size();
    }

    display_s_meter();
    display_vfo_frequencies();
    display_rit_status();
    display_ptt_status();
    display_squelch_status(); // Display the squelch threshold

    do_tuning();

    // Process audio input and output
    process_audio();
}

void increase_rit() {
    rit_offset += 100; // Adjust step size as needed
    radio_tune(current_frequency);
    display_rit_status();
}

void decrease_rit() {
    rit_offset -= 100; // Adjust step size as needed
    radio_tune(current_frequency);
    display_rit_status();
}
