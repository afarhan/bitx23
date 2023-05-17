/*
The first time,
press bootsel while plugging in the pico
when the drive shows, copy paste the uf2 compiled by Arduin0

from next time, it will program through the com port
unless the program is crashing, then you have to do the
bootsel all over again
*/

#include "Wire.h"

#define ENC_A (8)
#define ENC_B (7)
#define ENC_SWITCH (6)

#define MODE_USB 0
#define MODE_LSB 1

int32_t ticks = 0;
const int32_t if_frequency = 26994100;
int32_t current_frequency = 145900000;
//int32_t current_frequency =   7000000;
int8_t current_mode = MODE_USB;

/* encoder routines */
uint8_t enc_state (void){
  static uint8_t prev_state = 0;

  uint8_t new_state = digitalRead(ENC_A) + (2*digitalRead(ENC_B));
  return new_state;
}

int enc_movement = 0;
int32_t debounce_until = 0;

void encoder_isr(){
  static uint8_t saved_enc = 0;
  uint8_t enc_now, enc_prev;

  enc_now = enc_state();
	if (enc_now == saved_enc)
		return;

	//swap the state before we return
	enc_prev = saved_enc;
	saved_enc = enc_now;

	if((enc_prev == 2 && enc_now == 3) ||
		(enc_prev == 3 && enc_now == 1) || (enc_prev == 1 && enc_now == 0) ||
		(enc_prev == 0 && enc_now == 2))
			enc_movement++;
 	if ((enc_prev == 3 && enc_now == 2) || (enc_prev == 2 && enc_now == 0) ||
		(enc_prev == 0 && enc_now == 1) || (enc_prev == 1 && enc_now == 3))
		enc_movement--;
}

void radio_tune(int frequency){
  int32_t lo;
  if(current_mode == MODE_LSB){
  	if (frequency < if_frequency)
			lo = if_frequency - frequency;
		else
			lo = frequency - if_frequency;   
  }
  else
    lo = frequency + if_frequency;    
	si570_tune(lo);
}

int32_t do_tuning(){
  char buff[100];
	
	if (!enc_movement)
		return 0;
	if(enc_movement != 0)
		current_frequency += 100 * enc_movement;

	enc_movement = 0;
	radio_tune(current_frequency);
	return current_frequency;
}

void setup() {
 Serial.begin(115200);//  lcd.begin(16, 2);
 delay(1000);

  pinMode(ENC_SWITCH, INPUT_PULLUP);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  display_init();
  si570_init();

	attachInterrupt(digitalPinToInterrupt(ENC_A), encoder_isr, CHANGE);
	attachInterrupt(digitalPinToInterrupt(ENC_B), encoder_isr, CHANGE);

	radio_tune(current_frequency);
 // print_line1("dayton");
 // si570_tune(41000000L);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop(){
	ticks = millis();
 // do_tuning();  //print_line1("hey");
	if(do_tuning()){
		char buff[100];
  	sprintf(buff, "%d", current_frequency);
  	print_line1(buff);
	}
}
