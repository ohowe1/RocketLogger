#include "LaunchDetector.h"

void LaunchDetector::update(float xAcceleration, float yAcceleration, float zAcceleration) {
    if (hasLaunched()) {
        return;
    }
    xAccelBuffer[currentPosition] = xAcceleration;
    yAccelBuffer[currentPosition] = yAcceleration;
    zAccelBuffer[currentPosition] = zAcceleration;

    currentPosition++;
    if (!fullBuffer) {
        if (currentPosition >= BUFFER_SIZE) {
            fullBuffer = true;
        } else {
            return;
        }
    }
    currentPosition %= BUFFER_SIZE;

    float xAveraged = average(xAccelBuffer, BUFFER_SIZE);
    float yAveraged = average(yAccelBuffer, BUFFER_SIZE);
    float zAveraged = average(zAccelBuffer, BUFFER_SIZE);

    float magnitudeSquared = sq(xAveraged) + sq(yAveraged) + sq(zAveraged);

    launched = magnitudeSquared >= sq(4 * 9.8);
    if (launched) {
        launchTimestamp = millis();
        tone(46, 500, 10000);
    }
}
