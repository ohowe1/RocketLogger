#include "Arduino.h"
#include "Parachute.h"

#define SERVO_PIN 1

void Parachute::setup(float pressure) {
    startAltitude = calculateAltitude(pressure / 100);

    servo->attach(SERVO_PIN);
    servo->write(45);
}

void Parachute::update(float pressure, float accelerationZMetersSecond) {
    if (hasDeployed) return;
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdateMs >= updateTimeMs) {
        float currentRelativeAltitude = calculateAltitude(pressure / 100) - startAltitude;
        if (inDescentIterations >= 10) {
            if (currentRelativeAltitude < PARACHUTE_DEPLOY_ALTITUDE_ABOVE_GROUND) {
                deployParachute();
                return;
            }
        }
        if (currentRelativeAltitude < lastAltitude) {
            inDescentIterations++;
        } else {
            if (inDescentIterations > 0) {
                inDescentIterations--;
            }
        }

        lastAltitude = currentRelativeAltitude;
        lastUpdateMs = currentMillis;
    }
}

void Parachute::deployParachute() {
    hasDeployed = true;
    servo->write(135);
    Serial.println("Parachute deployed");
}

