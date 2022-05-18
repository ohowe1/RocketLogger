#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define GYRO_ANGLE 0
#define ACCELEROMETER_X 1
#define ACCELEROMETER_Y 2
#define ACCELEROMETER_Z 3

struct Entry {
    unsigned long id;
    unsigned long timestamp;
    float value;
};

#define ENTRY_SIZE sizeof(Entry)

union Data {
    Entry entry;
    uint8_t bytes[sizeof(entry)];
};

const int chipPin = 8;

const double epoch = 0.001;
const unsigned long sensorUpdateMillis = 20;

const int bufferSize = ENTRY_SIZE * 100;
uint8_t* buffer;
unsigned int bufferPosition = 0;
unsigned int nextToWriteBufferPosition = 0;
unsigned int entriesInBuffer = 0;

File logFile;

float lastWrittenGyro = 0.0;

unsigned long lastMillis;

[[noreturn]] void fail() {
    SD.end();
    SPI.end();
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }
}


void writeFloat(unsigned int type, unsigned long timestamp, float value) {
    if (bufferPosition + ENTRY_SIZE >= bufferSize) {
        bufferPosition = bufferPosition % bufferSize;
    }

    Data newData{Entry {type, timestamp, value}};

    memcpy(buffer + bufferPosition, newData.bytes, ENTRY_SIZE);

    entriesInBuffer++;
    bufferPosition += ENTRY_SIZE;
}

void setup() {
    Serial.begin(9600);
    pinMode(10, OUTPUT);

    if (!SD.begin(chipPin)) {
        Serial.println("Failed to initialize SD card");
        fail();
    }

    buffer = (uint8_t *) malloc(bufferSize);

    logFile = SD.open("DATA.BINLOG", FILE_WRITE);
    if (!logFile) {
        Serial.println("Failed to open file");
        fail();
    }

    for (int i = 0; i < 8; i++) {
        logFile.write(0xff);
    }

    // TODO: initial readings from sensors and writings
    Serial.println("Initialized");
}

void loop() {
    unsigned long timeMs = millis();

    if (timeMs - lastMillis > sensorUpdateMillis) {
        // Repeat for each value
        float gyroValue = 0.0; // Read value however we do it
        if (abs(lastWrittenGyro - gyroValue) > epoch) {
            writeFloat(GYRO_ANGLE, timeMs, gyroValue);

            lastWrittenGyro = gyroValue;
        }
    }

    unsigned int ableToWrite = logFile.availableForWrite();
    // Make sure we can write a full entry
    if (ableToWrite >= ENTRY_SIZE && entriesInBuffer > 0) {
        // Only write a full entry, no partial entries
        unsigned int entriesToWrite = min(ableToWrite / ENTRY_SIZE, entriesInBuffer);
        Serial.println(entriesToWrite);
        unsigned int bytesToWrite = entriesToWrite * ENTRY_SIZE;

        // If we have to wrap to the beginning of the buffer
        if (nextToWriteBufferPosition + bytesToWrite >= bufferSize) {
            unsigned int toWriteToWrap = (bufferSize - 1) - nextToWriteBufferPosition;
            logFile.write(buffer + nextToWriteBufferPosition, toWriteToWrap);
            nextToWriteBufferPosition = 0;
            bytesToWrite -= toWriteToWrap;
        }
        logFile.write(buffer + nextToWriteBufferPosition, bytesToWrite);
        nextToWriteBufferPosition += bytesToWrite;
        entriesInBuffer -= entriesToWrite;

        logFile.flush();

        delay(5);
    }
}
