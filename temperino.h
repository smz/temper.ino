#include <time.h>
#include <EEPROM.h>

// DEBUG
//  0 Nothing
//  1 actions
//  2 + rotary encoder
//  3 + schedule
//  4 + EEPROM
//  9 Everything
#define DEBUG 0


// Project configuration parameters
#undef  DS3231
#undef  DS3231_TEMP
#define MCP9808_TEMP
#undef  SOFTWARE_WIRE
#undef  LCD
#define SH1106
#define AUTO485


// Connections
#define LCD_PINS  7,  8,  9, 10, 11, 12
//      LCD_PINS RS, EN, D4, D5, D6, D7
#define ENCODER_PINS   2,  3,  4
//      ENCODER_PINS CLK, DT, SW
#define SOFTWARE_WIRE_SDA SDA
#define SOFTWARE_WIRE_SCL SCL
#define RELAY_PIN 13
#define AUTO485_DE_PIN 10


// Operative parameters
#define SERIAL_SPEED 9600
#define POLLING_TIME 1000
#define ENCODER_TIMER 1000
#define TEMP_MIN 5.0
#define TEMP_MAX 35.0
#define TEMP_INCREMENT 0.5
#define TEMP_HYSTERESIS 0.5
#define MAX_WEEKLY_STEPS 70
#define OVERRIDE_TIME_MAX 86400
#define OVERRIDE_TIME_INCREMENT 300
#define SLEEP_AFTER 60
#define RELAY_QUIESCENT_TIME 15
#define MCP9808_TEMP_RESOLUTION 0x03
#define MCP9808_I2C_ADDRESS 0x18
#define DEFAULT_OVERRIDE_TIME 3600
#define MY_ADDR 1


// Time parameters
#define TIMEZONE (1 * ONE_HOUR)
#define DST_RULES EU
#if DST_RULES == EU
  #include "eu_dst.h"
#endif


// i18n
#define MONTHS "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"
#define WEEKDAYS "Do", "Lu", "Ma", "Me", "Gi", "Ve", "Sa"
#define TIME_SETTINGS_MSG "Set the time:"
#define OVERRIDE_TIME_SETTINGS_MSG "Override time:"
#define TEMP_FAILED_MSG "ERR!"
#define CLOCK_FAILED_MSG "ERRORE OROLOGIO!"


// Commands
#define CMD_GET_TEMPERATURE   1
#define CMD_GET_SET_TIME      2
#define CMD_GET_SET_SETPOINT  3  
#define CMD_GET_SET_OVERRIDE  4
#define CMD_GET_SET_STEPS     5


// Global variables
float temperature;
float setpoint;
bool relayTarget;
bool relayStatus;
time_t prevActivationTime;
unsigned long prevMillis;
time_t now;
struct tm tmNow;
struct tm tmSettings;
uint16_t nowTOW;
time_t lastTouched;
struct tm tmOverride;
char tempString[20];
char timestamp[20];
bool tempFailed;
bool clockFailed;
bool settingOverride;
#if DEBUG > 8
  unsigned long loops = 0;
  #define PrintLoops() {             \
      mySerial.print(timestamp);     \
      mySerial.print(F(" Loops: ")); \
      mySerial.println(loops);       \
      loops = 0;                     \
      }
#endif


// EEPROM storage
// Schedule table
bool status;
#define EEPROMstatusAddress (0)

time_t overrideTime;
#define EEPROMoverrideTimeAddress (sizeof(bool))

typedef struct {uint16_t tow; float temperature;} programStep;  // there will be many of this...
#define EEPROMstepAddress(x) (EEPROMoverrideTimeAddress + sizeof(time_t) + x * sizeof(programStep))


// RS-485 support
#ifdef AUTO485
  // Use the Auto485 library by Michael Adams (https://github.com/madleech/Auto485)
  #include <Auto485.h>
  #define mySerial Auto485Bus
  Auto485 mySerial(AUTO485_DE_PIN);
#else
  #define mySerial Serial
#endif


// LCD
#ifdef LCD
  // Use the LiquidCrystal library by Adafruit (https://www.arduino.cc/en/Reference/LiquidCrystal)
  #include <LiquidCrystal.h>
  LiquidCrystal lcd(LCD_PINS);
  char lcdLine1[17];
  char lcdLine2[17];
#endif


// SH1106 OLED
#ifdef SH1106
  // Use the u8g2 library by olikraus (https://github.com/olikraus/u8g2)
  #include <U8g2lib.h>
  #define BIG_FONT u8g2_font_profont22_tr
  #define SMALL_FONT u8g2_font_7x13B_tr
  U8G2_SH1106_128X64_VCOMH0_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif


// TWI/I2C interface
// Note: SoftwareWire untested together with SH1106 OLED: it probably wouldn't work or be extremely sluggish
#ifdef SOFTWARE_WIRE
  #include <SoftwareWire.h>
  #define myWire SoftwareWire
  myWire I2C(SOFTWARE_WIRE_SDA, SOFTWARE_WIRE_SCL);
#else
  #include <Wire.h>
  #define myWire TwoWire
  #define I2C Wire
#endif


// RTC
// Use the RTCtime library by smz (https://github.com/smz/Arduino-RTCtime)
#ifdef DS3231
  #include <RtcDS3231.h>
  RtcDS3231<myWire> Rtc(I2C);
#else
  #include <RtcDS1307.h>
  RtcDS1307<myWire> Rtc(I2C);
#endif


// Temperature sensor
// Use the MCP9808sensor library by smz (https://github.com/smz/Arduino-MCP9808sensor)
#ifdef MCP9808_TEMP
  #include "MCP9808sensor.h"
  MCP9808sensor<myWire> tempsensor(I2C);
#endif


// Rotary Encoder
// Use the ClickEncoder library by soligen2010 (https://github.com/soligen2010/encoder
// Needs the TimerOne library by Paul Stoffregen (https://github.com/PaulStoffregen/TimerOne)
#include <TimerOne.h>
#include <ClickEncoder.h>
ClickEncoder *encoder;
void timerIsr()
{
  encoder->service();
}


// Structures defining the different values that can be handled
// using the rotary encoder and their associated display functions 
typedef (EncoderRotatedFunction_t)(int16_t value);
typedef (DisplayFunction_t)();
typedef (ButtonFunction_t)();
typedef struct
{
  union {
    float *float_value;
    uint8_t *uint8_value;
    uint16_t *uint16_value;
    int *int_value;
  };
  tm *tm_base;
  float Min;
  float Max;
  float Increment;
  EncoderRotatedFunction_t *EncoderRotatedFunction;
  DisplayFunction_t *DisplayFunction;
  ButtonFunction_t *ButtonClickedFunction;
  ButtonFunction_t *ButtonDoubleClickedFunction;
  ButtonFunction_t *ButtonHeldFunction;
  ButtonFunction_t *ButtonReleasedFunction;
} EncoderHandler_t;

EncoderHandler_t TemperatureHandler;
EncoderHandler_t SetYearHandler;
EncoderHandler_t SetMonthHandler;
EncoderHandler_t SetDayHandler;
EncoderHandler_t SetHoursHandler;
EncoderHandler_t SetMinutesHandler;
EncoderHandler_t SetSecondsHandler;
EncoderHandler_t SleepHandler;
EncoderHandler_t OffHandler;
EncoderHandler_t *handler;
