#pragma once

// ================================================================
// PIN DEFINITIONS — fill in once GPIO allocation is finalized
// ================================================================

// --- BTS7960 Motor Driver ---
#define PIN_MOTOR_RPWM      14 // proposed >>> 14    !! // Old 6
#define PIN_MOTOR_LPWM      13 // proposed >>> 13    !! // Old 5
#define PIN_MOTOR_R_IS      6 // Proposed >>> 6    !! // Old 7
#define PIN_MOTOR_L_IS      7 // proposed >>> 7    !! // Old 16

// --- Buzzer ---
#define PIN_BUZZER          5  // proposed >>> 5   !! // Old 2

// --- DHT22 (Indoor Temp/Humidity) ---
#define PIN_DHT22           1 //

// --- Linear Actuator Potentiometer ---
#define PIN_LA_POT          4 // TEST! originally 1  // analog pin for position feedback

// --- Rotary Encoder ---
#define PIN_ENC_CLK         9 // proposed >>> 9 !! // Old 14
#define PIN_ENC_DT          8 // proposed >>> 8    !! // Old 15
#define PIN_ENC_SW          18 //   !! // Old 18

// --- Emergency Stop ---
#define PIN_ESTOP           10      // proposed >>> 12  !! // hardware e-stop GPIO read pin // Old 17

// --- I2C LCD ---
#define PIN_SDA             11 // proposed >>> 10    !! Old 8
#define PIN_SCL             12 // proposed >>> 11    !! Old 9

// --- SD Card (SPI) ---
#define PIN_SD_MOSI         17 // proposed >>> 17   !! Old 11
#define PIN_SD_MISO         2 // proposed >>> 2    !! Old 13
#define PIN_SD_SCK          16 // proposed >>> 16  !! Old 12
#define PIN_SD_CS           15

// proposed >>> 15 !! Old 10

// ================================================================
// TEMPERATURE THRESHOLDS
// ================================================================
#define TEMP_TARGET         25.0f   // desired indoor temperature (°C)
#define TEMP_RANGE          2.0f    // ± range around target
#define TEMP_UPPER          (TEMP_TARGET + TEMP_RANGE)  // 27°C — start closing
#define TEMP_LOWER          (TEMP_TARGET - TEMP_RANGE)  // 23°C — start opening

// outdoor must be this much cooler than indoor to justify opening
#define TEMP_OUTDOOR_DIFF   2.0f

// ================================================================
// SETTINGS BOUNDS (for encoder menu)
// ================================================================
#define TEMP_TARGET_MIN     16.0f   // °C
#define TEMP_TARGET_MAX     32.0f   // °C
#define TEMP_TARGET_STEP    1.0f    // increment per encoder tick

#define TEMP_RANGE_MIN      0.5f    // °C
#define TEMP_RANGE_MAX      5.0f    // °C
#define TEMP_RANGE_STEP     0.5f    // increment per encoder tick


// ================================================================
// NATURAL EVENT THRESHOLDS
// ================================================================
#define WIND_SPEED_MAX      30.0f   // km/h — above this, force close
// rain is binary — if rain sensor triggers, force close

// ================================================================
// MOTOR / POSITION
// ================================================================
#define MOTOR_PWM_FREQ      1000    // Hz
#define MOTOR_PWM_CHANNEL   0
#define MOTOR_PWM_RESOLUTION 8      // bits (0-255)
#define MOTOR_MIN_PWM       100     // minimum PWM to actually move motor
#define MOTOR_MAX_PWM       128    // DEBUG (orig 255)

#define WINDOW_POS_MIN    0     // display min
#define WINDOW_POS_MAX    100   // display max
#define WINDOW_PHYS_MIN   22    // actual potentiometer lower limit
#define WINDOW_PHYS_MAX   71    // actual potentiometer upper limit

#define OVERCURRENT_THRESHOLD   1.5f    // volts
#define OVERCURRENT_RETRY_MS    5000    // 5 seconds
// ================================================================
// ENCODER
// ================================================================
#define ENCODER_STEPS_PER_PERCENT  2    // tune after testing
#define ESTOP_RESET_HOLD_MS        3000 // hold encoder button 3s to reset e-stop

// ================================================================
// TIMING
// ================================================================
#define SENSOR_POLL_INTERVAL_MS    2000   // how often to read DHT22
#define ESPNOW_TIMEOUT_MS          10000  // if no data received in 10s, flag as stale
#define LOG_INTERVAL_MS            5000   // how often to write to SD card

// ================================================================
// MODES
// ================================================================
enum SystemMode {
    MODE_AUTOMATIC  = 0,
    MODE_MANUAL     = 1,
    MODE_LOCK       = 2,
    MODE_IP         = 3
};

// ================================================================
// STATES
// ================================================================
enum SystemState {
    STATE_IDLE = 0,
    STATE_ADJUSTING,
    STATE_FULLY_OPEN,
    STATE_FULLY_CLOSED,
    STATE_FORCE_CLOSING,
    STATE_EMERGENCY_STOP
};

// ================================================================
// UI STATES DISPLAY (FOR LCD)
// ================================================================
enum UIState {
    UI_MANUAL,
    UI_SELECT_MODE,
    UI_SET_TEMP_TARGET,
    UI_SET_TEMP_RANGE,
    UI_AUTOMATIC,
    UI_LOCK,
    UI_EMERGENCY_STOP,
    UI_OVERCURRENT,
    UI_RAIN_WIND_ALERT,
    UI_IP_ADDRESS
};

// ================================================================
// SENSORS
// ================================================================
#define DHT_SENSOR_TYPE     DHT_TYPE_22  // change to DHT_TYPE_22 if you switch
#define POT_ADC_MIN         0            // ADC value at fully closed
#define POT_ADC_MAX         4095         // ADC value at fully open
#define POT_REVERSE         false        // flip to true if pot reads backwards
#define ESTOP_ACTIVE_LOW    true

// ================================================================
// DEFAULT PASSWORDS (first boot, before admin sets them)
// ================================================================
#define DEFAULT_USER_PASSWORD     "user1234"
#define DEFAULT_TEACHER_PASSWORD  "teacher1234"

// ================================================================
// AUTO MODE POSITIONS
// ================================================================
#define AUTO_POS_CLOSE    0
#define AUTO_POS_MID      50
#define AUTO_POS_OPEN     100
#define AUTO_TEMP_MID     2.5f   // excess °C to go mid

// ================================================================
// NTP (for logger timestamps)
// ================================================================
#define NTP_SERVER      "pool.ntp.org"
#define NTP_UTC_OFFSET  43200    // UTC+12 for Auckland (seconds)
#define NTP_DST_OFFSET  0        // daylight saving handled by NTP