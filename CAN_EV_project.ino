// ECU 1 (BMS) CODE
#include <SPI.h>
#include <mcp2515.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// CAN setup
MCP2515 mcp2515(10); // CS pin

// DS18B20 setup
#define ONE_WIRE_BUS 3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// CAN message structure
struct can_frame canMsg;

void setup() {
  Serial.begin(9600);
  sensors.begin(); // Start DS18B20
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
  Serial.println("Nano ready: SOC + Temp");
}

void loop() {
  // Read SOC from potentiometer
  int potValue = analogRead(A0);
  int soc = map(potValue, 0, 1023, 0, 100);

  // Read temperature from DS18B20
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  int tempInt = (int)(temperatureC * 10); // To preserve 1 decimal

  // Send SOC - CAN ID 0x100
  canMsg.can_id = 0x100;
  canMsg.can_dlc = 1;
  canMsg.data[0] = soc;
  mcp2515.sendMessage(&canMsg);
  Serial.print("SOC Sent: ");
  Serial.println(soc);
  delay(10);

  // Send Temp - CAN ID 0x300
  canMsg.can_id = 0x300;
  canMsg.can_dlc = 2;
  canMsg.data[0] = tempInt >> 8;    // High byte
  canMsg.data[1] = tempInt & 0xFF;  // Low byte
  mcp2515.sendMessage(&canMsg);
  Serial.print("Temp Sent: ");
  Serial.println(temperatureC);
  delay(1000);
}


// ECU 2 CODE
#include <SPI.h>
#include <mcp2515.h>

MCP2515 mcp2515(10); // CS pin
struct can_frame canMsg;

const int seatbeltPin = 2; // Digital pin connected to toggle switch

void setup() {
  pinMode(seatbeltPin, INPUT_PULLUP); // Enables internal pull-up resistor
  Serial.begin(9600);
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void loop() {
  // Logic: LOW when switch is ON (pressed), so invert it
  bool seatbeltStatus = !digitalRead(seatbeltPin); // 1 = ON, 0 = OFF
  canMsg.can_id = 0x200;      // Custom ID for seatbelt status
  canMsg.can_dlc = 1;         // Sending 1 byte
  canMsg.data[0] = seatbeltStatus ? 1 : 0;
  mcp2515.sendMessage(&canMsg);
  Serial.print("Seatbelt Status Sent: ");
  Serial.println(seatbeltStatus ? "ON" : "OFF");
  delay(1000); // Send every 1 second
}


// MAIN VCU CODE
#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address might be different (use scanner if unsure)

MCP2515 mcp2515(10); // CS pin
struct can_frame canMsg;

int soc = -1;
int seatbeltStatus = -1;
float temperature = -100; // Default to invalid temp
unsigned long lastDisplayTime = 0;
int displayState = 0; // 0 = SOC, 1 = Seatbelt, 2 = Temp

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
  lcd.setCursor(0, 0);
  lcd.print("CAN Receiver Ready");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Read CAN Messages
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 0x100 && canMsg.can_dlc >= 1) {
      soc = canMsg.data[0];
      Serial.print("SOC: ");
      Serial.println(soc);
    }
    if (canMsg.can_id == 0x200 && canMsg.can_dlc >= 1) {
      seatbeltStatus = canMsg.data[0];
      Serial.print("Seatbelt: ");
      Serial.println(seatbeltStatus ? "ON" : "OFF");
    }
    if (canMsg.can_id == 0x300 && canMsg.can_dlc == 2) {
      int tempRaw = (canMsg.data[0] << 8) | canMsg.data[1];
      temperature = tempRaw / 10.0;
      Serial.print("Temp: ");
      Serial.println(temperature);
    }
  }

  // Display on LCD every 1 second
  if (millis() - lastDisplayTime > 1000) {
    lcd.clear();
    switch (displayState) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("Battery SOC:");
        lcd.setCursor(0, 1);
        lcd.print(soc != -1 ? String(soc) + " %" : "No Data");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print("Seatbelt:");
        lcd.setCursor(0, 1);
        if (seatbeltStatus == -1)
          lcd.print("No Data");
        else
          lcd.print(seatbeltStatus ? "ON" : "OFF");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print("Battery Temp:");
        lcd.setCursor(0, 1);
        if (temperature < -50) // Invalid range
          lcd.print("No Data");
        else {
          lcd.print(temperature);
          lcd.print((char)223); // Degree symbol
          lcd.print("C");
        }
        break;
    }
    displayState = (displayState + 1) % 3;
    lastDisplayTime = millis();
  }
}
