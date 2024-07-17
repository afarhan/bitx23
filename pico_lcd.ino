#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TFT_eSPI.h>

// Define pins for I2C and SPI
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define SPI_CS_PIN 10
#define SPI_DC_PIN 9
#define SPI_RST_PIN 8

// Define display parameters
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)

// Create TFT_eSPI instance
TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

void setup() {
    // Initialize UART for debugging
    Serial.begin(115200);
    Serial.println("Initializing...");

    // Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Serial.println("I2C Initialized");

    // Initialize SPI
    SPI.begin();
    Serial.println("SPI Initialized");

    // Initialize the display
    tft.init();
    tft.setRotation(1); // Set the display orientation

    // Clear the display
    tft.fillScreen(TFT_BLACK);

    // Draw initial graphics
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println("Bitx23 VFO");

    // Additional graphics example
    tft.drawLine(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, TFT_WHITE);
    tft.drawLine(SCREEN_WIDTH, 0, 0, SCREEN_HEIGHT, TFT_WHITE);

    // Draw a rectangle
    tft.drawRect(50, 50, 100, 50, TFT_GREEN);

    // Draw a filled circle
    tft.fillCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 30, TFT_BLUE);
}

void loop() {
    // Example of updating the display with new information
    static int counter = 0;
    tft.setCursor(10, 50);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Set text color with background
    tft.setTextSize(2);
    tft.print("Counter: ");
    tft.println(counter);
    counter++;

    // Add a delay to make the counter visible
    delay(1000);
}
