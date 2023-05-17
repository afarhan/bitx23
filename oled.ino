#include <GyverOLED.h>

GyverOLED<SSH1106_128x64> oled;

void display_init(){
  oled.init();  
  
  oled.clear();   
  oled.update(); 

  oled.setCursorXY(28, 26);   
  oled.rect(0,0,127,63,OLED_STROKE);
  oled.print("Dayton");
  oled.update();
}

void print_line1(char *text){
  oled.clear();   
  oled.setCursorXY(10, 10);
	oled.setScale(2);
  oled.print(text);
  oled.update();
}
