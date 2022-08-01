#pragma once
#include "Arduino.h"

#define BUFFER_SIZE 30
class LaunchDetector {
public:
    void update(float xAcceleration, float yAcceleration, float zAcceleration);
    bool hasLaunched() const {
        return launched;
    }
    bool hasLanded() const {
        // 5 minutes after launch
        return hasLaunched() && (millis() - launchTimestamp) >= 5 * 60 * 1000;
    }
protected:
    bool launched = false;
    unsigned long launchTimestamp{};
    bool fullBuffer = false;
    int currentPosition = 0;
    float xAccelBuffer[BUFFER_SIZE]{};
    float yAccelBuffer[BUFFER_SIZE]{};
    float zAccelBuffer[BUFFER_SIZE]{};

    float static average(float* buffer, size_t size) {
        float sum = 0;
        for (int i = 0; i < size; ++i) {
            sum += buffer[i];
        }

        return sum / ((float) size);
    }
};
