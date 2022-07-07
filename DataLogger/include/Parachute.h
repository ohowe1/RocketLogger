#pragma once
#include "Servo.h"

#define PARACHUTE_DEPLOY_ALTITUDE_ABOVE_GROUND 500

class Parachute {
public:
    void setup(float pressure);
    void update(float pressure, float accelerationZMetersSecond);
    void deployParachute();
private:
    static const unsigned long updateTimeMs = 1000;
    /**
     * @param pressure the pressure in hPa
     * @return the estimated altitude in feet
     */
    static float calculateAltitude(float pressure) {
        return 44330 * 3.28084 * (1.0 - pow(pressure / 1013.25, 0.1903));
    }
    bool hasDeployed = false;
    int inDescentIterations = 0;
    float startAltitude = 0;
    float lastAltitude = 0;
    unsigned long lastUpdateMs = 0;
    Servo* servo = new Servo();
};
