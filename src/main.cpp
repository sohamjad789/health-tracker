#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

Adafruit_MPU6050 mpu;
bool imuReady = false;

float simulatedHeartRate = 70.0;
unsigned long lastSample = 0;
unsigned long lastHeartRate = 0;
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BOOT: firmware started");
  Wire.begin(21, 22);

  imuReady = mpu.begin(0x68);
  if (imuReady) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.println("MPU6050 connected");
  } else {
    Serial.println("MPU6050 not found; continuing diagnostics");
  }

  Serial.println("Fitness band simulation ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastHeartbeat >= 1000) {
    lastHeartbeat = now;
    Serial.println("HEARTBEAT: loop running");
  }

  if (now - lastHeartRate >= 1000) {
    lastHeartRate = now;

    // Fake heart rate: 70–120 BPM
    simulatedHeartRate =
      95.0 + 25.0 * sin(now / 5000.0);

    Serial.print("Heart rate: ");
    Serial.print(simulatedHeartRate);
    Serial.println(" BPM");
  }

  if (now - lastSample >= 500) {
    lastSample = now;

    if (!imuReady) {
      Serial.println("Waiting for MPU6050...");
      return;
    }

    sensors_event_t acceleration;
    sensors_event_t gyro;
    sensors_event_t temperature;

    mpu.getEvent(&acceleration, &gyro, &temperature);

    float ax = acceleration.acceleration.x / 9.80665;
    float ay = acceleration.acceleration.y / 9.80665;
    float az = acceleration.acceleration.z / 9.80665;

    float motionMagnitude =
      sqrt(ax * ax + ay * ay + az * az);

    // Simple motion estimate.
    float motionIntensity = fabs(motionMagnitude - 1.0);

    // Placeholder calorie estimate.
    // Replace with your Keytel/MET formula later.
    float caloriesPerMinute =
      0.01 * simulatedHeartRate + 2.0 * motionIntensity;

    Serial.print("Accel g: ");
    Serial.print(ax, 2);
    Serial.print(", ");
    Serial.print(ay, 2);
    Serial.print(", ");
    Serial.println(az, 2);

    Serial.print("Motion intensity: ");
    Serial.println(motionIntensity, 2);

    Serial.print("Calories/min: ");
    Serial.println(caloriesPerMinute, 2);

    Serial.println("---");
  }
}
