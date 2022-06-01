#include "Adafruit_LSM6DS33.h"

class Readable_Adafruit_LSM6DS33 : public Adafruit_LSM6DS33 {
public:
    void read() {
        _read();
    }
};
