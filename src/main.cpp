#include <Wire.h>
#include <SHT85.h>
#include <Arduino_LED_Matrix.h>

ArduinoLEDMatrix matrix;

SHT85 sht(0x44);

uint8_t muxAddr = 0;
bool channelActive[8] = {};
unsigned long lastRead = 0;
unsigned long LED_on_timer = 0;

const unsigned long READ_INTERVAL = 2000;  // ms
float temperature = 0.0;
float humidity = 0.0;

// status LEDs
const int ROWS = 8;
const int COLS = 12;
const int FLASH_INTERVAL = 100;  // ms

byte frame[ROWS][COLS];
int bottle1 = 0;  // 0–100 %
int bottle2 = 0;  // 0–100 %
bool valve1 = false;
bool valve2 = false;
bool valve3 = false;
float tempLevel1 = 0;   // 0+
float humidLevel1 = 0;  // 0–100 %
float tempLevel2 = 0;
float humidLevel2 = 0;  // 0–100 %
bool stimulus = false;

int incomingByte = 0;  // for incoming serial data

// Fill N pixels from the bottom proportional to level (0–100)
void drawTemp(int col, int level) {
  bool show = (level > 18 && level < 28) || (millis() / FLASH_INTERVAL) % 2;
  int filled = map(constrain(level, 15, 35), 15, 35, 0, ROWS);
  for (int r = 0; r < ROWS; r++) {
    frame[r][col] = (show && (ROWS - 1 - r) < filled) ? 1 : 0;
  }
}

void drawHumid(int col, int level) {
  bool show = (level > 30 && level < 70) || (millis() / FLASH_INTERVAL) % 2;
  int filled = map(constrain(level, 10, 80), 10, 80, 0, ROWS);
  for (int r = 0; r < ROWS; r++) {
    frame[r][col] = (show && (ROWS - 1 - r) < filled) ? 1 : 0;
  }
}

void drawBar(int col, int level) {
  bool show = (level < 100) || (millis() / FLASH_INTERVAL) % 2;
  int filled = map(constrain(level, 0, 100), 0, 100, 0, ROWS);
  for (int r = 0; r < ROWS; r++) {
    frame[r][col] = (show && (ROWS - 1 - r) < filled) ? 1 : 0;
  }
}

// Single dot at mid-column when ON, blank when OFF
void drawBinary(int col, bool state) {
  for (int r = 0; r < ROWS; r++) {
    frame[r][col] = (state && r == ROWS / 2) ? 1 : 0;
  }
}

// Full column lit when stimulus ON, blank when OFF
void drawStimulus(int col, bool state) {
  for (int r = 0; r < ROWS; r++) {
    frame[r][col] = state ? 1 : 0;
  }
}

void renderStatus() {
  memset(frame, 0, sizeof(frame));
  drawBar(0, bottle1);
  drawBar(2, bottle2);
  drawBinary(4, valve1);
  drawBinary(5, valve2);
  drawBinary(6, valve3);
  drawTemp(7, tempLevel1);
  drawHumid(8, humidLevel1);
  drawTemp(9, tempLevel2);
  drawHumid(10, humidLevel2);
  drawStimulus(11, stimulus);
  matrix.renderBitmap(frame, ROWS, COLS);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  matrix.begin();
  Wire.begin();

  pinMode(3, OUTPUT);

  // Find the mux (skip 0x44, the SHT85 sensor address)
  Serial.println("Scanning I2C bus...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0 && addr != 0x44) {
      muxAddr = addr;
      Serial.print("Mux found at 0x");
      Serial.println(addr, HEX);
      break;
    }
  }

  // Probe each mux channel for an SHT85
  if (muxAddr != 0) {
    for (int ch = 0; ch < 8; ch++) {
      Wire.beginTransmission(muxAddr);
      Wire.write(1 << ch);
      Wire.endTransmission();
      delay(50);
      Wire.beginTransmission(0x44);
      if (Wire.endTransmission() == 0) {
        sht.begin();  // initialise library with Wire while channel is active
        channelActive[ch] = true;
        Serial.print("SHT85 found on channel ");
        Serial.println(ch);
      }
    }
    // Deselect all channels
    Wire.beginTransmission(muxAddr);
    Wire.write(0);
    Wire.endTransmission();
  } else {
    Serial.println("No mux found.");
  }
  Serial.println("Done.");
}

void loop() {
  unsigned long now = millis();

  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();
    digitalWrite(3, HIGH);
    LED_on_timer = millis();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingByte);
  }
  if (now - LED_on_timer > 1000) {
    digitalWrite(3, LOW);
  }



  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;
    int sensorIndex = 0;
    for (int ch = 0; ch < 8; ch++) {
      if (!channelActive[ch]) continue;
      sensorIndex++;
      Wire.beginTransmission(muxAddr);
      Wire.write(1 << ch);  // select correct mux channel
      Wire.endTransmission();
      if (!sht.begin()) {
        Serial.println("begin() failed");
        continue;
      }
      delay(20);
      //Serial.println("Ch" + String(sensorIndex) + ": " + sht.read());
      if (sht.read()) {
        temperature = sht.getTemperature();
        humidity = sht.getHumidity();
        if (sensorIndex == 1) {
          tempLevel1 = temperature;
          humidLevel1 = humidity;
        } else if (sensorIndex == 2) {
          tempLevel2 = temperature;
          humidLevel2 = humidity;
        }
        Serial.print("Ch");
        Serial.print(sensorIndex);
        Serial.print("_Temperature:");
        Serial.print(temperature);
        Serial.print(",Ch");
        Serial.print(sensorIndex);
        Serial.print("_Humidity:");
        Serial.println(humidity);
      } else {
        Serial.print("Channel ");
        Serial.print(sensorIndex);
        Serial.print(": Read failed, error: ");
        Serial.println(sht.getError());
      }
    }
  }
  renderStatus();
}