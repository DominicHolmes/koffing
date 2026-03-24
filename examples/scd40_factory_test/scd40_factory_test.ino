// SCD40 Official Factory Test
// Uses the standard Sensirion I2C SCD4x library to demonstrate hardware failure.
// Compile: arduino-cli compile --fqbn arduino:esp32:nano_nora examples/scd40_factory_test
// Upload:  arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora examples/scd40_factory_test

#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

SensirionI2cScd4x scd4x;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);
    Serial.println("\n\n--- SCD40 Official Factory Test ---");

    Wire.begin();
    scd4x.begin(Wire);

    uint16_t error;
    char errorMessage[256];

    // 1. Stop any current measurement
    Serial.print("Stopping measurement... ");
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.print("Error: "); Serial.println(errorMessage);
    } else {
        Serial.println("OK");
    }
    delay(500);

    // 2. Read Serial Number
    uint16_t serial0, serial1, serial2;
    error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (!error) {
        Serial.print("Sensor Serial Number: 0x");
        Serial.print(serial0, HEX); Serial.print(serial1, HEX); Serial.println(serial2, HEX);
    }

    // 3. Perform Self Test (The "Smoking Gun")
    Serial.println("Starting Self Test (takes ~10 seconds)...");
    uint16_t selfTestResult;
    error = scd4x.performSelfTest(selfTestResult);
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.print("I2C Error during Self Test: "); Serial.println(errorMessage);
    } else if (selfTestResult != 0) {
        Serial.print("SELF TEST FAILED! Malfunction Code: 0x");
        Serial.println(selfTestResult, HEX);
        Serial.println("NOTE: Sensirion/SparkFun/Adafruit support consider any non-zero value a hardware defect.");
    } else {
        Serial.println("Self Test Passed (0x0000)");
    }

    // 4. Start Periodic Measurement
    Serial.print("Starting Periodic Measurement... ");
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.print("Error: "); Serial.println(errorMessage);
    } else {
        Serial.println("OK");
    }

    Serial.println("\nWaiting for measurements (5s interval)...");
}

void loop() {
    uint16_t error;
    char errorMessage[256];
    uint16_t co2;
    float temp, hum;

    delay(5000);

    error = scd4x.readMeasurement(co2, temp, hum);
    if (error) {
        errorToString(error, errorMessage, 256);
        Serial.print("Read Error: "); Serial.println(errorMessage);
    } else if (co2 == 0) {
        Serial.println("Incomplete sample (co2=0), skipping...");
    } else {
        Serial.print("CO2: "); Serial.print(co2);
        Serial.print(" ppm | Temp: "); Serial.print(temp);
        Serial.print(" C | Hum: "); Serial.print(hum);
        Serial.println(" %");
    }
}
