#include <Arduino.h>
#include "Adafruit_BMP280.h"
#include "Adafruit_LIS3MDL.h"
#include "Adafruit_SHT31.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_ST7789.h"
#include "Readable_Adafruit_LSM6DS33.h"
#include "bluefruit.h"

#define DEVICE_NAME "RAIDERS Cone"

#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF

#define TWENTY_FOUR_BITS 0b00000000111111111111111111111111

#define LEFT_BUTTON 5
#define RIGHT_BUTTON 11

struct Entry {
    const uint8_t id;
    const float epoch;
    // actually 24 or 16
    int32_t lastWrittenValue;
};

const unsigned long updateTimeMs = 10;

Entry gyroX = {0, 500, 0};
Entry gyroY = {1, 500, 0};
Entry gyroZ = {2, 500, 0};
Entry accelX = {3, 100,  0};
Entry accelY = {4, 100,  0};
Entry accelZ = {5, 100,  0};
Entry magX = {6, 500,  0};
Entry magY = {7, 500,  0};
Entry magZ = {8, 500,  0};
Entry pressure = {9, 50, 0};
Entry temperature = {10, 128, 0};

struct EntryData {
    int32_t idAndValue;
    unsigned long timestamp;
};

// Sensors
Adafruit_BMP280 pressureSensor = Adafruit_BMP280();
Readable_Adafruit_LSM6DS33 accelGyroSensor = Readable_Adafruit_LSM6DS33();
Adafruit_LIS3MDL magnetometer = Adafruit_LIS3MDL();

Adafruit_ST7789 *display;

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem fatFileSystem;
File logFile;

bool inLogMode;

// Only here to deal with negative values
int32_t convertTo24Bit(int32_t value) {
    return value & TWENTY_FOUR_BITS;
}

uint8_t beaconUuid[16] = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
                0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0};
BLEBeacon beacon(beaconUuid, 0x0102, 0x0304, -54);
void startBluetooth() {
    Bluefruit.begin();

    beacon.setManufacturer(0x004C);

    Bluefruit.Advertising.setBeacon(beacon);
    Bluefruit.setName(DEVICE_NAME);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 160);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void logValue(uint8_t id, unsigned long time, int32_t value) {
    value = convertTo24Bit(value);
    EntryData newData = {value | (id << 24), time};

    auto* bytes = reinterpret_cast<uint8_t*>(&newData);

    int written = (int) logFile.write(bytes, sizeof(EntryData));
    if (written < (int)sizeof(EntryData) && written != -1) {
        // We do not want a partial entry because that will corrupt everything
        logFile.seekCur(-written);
    }
}


bool logIfAboveEpoch(unsigned long time, int32_t value, Entry& entry) {
    if (abs(value - entry.lastWrittenValue) >= entry.epoch) {
        entry.lastWrittenValue = value;
        delay(20);
        logValue(entry.id, time, value);
        return true;
    }
    return false;
}

void initialLog(unsigned long time, int32_t value, Entry& entry) {
    entry.lastWrittenValue = value;
    logValue(entry.id, time, value);
}

void clearDisplay() {
    display->fillScreen(BLACK);
    display->setCursor(0, 0);
    display->setTextColor(WHITE);
}

void displayWrapped(const String& message, int fontSize) {
    display->setTextSize(fontSize);
    unsigned int maxPerLine = 240 / (fontSize * 6);
    const char* charArray = message.c_str();

    unsigned int charsOnLine = 0;
    for (unsigned int i = 0; i < message.length(); ++i) {
        if (charArray[i] == '\n') {
            charsOnLine = 0;
        } else {
            int nextSpace = message.indexOf(' ', i);
            if (nextSpace != -1) {
                if ((nextSpace - i) + charsOnLine > maxPerLine || (charsOnLine >= maxPerLine && charArray[i] == ' ')) {
                    display->write('\n');
                    charsOnLine = 0;
                }
            } else {
                if ((message.length() - i) + charsOnLine > maxPerLine) {
                    display->write('\n');
                    charsOnLine = 0;
                }
            }
        }
        if (!(charsOnLine == 0 && charArray[i] == ' ')) {
            charsOnLine++;
            display->write(charArray[i]);
        }
    }
    display->println();
}

void initializeDisplay() {
    // Backlight
    pinMode(34, OUTPUT);
    analogWrite(34, 240);

    display = new Adafruit_ST7789(&SPI1, 31, 32, 33);

    display->init(240, 240);
    display->setRotation(1);
    clearDisplay();
}

[[noreturn]] void fail(const String& error) {
    clearDisplay();
    display->setTextColor(RED);
    displayWrapped(error, 1);
    while (true) {
        digitalWrite(17, HIGH);
        delay(400);
        digitalWrite(17, LOW);
        delay(400);
    }
}

void logModeSetup() {
    clearDisplay();
    displayWrapped("Logging will start in 2 seconds", 2);
    delay(2000);
    // Turn off backlight
    analogWrite(34, 0);
    display->enableDisplay(false);

    unsigned long currentTime = millis();
    accelGyroSensor.read();
    initialLog(currentTime, accelGyroSensor.rawGyroX, gyroX);
    initialLog(currentTime, accelGyroSensor.rawGyroY, gyroY);
    initialLog(currentTime, accelGyroSensor.rawGyroZ, gyroZ);

    initialLog(currentTime, accelGyroSensor.rawAccX, accelX);
    initialLog(currentTime, accelGyroSensor.rawAccY, accelY);
    initialLog(currentTime, accelGyroSensor.rawAccZ, accelZ);

    initialLog(currentTime, accelGyroSensor.rawTemp, temperature);

    magnetometer.read();
    initialLog(currentTime, magnetometer.x, magX);
    initialLog(currentTime, magnetometer.y, magY);
    initialLog(currentTime, magnetometer.z, magZ);

    int32_t rawPressure = ((int32_t) pressureSensor.readPressure());
    initialLog(currentTime, rawPressure, pressure);

    logFile.flush();
}

void retrieveModeSetup() {
    clearDisplay();
    display->setTextColor(GREEN);
    displayWrapped("Click left to send data by serial", 2);
    display->println();
    display->setTextColor(CYAN);
    displayWrapped("Hold right to erase log file", 2);

    delay(1);
    unsigned long rightStartTime = 0;
    while (true) {
        if (!digitalRead(LEFT_BUTTON)) {
            clearDisplay();
            uint32_t fileSize = logFile.size();
            displayWrapped("Sending file...", 2);
            unsigned int wroteBytes = 0;
            logFile.seek(0);
            while (logFile.available()) {
                wroteBytes += Serial.write(logFile.read());
            }
            Serial.flush();
            clearDisplay();
            char temp[50];
            sprintf(temp, "Wrote %d bytes of %lu total", wroteBytes, fileSize);
            displayWrapped(temp, 2);
            display->println();
            displayWrapped("Use reset button to proceed", 2);
            while (true);
        }
        if (!digitalRead(RIGHT_BUTTON)) {
            if (rightStartTime == 0) {
                rightStartTime = millis();
            }
            if (millis() - rightStartTime > 4000) {
                if (logFile.remove()) {
                    clearDisplay();
                    displayWrapped("File erased", 2);
                    display->println();
                    displayWrapped("Use reset button to proceed", 2);
                    tone(46, 1000, 100);
                    while (true);
                }
                fail("Failed to erase file");
            }
        } else {
            rightStartTime = 0;
        }
    }
}

unsigned long lastUpdateMs = 0;
unsigned long lastFlushMs = 0;
bool somethingWrote = false;
void logModeLoop() {
    // Read data
    unsigned long currentTime = millis();
    // Left button is do not record button
    if (currentTime - lastUpdateMs >= updateTimeMs && digitalRead(LEFT_BUTTON)) {
        accelGyroSensor.read();
        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawGyroX, gyroX);
        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawGyroY, gyroY);
        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawGyroZ, gyroZ);

        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawAccX, accelX);
        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawAccY, accelY);
        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawAccZ, accelZ);

        somethingWrote |= logIfAboveEpoch(currentTime, accelGyroSensor.rawTemp, temperature);

        magnetometer.read();
        somethingWrote |= logIfAboveEpoch(currentTime, magnetometer.x, magX);
        somethingWrote |= logIfAboveEpoch(currentTime, magnetometer.y, magY);
        somethingWrote |= logIfAboveEpoch(currentTime, magnetometer.z, magZ);

        auto rawPressure = (int32_t) pressureSensor.readPressure();
        somethingWrote |= logIfAboveEpoch(currentTime, rawPressure, pressure);
    }

    if (somethingWrote && currentTime - lastFlushMs > 1000) {
        logFile.flush();
        lastFlushMs = currentTime;
        somethingWrote = false;
    }

    Serial.println(millis() - currentTime);
}

void retrieveModeLoop() {
}

void setup() {
    Serial.begin(115200);
    // Buttons
    pinMode(LEFT_BUTTON, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    // Light
    pinMode(17, OUTPUT);
    // Power led type thing
    digitalWrite(17, HIGH);

    initializeDisplay();
    if (!flash.begin()) {
        fail("Failed to initialize flash");
    }
    if (!fatFileSystem.begin(&flash)) {
        fail("Failed to initialize file system");
    }
    logFile = fatFileSystem.open("data.log", FILE_WRITE);
    if (!logFile) {
        delay(500);
        if (!digitalRead(LEFT_BUTTON) && !digitalRead(RIGHT_BUTTON)) {
            clearDisplay();
            if (fatFileSystem.remove("data.log")) {
                display->println("Recovery erase complete");
                while(true);
            } else {
                fail("Recovery failed. Consider reformation flash");
            }
        }
        fail("Failed to open file");
    }
    if (!pressureSensor.begin(0x76)) {
        fail("Failed to initialize BMP280");
    }
    if (!accelGyroSensor.begin_I2C()) {
        fail("Failed to initialize LSM6DS");
    }
    if (!magnetometer.begin_I2C()) {
        fail("Failed to initialize LIS3MDL");
    }

    display->setCursor(0, 0);
    displayWrapped("Mode Select", 3);
    display->println();
    display->setTextColor(GREEN);
    displayWrapped("Press left button to start logging", 2);

    display->setTextColor(CYAN);
    display->println();
    displayWrapped("Press right button to enter retrieve mode", 2);

    startBluetooth();

    while (true) {
        if (!digitalRead(LEFT_BUTTON)) {
            inLogMode = true;
            break;
        }
        if (!digitalRead(RIGHT_BUTTON)) {
            inLogMode = false;
            break;
        }
    }

    if (inLogMode) {
        logModeSetup();
    } else {
        retrieveModeSetup();
    }
}

void loop() {
    if (inLogMode) {
        logModeLoop();
    } else {
        retrieveModeLoop();
    }
}
