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
#define WITH_LCD
#define WITH_ENCODER
#define RESET_RTC_TIME
#undef  RTC_SOFTWARE_WIRE


// Connections
#define LCD_PINS  7,  8,  9, 10, 11, 12
//      LCD_PINS RS, EN, D4, D5, D6, D7
#define RTC_SOFTWARE_WIRE_SDA PIN_A0
#define RTC_SOFTWARE_WIRE_SCL PIN_A1
#define VALVE_PIN 13
#define ENCODER_PINS 2, 3, 4


// Operative parameters
#define SERIAL_SPEED 57600
#define TIMEZONE (1 * ONE_HOUR)
#define TEMP_HYSTERESIS 0.5
#define POLLING_TIME 1000
#define ENCODER_TIMER 1000
#define STEPS_PER_DEGREE 2
#define SECONDS_PER_STEP 600  // 10 minutes
#define MAX_OVERRIDE 86400    // 24 hours
#define VALVE_ACTIVATION_TIME 15
#define MIN_TEMP 5
#define MAX_TEMP 25
#define MAX_WEEKLY_STEPS 70


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
float setpoint;
float temperature;
bool valve_target;
bool valve_status;
time_t prev_valve_time;
char timestamp[20];
unsigned long prev_millis = 0;
time_t now;
struct tm now_tm;
uint16_t now_tow;
time_t override_t = 0;
 
// Schedule table
struct {uint16_t tow; float temperature;} schedule[MAX_WEEKLY_STEPS];
uint8_t current_step = MAX_WEEKLY_STEPS + 1;


#ifdef WITH_LCD
  #include <LiquidCrystal.h>
  char lcd_line1[17];
  char lcd_line2[17];
  LiquidCrystal lcd(LCD_PINS);
#endif


#ifndef RTC_SOFTWARE_WIRE
  #include <Wire.h>
  #ifdef DS3231
    #include <RtcDS3231.h>
    RtcDS3231<TwoWire> Rtc(Wire);
  #else
    #include <RtcDS1307.h>
    RtcDS1307<TwoWire> Rtc(Wire);
  #endif
#else
  #include <SoftwareWire.h>
  #ifdef DS3231
    #include <RtcDS3231.h>
    SoftwareWire myWire(RTC_SOFTWARE_WIRE_SDA, RTC_SOFTWARE_WIRE_SCL);
    RtcDS3231<SoftwareWire> Rtc(myWire);
  #else
    #include <RtcDS1307.h>
    SoftwareWire myWire(RTC_SOFTWARE_WIRE_SDA, RTC_SOFTWARE_WIRE_SCL);
    RtcDS1307<SoftwareWire> Rtc(myWire);
  #endif
#endif


#ifdef MCP9808_TEMP
  // Adafruit MCP9808 i2c temperature sensor
  // See: http://www.adafruit.com/products/1782
  #define MCP9808_ADDRESS 0x18
  #include "Adafruit_MCP9808.h"
  Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
#endif


#ifdef WITH_ENCODER
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


  typedef (encoder_function_t)(int16_t value, ClickEncoder::Button b);
  typedef (display_function_t)();

  struct encoder_handler
  {
    long int encoder_value;
    encoder_function_t *function;
    encoder_function_t *next_function;
    encoder_function_t *prev_function;
    display_function_t *display_function;
  };

  struct encoder_handler temperature_handler;
  struct encoder_handler configuration_handler;
  struct encoder_handler *current_handler = &temperature_handler;
#endif
