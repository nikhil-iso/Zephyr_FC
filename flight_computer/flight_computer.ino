#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <math.h>

// ============================================================
// Desktop Flight Computer Proof-of-Concept
// Hardware:
//   - Arduino Micro
//   - GY-61 (ADXL335 analog accelerometer)
//   - BMP280 (I2C barometer)
//   - Push button on pin 2 using INPUT_PULLUP
//
// Serial CSV output format:
// timestamp_ms,state,altitude_m,temperature_C,accelX_g,accelY_g,accelZ_g,pitch_deg,roll_deg,current_max_altitude_m
//
// States:
//   0 = IDLE
//   1 = ASCENT
//   2 = DESCENT
// ============================================================

// -----------------------------
// Pin definitions
// -----------------------------
const int PIN_ACCEL_X = A2;
const int PIN_ACCEL_Y = A1;
const int PIN_ACCEL_Z = A0;
const int PIN_BUTTON  = 10;

// -----------------------------
// BMP280 object
// -----------------------------
Adafruit_BMP280 bmp;

// -----------------------------
// State machine
// -----------------------------
enum FlightState
{
  IDLE = 0,
  ASCENT = 1,
  DESCENT = 2
};

FlightState state = IDLE;

// -----------------------------
// Timing
// -----------------------------
const unsigned long LOG_INTERVAL_MS = 50;   // 20 Hz
unsigned long lastLogTime = 0;

// -----------------------------
// Button debounce
// -----------------------------
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 30;

// -----------------------------
// Accelerometer calibration
// ADXL335 typical:
//   0 g bias around mid-supply
//   sensitivity about 300 mV/g
//
// Using Arduino 10-bit ADC with 5 V reference:
//   ADC volts = reading * (5.0 / 1023.0)
//
// Then:
//   accel_g = (voltage - zero_g_voltage) / 0.300
//
// For this proof-of-concept, we estimate zero-g baselines from 100 samples.
// For Z, since the board is stationary on the desk, the measured average includes
// gravity. We store the baseline as-is, and later compute motion-friendly values
// by re-centering relative to the desk orientation.
// -----------------------------
float accelXOffsetVolts = 0.0;
float accelYOffsetVolts = 0.0;
float accelZOffsetVolts = 0.0;

// -----------------------------
// Altitude reference
// -----------------------------
float groundAltitude = 0.0;

// -----------------------------
// Flight/apogee tracking
// -----------------------------
float currentMaxAltitude = 0.0;
unsigned long launchTimeMs = 0;

// Apogee detection settings
const float APOGEE_DROP_THRESHOLD_M = 0.25;  // declare apogee after drop exceeds this
const int APOGEE_CONFIRM_SAMPLES = 3;        // require consecutive samples below peak threshold
int apogeeDropCounter = 0;

// Landing detection settings
const float LANDING_ALTITUDE_BAND_M = 0.50;  // must be within +/- this around ground level
const int LANDING_WINDOW_SAMPLES = 20;       // 1 second at 20 Hz
const float LANDING_STABILITY_RANGE_M = 0.15;

// Store recent altitude samples for landing detection
float landingBuffer[LANDING_WINDOW_SAMPLES];
int landingBufferIndex = 0;
bool landingBufferFilled = false;

// -----------------------------
// Helper function prototypes
// -----------------------------
void calibrateAccelerometer();
float adcToVolts(int adcValue);
void readAccelerometer(float &ax_g, float &ay_g, float &az_g, float &pitch_deg, float &roll_deg);
bool buttonPressedEvent();
void resetFlightVariables();
void updateLandingBuffer(float altitude);
bool landingDetected();
void printCSVHeader();

// ============================================================
// SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Initialize BMP280
  bool bmpOK = bmp.begin(0x76);
  if (!bmpOK)
  {
    // Some BMP280 boards use 0x77 instead
    bmpOK = bmp.begin(0x77);
  }

  if (!bmpOK)
  {
    Serial.println("ERROR: BMP280 not found. Check wiring/address.");
    while (1)
    {
      delay(1000);
    }
  }

  // Give sensors a moment to settle
  delay(500);

  // Calibrate accelerometer on desk
  calibrateAccelerometer();

  // Set altitude reference so relative altitude starts at 0 m
  groundAltitude = bmp.readAltitude(1013.25);

  // Initialize landing buffer
  for (int i = 0; i < LANDING_WINDOW_SAMPLES; i++)
  {
    landingBuffer[i] = 0.0;
  }

  printCSVHeader();
}

// ============================================================
// LOOP
// ============================================================
void loop()
{
  unsigned long now = millis();

  // Check for button press event
  if (buttonPressedEvent())
  {
    if (state == IDLE)
    {
      // Button acts as launch commit
      resetFlightVariables();
      launchTimeMs = now;
      state = ASCENT;
    }
  }

  // Log and process at 20 Hz
  if (now - lastLogTime >= LOG_INTERVAL_MS)
  {
    lastLogTime = now;

    // -----------------------------
    // Read sensors
    // -----------------------------
    float temperature_C = bmp.readTemperature();
    float absoluteAltitude = bmp.readAltitude(1013.25);
    float relativeAltitude = absoluteAltitude - groundAltitude;

    float ax_g, ay_g, az_g;
    float pitch_deg, roll_deg;
    readAccelerometer(ax_g, ay_g, az_g, pitch_deg, roll_deg);

    // -----------------------------
    // State machine logic
    // -----------------------------
    if (state == ASCENT)
    {
      // Track highest altitude seen so far
      if (relativeAltitude > currentMaxAltitude)
      {
        currentMaxAltitude = relativeAltitude;
        apogeeDropCounter = 0; // reset because we found a new peak
      }
      else
      {
        // Check whether altitude has dropped enough from peak to declare apogee
        float dropFromPeak = currentMaxAltitude - relativeAltitude;

        if (dropFromPeak > APOGEE_DROP_THRESHOLD_M)
        {
          apogeeDropCounter++;
        }
        else
        {
          apogeeDropCounter = 0;
        }

        // Require several consecutive below-peak samples to avoid false trigger
        if (apogeeDropCounter >= APOGEE_CONFIRM_SAMPLES)
        {
          state = DESCENT;
          apogeeDropCounter = 0;
        }
      }
    }
    else if (state == DESCENT)
    {
      // Continue updating landing detection buffer
      updateLandingBuffer(relativeAltitude);

      if (landingDetected())
      {
        state = IDLE;
      }
    }
    else
    {
      // In IDLE, keep the landing buffer aligned to current desk altitude
      // This also helps if the local pressure drifts slightly over time.
      updateLandingBuffer(relativeAltitude);
    }

    // -----------------------------
    // CSV output
    // -----------------------------
    Serial.print(now);
    Serial.print(",");
    Serial.print((int)state);
    Serial.print(",");
    Serial.print(relativeAltitude, 3);
    Serial.print(",");
    Serial.print(temperature_C, 2);
    Serial.print(",");
    Serial.print(ax_g, 3);
    Serial.print(",");
    Serial.print(ay_g, 3);
    Serial.print(",");
    Serial.print(az_g, 3);
    Serial.print(",");
    Serial.print(pitch_deg, 2);
    Serial.print(",");
    Serial.print(roll_deg, 2);
    Serial.print(",");
    Serial.println(currentMaxAltitude, 3);
  }
}

// ============================================================
// FUNCTIONS
// ============================================================

// Print CSV header so Python can parse columns easily
void printCSVHeader()
{
  Serial.println("timestamp_ms,state,altitude_m,temperature_C,accelX_g,accelY_g,accelZ_g,pitch_deg,roll_deg,current_max_altitude_m");
}

// Convert 10-bit ADC count to volts
float adcToVolts(int adcValue)
{
  return adcValue * (5.0 / 1023.0);
}

// Take 100 samples while stationary to estimate axis baseline voltages
void calibrateAccelerometer()
{
  const int N = 100;
  float sumX = 0.0;
  float sumY = 0.0;
  float sumZ = 0.0;

  for (int i = 0; i < N; i++)
  {
    sumX += adcToVolts(analogRead(PIN_ACCEL_X));
    sumY += adcToVolts(analogRead(PIN_ACCEL_Y));
    sumZ += adcToVolts(analogRead(PIN_ACCEL_Z));
    delay(10);
  }

  accelXOffsetVolts = sumX / N;
  accelYOffsetVolts = sumY / N;
  accelZOffsetVolts = sumZ / N;
}

// Read accelerometer and compute pitch/roll
void readAccelerometer(float &ax_g, float &ay_g, float &az_g, float &pitch_deg, float &roll_deg)
{
  // Sensitivity for ADXL335 is approximately 300 mV/g
  const float SENSITIVITY_V_PER_G = 0.300;

  float vx = adcToVolts(analogRead(PIN_ACCEL_X));
  float vy = adcToVolts(analogRead(PIN_ACCEL_Y));
  float vz = adcToVolts(analogRead(PIN_ACCEL_Z));

  // Recenter around desk reference.
  // Since desk reference includes gravity, x/y are near 0 g and z is near +1 g
  // for a board that starts approximately flat.
  //
  // This is not a full physical calibration, but it is simple and works well
  // for a desktop orientation demo.
  ax_g = (vx - accelXOffsetVolts) / SENSITIVITY_V_PER_G;
  ay_g = (vy - accelYOffsetVolts) / SENSITIVITY_V_PER_G;

  // Make stationary desk condition approximately +1 g on Z
  az_g = 1.0 + (vz - accelZOffsetVolts) / SENSITIVITY_V_PER_G;

  // Pitch and roll from accelerometer only
  //
  // Common formulas:
  // pitch = atan2(ax, sqrt(ay^2 + az^2))
  // roll  = atan2(ay, sqrt(ax^2 + az^2))
  //
  // Convert radians to degrees:
  // degrees = radians * 180 / pi
  pitch_deg = atan2(ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0 / PI;
  roll_deg  = atan2(ay_g, sqrt(ax_g * ax_g + az_g * az_g)) * 180.0 / PI;
}

// Debounced button press detection
bool buttonPressedEvent()
{
  bool reading = digitalRead(PIN_BUTTON);
  bool pressedEvent = false;

  if (reading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS)
  {
    if (reading != stableButtonState)
    {
      stableButtonState = reading;

      // Since INPUT_PULLUP is used, LOW means button pressed
      if (stableButtonState == LOW)
      {
        pressedEvent = true;
      }
    }
  }

  lastButtonReading = reading;
  return pressedEvent;
}

// Reset variables at flight start
void resetFlightVariables()
{
  currentMaxAltitude = 0.0;
  apogeeDropCounter = 0;

  for (int i = 0; i < LANDING_WINDOW_SAMPLES; i++)
  {
    landingBuffer[i] = 0.0;
  }

  landingBufferIndex = 0;
  landingBufferFilled = false;

  // Re-zero altitude at launch start so each hand-flight starts from ~0
  groundAltitude = bmp.readAltitude(1013.25);
}

// Store recent altitude samples for landing stability check
void updateLandingBuffer(float altitude)
{
  landingBuffer[landingBufferIndex] = altitude;
  landingBufferIndex++;

  if (landingBufferIndex >= LANDING_WINDOW_SAMPLES)
  {
    landingBufferIndex = 0;
    landingBufferFilled = true;
  }
}

// Landing is detected when:
// 1. buffer is full
// 2. all recent altitudes stay close to ground
// 3. variation over that window is small
bool landingDetected()
{
  if (!landingBufferFilled)
  {
    return false;
  }

  float minAlt = landingBuffer[0];
  float maxAlt = landingBuffer[0];

  for (int i = 0; i < LANDING_WINDOW_SAMPLES; i++)
  {
    float h = landingBuffer[i];

    // Must remain near ground
    if (fabs(h) > LANDING_ALTITUDE_BAND_M)
    {
      return false;
    }

    if (h < minAlt) minAlt = h;
    if (h > maxAlt) maxAlt = h;
  }

  // Must be stable over the full window
  if ((maxAlt - minAlt) < LANDING_STABILITY_RANGE_M)
  {
    return true;
  }

  return false;
}