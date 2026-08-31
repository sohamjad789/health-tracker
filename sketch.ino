/*
  Screenless Wearable Health Tracker - Wokwi Simulation Firmware

  Wokwi currently has no official nRF52840 board simulation. The ESP32 is a
  simulator-only stand-in for the IMU, motion and calorie pipeline. When this
  source is compiled under the Adafruit nRF52 board package, the guarded code
  below uses the standard Bluefruit Heart Rate Service (UUID 0x180D).
*/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

#if defined(ARDUINO_ARCH_NRF52)
#include <bluefruit.h>
#define HAS_BLUEFRUIT 1
#else
#define HAS_BLUEFRUIT 0
#endif

// ---------------- User-adjustable profile ----------------
enum Sex { FEMALE, MALE };
const Sex USER_SEX = MALE;
const float USER_WEIGHT_KG = 75.0f;
const uint8_t USER_AGE_YEARS = 28;
const uint8_t RESTING_HR_BPM = 60;

const uint32_t HR_UPDATE_MS = 1500;
const uint32_t CALORIE_UPDATE_MS = 1000;
const uint8_t ENMO_WINDOW = 12;

Adafruit_MPU6050 mpu;
bool imuReady = false;
float heartRateBpm = RESTING_HR_BPM;
float motionEnmoG = 0.0f;
float totalCaloriesKcal = 0.0f;
float enmoSamples[ENMO_WINDOW] = {0.0f};
float enmoSum = 0.0f;
uint8_t enmoNext = 0;
uint8_t enmoCount = 0;
unsigned long lastHrMs = 0;
unsigned long lastCalorieMs = 0;

#if HAS_BLUEFRUIT
BLEService heartRateService(UUID16_SVC_HEART_RATE);
BLECharacteristic heartRateMeasurement(UUID16_CHR_HEART_RATE_MEASUREMENT);
#endif

bool startImu();
float readSmoothedEnmoG();
float generateSyntheticHeartRateBpm();
float bmrKcalPerMinute();
float keytelKcalPerMinute(float hrBpm);
float motionKcalPerMinute(float enmoG);
float fusedKcalPerMinute(float hrBpm, float enmoG);
void updateCalories(unsigned long nowMs);
void publishHeartRate(uint8_t bpm);
#if HAS_BLUEFRUIT
void startHeartRateBle();
void startAdvertising();
void connectCallback(uint16_t handle);
void disconnectCallback(uint16_t handle, uint8_t reason);
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Health tracker simulation starting");

  // ESP32/Wokwi I2C pins. Use the XIAO board's I2C pins on physical hardware.
#if HAS_BLUEFRUIT
  Wire.begin();
#else
  Wire.begin(21, 22);
#endif
  imuReady = startImu();
  randomSeed(micros());

#if HAS_BLUEFRUIT
  Bluefruit.begin(1, 0);
  Bluefruit.setName("HealthTracker");
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);
  startHeartRateBle();
  startAdvertising();
  Serial.println("BLE Heart Rate Service advertising");
#else
  Serial.println("Wokwi BLE stub active: notifications will be printed");
#endif
}

void loop() {
  // Read the simulated MPU6050 on every pass through loop().
  motionEnmoG = readSmoothedEnmoG();
  const unsigned long nowMs = millis();

  if (nowMs - lastHrMs >= HR_UPDATE_MS) {
    lastHrMs = nowMs;
    // ===== FAKE DATA GENERATOR: replace with MAX30101/MAX32664 reading. =====
    heartRateBpm = generateSyntheticHeartRateBpm();
    // ========================================================================
    publishHeartRate((uint8_t)lroundf(heartRateBpm));
  }

  if (nowMs - lastCalorieMs >= CALORIE_UPDATE_MS) {
    lastCalorieMs = nowMs;
    updateCalories(nowMs);
    Serial.printf("HR: %.0f bpm | ENMO: %.3f g | burn: %.2f kcal/min | total: %.3f kcal\n",
                  heartRateBpm, motionEnmoG,
                  fusedKcalPerMinute(heartRateBpm, motionEnmoG), totalCaloriesKcal);
  }
  delay(25);
}

bool startImu() {
  if (!mpu.begin(0x68)) {
    Serial.println("ERROR: MPU6050 not found at 0x68");
    return false;
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 ready");
  return true;
}

float readSmoothedEnmoG() {
  if (!imuReady) return 0.0f;

  sensors_event_t accel, gyro, temperature;
  mpu.getEvent(&accel, &gyro, &temperature);
  const float axG = accel.acceleration.x / SENSORS_GRAVITY_STANDARD;
  const float ayG = accel.acceleration.y / SENSORS_GRAVITY_STANDARD;
  const float azG = accel.acceleration.z / SENSORS_GRAVITY_STANDARD;
  const float magnitudeG = sqrtf(axG * axG + ayG * ayG + azG * azG);
  const float enmoG = fmaxf(0.0f, magnitudeG - 1.0f);

  enmoSum -= enmoSamples[enmoNext];
  enmoSamples[enmoNext] = enmoG;
  enmoSum += enmoG;
  enmoNext = (enmoNext + 1) % ENMO_WINDOW;
  if (enmoCount < ENMO_WINDOW) enmoCount++;
  return enmoSum / enmoCount;
}

float generateSyntheticHeartRateBpm() {
  // ===== FAKE DATA GENERATOR: replace with MAX30101/MAX32664 reading. =====
  // Slow physiological-looking trend plus +/-3 bpm random measurement jitter.
  const float trend = 105.0f + 38.0f * sinf(millis() / 18000.0f);
  const float jitter = (float)random(-30, 31) / 10.0f;
  return constrain(trend + jitter, 60.0f, 160.0f);
  // ========================================================================
}

float bmrKcalPerMinute() {
  // Simple background basal burn; this is not a medical estimate.
  const float dailyBmr = USER_WEIGHT_KG * 24.0f * (USER_SEX == MALE ? 1.0f : 0.9f);
  return dailyBmr / 1440.0f;
}

float keytelKcalPerMinute(float hrBpm) {
  // Keytel-style exercise estimate; only used above the resting-HR threshold.
  const float kcal = USER_SEX == MALE
      ? (-55.0969f + 0.6309f * hrBpm + 0.1988f * USER_WEIGHT_KG + 0.2017f * USER_AGE_YEARS) / 4.184f
      : (-20.4022f + 0.4472f * hrBpm - 0.1263f * USER_WEIGHT_KG + 0.074f * USER_AGE_YEARS) / 4.184f;
  return fmaxf(0.0f, kcal);
}

float motionKcalPerMinute(float enmoG) {
  // Motion fallback: map ENMO to 1-7 METs and remove the resting portion.
  const float met = constrain(1.0f + 12.0f * enmoG, 1.0f, 7.0f);
  const float gross = met * 3.5f * USER_WEIGHT_KG / 200.0f;
  return fmaxf(0.0f, gross - bmrKcalPerMinute());
}

float fusedKcalPerMinute(float hrBpm, float enmoG) {
  const bool elevatedHr = hrBpm >= RESTING_HR_BPM + 8.0f;
  const float active = elevatedHr ? keytelKcalPerMinute(hrBpm) : motionKcalPerMinute(enmoG);
  return bmrKcalPerMinute() + active;
}

void updateCalories(unsigned long nowMs) {
  static unsigned long previousMs = nowMs;
  totalCaloriesKcal += fusedKcalPerMinute(heartRateBpm, motionEnmoG)
      * ((nowMs - previousMs) / 60000.0f);
  previousMs = nowMs;
}

void publishHeartRate(uint8_t bpm) {
  // Standard HRS payload: flags=0 means one following byte contains bpm.
  const uint8_t payload[] = {0x00, bpm};
#if HAS_BLUEFRUIT
  heartRateMeasurement.write(payload, sizeof(payload));
  if (Bluefruit.connected()) heartRateMeasurement.notify(payload, sizeof(payload));
#else
  // BLE radio is not simulated by Wokwi's ESP32 fallback.
  Serial.printf("SIM BLE HRS notify: flags=0x%02X bpm=%u\n", payload[0], payload[1]);
#endif
}

#if HAS_BLUEFRUIT
void startHeartRateBle() {
  heartRateService.begin();
  heartRateMeasurement.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  heartRateMeasurement.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  heartRateMeasurement.setFixedLen(2);
  heartRateMeasurement.begin();
  const uint8_t initial[] = {0x00, RESTING_HR_BPM};
  heartRateMeasurement.write(initial, sizeof(initial));
}

void startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(heartRateService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void connectCallback(uint16_t handle) {
  (void)handle;
  Serial.println("BLE central connected");
}

void disconnectCallback(uint16_t handle, uint8_t reason) {
  (void)handle;
  Serial.printf("BLE central disconnected: 0x%02X\n", reason);
}
#endif
