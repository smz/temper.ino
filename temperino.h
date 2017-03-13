#include <time.h>

// DEBUG
//  0 No debug
//  1 Debug actions
//  2 Debug actions and Rotary Encoder
//  3 Debug actions, Rotary Encoder and scheduler
// 99 Debug all
#define DEBUG 3
#include "debug.h"

// Project configuration parameters
#define DS3231
#undef  DS3231_TEMP
#define MCP9808_TEMP
#define RESET_RTC_TIME
#undef  RTC_SOFTWARE_WIRE


// Connections
#define LCD_PINS  7,  8,  9, 10, 11, 12
//      LCD_PINS RS, EN, D4, D5, D6, D7
#define ENCODER_PINS   2,  3,  4
//      ENCODER_PINS CLK, DT, SW
#define RTC_SOFTWARE_WIRE_SDA SDA
#define RTC_SOFTWARE_WIRE_SCL SCL
#define VALVE_PIN 13


// Operative parameters
#define SERIAL_SPEED 57600
#define TIMEZONE (1 * ONE_HOUR)
#define TEMP_HYSTERESIS 0.5
#define POLLING_TIME 1000
#define ENCODER_TIMER 1000
#define TEMP_INCREMENT 0.5
#define OVERRIDE_TIME_MAX 86400
#define OVERRIDE_TIME_INCREMENT 300
#define VALVE_ACTIVATION_TIME 15
#define TEMP_MIN 5.0
#define TEMP_MAX 25.0
#define MAX_WEEKLY_STEPS 70
#define MCP9808_TEMP_RESOLUTION 0x03
#define MCP9808_I2C_ADDRESS 0x18


// Global variables
#if DEBUG > 90
  unsigned long loops = 0;
  #define PrintLoops() {           \
      Serial.print(timestamp);     \
      Serial.print(F(" Loops: ")); \
      Serial.println(loops);       \
      loops = 0;                   \
      }
#endif
float temperature;
bool valve_target;
bool valve_status;
time_t prev_valve_time;
char timestamp[20];
unsigned long prev_millis = 0;
time_t now;
struct tm now_tm;
struct tm temp_tm;
uint16_t now_tow;

// Schedule table
struct {uint16_t tow; float temperature;} schedule[MAX_WEEKLY_STEPS];
uint8_t current_step = MAX_WEEKLY_STEPS + 1;


// LCD
#include <LiquidCrystal.h>
char lcd_line1[17];
char lcd_line2[17];
LiquidCrystal lcd(LCD_PINS);


// TWI/I2C interface
#ifdef RTC_SOFTWARE_WIRE
  #include <SoftwareWire.h>
  #define myWire SoftwareWire
  myWire I2C(RTC_SOFTWARE_WIRE_SDA, RTC_SOFTWARE_WIRE_SCL);
#else
  #include <Wire.h>
  #define myWire TwoWire
  #define I2C Wire
#endif


// RTC
#ifdef DS3231
  #include <RtcDS3231.h>
  RtcDS3231<myWire> Rtc(I2C);
#else
  #include <RtcDS1307.h>
  RtcDS1307<myWire> Rtc(I2C);
#endif

// This is for the str20ToTime() function used to bootstrap the RTC in case it has an invalid time
#include "RTCtimeUtils.h"


// Temperature sensor
#ifdef MCP9808_TEMP
  #include "MCP9808sensor.h"
  MCP9808sensor<myWire> tempsensor(I2C);
#endif


// Rotary Encoder
// Uses the ClickEncoder library by 0xPIT
// See: https://github.com/0xPIT/encoder
// ClickEncoder needs the Timer1 library (see: http://playground.arduino.cc/Code/Timer1)
// as implemented by Paul Stoffregen (https://github.com/PaulStoffregen/TimerOne)
#include <TimerOne.h>
#include <ClickEncoder.h>
ClickEncoder *encoder;
void timerIsr()
{
  encoder->service();
}


typedef (EncoderRotatedFunction_t)(int16_t value);
typedef (DisplayFunction_t)();
typedef (ButtonFunction_t)();

struct EncoderHandler_t
{
  float value;
  float Min;
  float Max;
  float Increment;
  EncoderRotatedFunction_t *EncoderRotatedFunction;
  DisplayFunction_t *DisplayFunction;
  ButtonFunction_t *ButtonClickedFunction;
  ButtonFunction_t *ButtonDoubleClickedFunction;
  ButtonFunction_t *ButtonHeldFunction;
  ButtonFunction_t *ButtonReleasedFunction;
};

struct EncoderHandler_t TemperatureHandler;
struct EncoderHandler_t OverrideTimeHandler;
struct EncoderHandler_t SetYearHandler;
struct EncoderHandler_t SetMonthHandler;
struct EncoderHandler_t SetDayHandler;
struct EncoderHandler_t SetHoursHandler;
struct EncoderHandler_t SetMinutesHandler;
struct EncoderHandler_t SetSecondsHandler;
struct EncoderHandler_t *ActiveHandler = &TemperatureHandler;
