#include <Arduino.h>
#include "Adafruit_BMP280.h"
#include "Adafruit_LIS3MDL.h"
#include "Adafruit_SHT31.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_ST7789.h"
#include "Readable_Adafruit_LSM6DS33.h"
#include "BluetoothManager.h"
#include "LaunchDetector.h"

#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF

#define LEFT_BUTTON 5
#define RIGHT_BUTTON 11
#define TONE_PIN 46

[[noreturn]] void suspend() {
    while (true);
}

struct Entry {
    const uint8_t id;
    const float epoch;
    // actually 24 or 16
    int32_t lastWrittenValue;
};

const unsigned long updateTimeMs = 100;

// Do not have any ids >= 192
// 1 rad/s is 6548
Entry gyroXEntry = {0, 700, 0};
Entry gyroYEntry = {1, 700, 0};
Entry gyroZEntry = {2, 700, 0};
// 1 m/s^2 is 417 units
Entry accelXEntry = {3, 80, 0};
Entry accelYEntry = {4, 80, 0};
Entry accelZEntry = {5, 80, 0};
// 1 gauss is 6842 units
Entry magXEntry = {6, 6842, 0};
Entry magYEntry = {7, 6842, 0};
Entry magZEntry = {8, 6842, 0};
// 1 is 1 Pa. 1 foot in altitude is about 5 Pa
Entry pressureEntry = {9, 5, 0};
// 1 degree c is 256
Entry interiorTemperatureEntry = {10, 256, 0};
// 1 degree c is 100
Entry exteriorTemperatureEntry = {11, 100, 0};

struct EntryData {
    int32_t idAndValue;
};

struct TimeStampData {
    uint32_t startAndAmount;
    uint32_t timeStamp;
};

// Sensors
Adafruit_BMP280* pressureSensor = new Adafruit_BMP280();
Readable_Adafruit_LSM6DS33* accelGyroSensor = new Readable_Adafruit_LSM6DS33();
Adafruit_LIS3MDL* magnetometer = new Adafruit_LIS3MDL();

Adafruit_ST7789* display;

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem fatFileSystem;
File logFile;

LaunchDetector* launchDetector = new LaunchDetector();

bool inLogMode;

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
        tone(TONE_PIN, 2000, 400);
        delay(400);
        digitalWrite(17, LOW);
        delay(400);
    }
}

void writeBytesAndCheckCorruption(uint8_t* bytes, size_t amount) {
    int written = (int) logFile.write(bytes, amount);
    if (written != (int) amount) {
        // We do not want a partial entry because that will corrupt everything
        logFile.seekCur(-written);
    }
}

void logValue(uint8_t id, int32_t value) {
    EntryData newData = {((value << 8) | (id))};

    writeBytesAndCheckCorruption(reinterpret_cast<uint8_t*>(&newData), sizeof(EntryData));
}

int logIfAboveEpoch(int32_t value, Entry& entry) {
    if (abs(value - entry.lastWrittenValue) >= entry.epoch) {
        entry.lastWrittenValue = value;
        logValue(entry.id, value);
        return 1;
    }
    return 0;
}

void logTimeStamp(uint8_t amountLogged, uint32_t timeStamp) {
    uint32_t amountLoggedFlagged = (amountLogged << 24) | 0x00ffffff;
    TimeStampData timeStampData = {amountLoggedFlagged, timeStamp};

    writeBytesAndCheckCorruption(reinterpret_cast<uint8_t*>(&timeStampData), sizeof(TimeStampData));
}

int initialLog(int32_t value, Entry& entry) {
    entry.lastWrittenValue = value;
    logValue(entry.id, value);
    return 1;
}

void logModeSetup() {
    clearDisplay();
    displayWrapped("Logging will start in 2 seconds", 2);
    tone(TONE_PIN, 500, 2000);
    delay(2000);
    // Turn off backlight
    analogWrite(34, 0);
    display->enableDisplay(false);

    unsigned long currentTime = millis();
    int amountWritten = 0;

    accelGyroSensor->read();
    amountWritten += initialLog(accelGyroSensor->rawGyroX, gyroXEntry);
    amountWritten += initialLog(accelGyroSensor->rawGyroY, gyroYEntry);
    amountWritten += initialLog(accelGyroSensor->rawGyroZ, gyroZEntry);

    amountWritten += initialLog(accelGyroSensor->rawAccX, accelXEntry);
    amountWritten += initialLog(accelGyroSensor->rawAccY, accelYEntry);
    amountWritten += initialLog(accelGyroSensor->rawAccZ, accelZEntry);

    magnetometer->read();
    amountWritten += initialLog(magnetometer->x, magXEntry);
    amountWritten += initialLog(magnetometer->y, magYEntry);
    amountWritten += initialLog(magnetometer->z, magZEntry);

    amountWritten += initialLog(pressureSensor->readTemperature() * 100, exteriorTemperatureEntry);

    float pressure = pressureSensor->readPressure();
    auto rawPressure = (int32_t) pressure;

    amountWritten += initialLog(accelGyroSensor->rawTemp, interiorTemperatureEntry);
    amountWritten += initialLog(rawPressure, pressureEntry);

    logTimeStamp(amountWritten, currentTime);

    logFile.flush();
}

unsigned long lastUpdateMs = 0;
unsigned long lastFlushMs = 0;
void logModeLoop() {
    // Read data
    unsigned long currentTime = millis();
    int amountWritten = 0;
    // Left button is do not record button
    if (currentTime - lastUpdateMs >= updateTimeMs && digitalRead(LEFT_BUTTON)) {
        accelGyroSensor->read();
        launchDetector->update(accelGyroSensor->accX, accelGyroSensor->accY, accelGyroSensor->accZ);
        if (!launchDetector->hasLaunched() || launchDetector->hasLanded()) {
            return;
        }

        amountWritten += logIfAboveEpoch(accelGyroSensor->rawGyroX, gyroXEntry);
        amountWritten += logIfAboveEpoch(accelGyroSensor->rawGyroY, gyroYEntry);
        amountWritten += logIfAboveEpoch(accelGyroSensor->rawGyroZ, gyroZEntry);

        amountWritten += logIfAboveEpoch(accelGyroSensor->rawAccX, accelXEntry);
        amountWritten += logIfAboveEpoch(accelGyroSensor->rawAccY, accelYEntry);
        amountWritten += logIfAboveEpoch(accelGyroSensor->rawAccZ, accelZEntry);

        amountWritten += logIfAboveEpoch(accelGyroSensor->rawTemp, interiorTemperatureEntry);

        magnetometer->read();
        amountWritten += logIfAboveEpoch(magnetometer->x, magXEntry);
        amountWritten += logIfAboveEpoch(magnetometer->y, magYEntry);
        amountWritten += logIfAboveEpoch(magnetometer->z, magZEntry);

        amountWritten += logIfAboveEpoch(pressureSensor->readTemperature() * 100, exteriorTemperatureEntry);

        float pressure = pressureSensor->readPressure();
        auto rawPressure = (int32_t) pressure;
        amountWritten += logIfAboveEpoch(rawPressure, pressureEntry);

        if (amountWritten > 0) {
            logTimeStamp(amountWritten, currentTime);
        }
        lastUpdateMs = currentTime;
    }

    if (currentTime - lastFlushMs > 1000) {
        logFile.flush();
        lastFlushMs = currentTime;
    }
}

void retrieveModeSetup() {
    clearDisplay();
    display->setTextColor(GREEN);
    displayWrapped("Other Settings", 2);
    display->println();
    display->setTextColor(CYAN);
    displayWrapped("Hold right to erase log file", 2);

    delay(100);
    unsigned long rightStartTime = 0;
    while (true) {
        if (!digitalRead(LEFT_BUTTON)) {
            clearDisplay();
            display->setTextColor(GREEN);
            displayWrapped("Click left to send data by serial", 2);
            display->println();
            display->setTextColor(CYAN);
            displayWrapped("Placeholder", 2);
            delay(100);
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
                    suspend();
                }
                if (!digitalRead(RIGHT_BUTTON)) {
                    clearDisplay();
                    display->setTextColor(MAGENTA);
                    displayWrapped("Placeholder", 2);
                }
            }
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
                    tone(TONE_PIN, 100, 100);
                    suspend();
                }
                fail("Failed to erase file");
            }
        } else {
            rightStartTime = 0;
        }
    }
}

void retrieveModeLoop() {
}

void setup() {
    Serial.begin(115200);
    // Buttons
    pinMode(LEFT_BUTTON, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    pinMode(1, INPUT_PULLUP);
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
                suspend();
            } else {
                fail("Recovery failed. Consider reformation flash");
            }
        }
        fail("Failed to open file");
    }
    if (!pressureSensor->begin(0x76)) {
        fail("Failed to initialize BMP280");
    }
    if (!accelGyroSensor->begin_I2C()) {
        fail("Failed to initialize LSM6DS");
    }
    accelGyroSensor->setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
    if (!magnetometer->begin_I2C()) {
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

    bluetooth::startBluetooth();

    uint32_t startMillis = millis();
    while (true) {
        if (!digitalRead(LEFT_BUTTON) || (!digitalRead(1) && millis() - startMillis > 2000)) {
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
