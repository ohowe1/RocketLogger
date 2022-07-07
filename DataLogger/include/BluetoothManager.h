#pragma once
#include "bluefruit.h"

#define DEVICE_NAME "RAIDERS Cone"
namespace bluetooth {
    uint8_t beaconUuid[16] = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
                              0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0};
    BLEBeacon *beacon = new BLEBeacon(beaconUuid, 0x0102, 0x0304, -54);

    void startBluetooth() {
        Bluefruit.begin();

        beacon->setManufacturer(0x004C);

        Bluefruit.Advertising.setBeacon(*beacon);
#ifdef DEVICE_NAME
        Bluefruit.setName(DEVICE_NAME);
#else
        Bluefruit.setName("RJHS Device");
#endif
        Bluefruit.ScanResponse.addName();
        Bluefruit.Advertising.restartOnDisconnect(true);
        Bluefruit.Advertising.setInterval(160, 160);
        Bluefruit.Advertising.setFastTimeout(30);
        Bluefruit.Advertising.start(0);
    }
}