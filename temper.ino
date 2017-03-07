#include <time.h>

// DEBUG
//  0 No debug
//  1 Debug actions
//  2 Debug actions and Rotary Encoder
//  3 Debug actions, Rotary Encoder and scheduler
// 99 Debug all
#define DEBUG 3


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
#define VALVE_ACTIVATION_TIME 15
#define MIN_TEMP 5
#define MAX_TEMP 25
#define MAX_WEEKLY_STEPS 70


// Global variables
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

  struct encoder_handler
  {
    int16_t encoder_value;
    int16_t encoder_last;
    void (*function)(int16_t *encoder_value);
  };

  struct encoder_handler temperature_handler;
  struct encoder_handler configuration_handler;
  struct encoder_handler *current_handler;
#endif


// Support stuff for debug...
#if DEBUG > 0
  #define DUMP(x)           \
    Serial.print(" ");      \
    Serial.print(#x);       \
    Serial.print(F(" = ")); \
    Serial.println(x);

  #define DUMP_TM(x)  \
    DUMP(x.tm_year);  \
    DUMP(x.tm_mon);   \
    DUMP(x.tm_mday);  \
    DUMP(x.tm_hour);  \
    DUMP(x.tm_min);   \
    DUMP(x.tm_sec);   \
    DUMP(x.tm_isdst); \
    DUMP(x.tm_yday);  \
    DUMP(x.tm_wday);
#endif

#if DEBUG > 90
  unsigned long loops = 0;
  #define PrintLoops() {           \
      Serial.print(timestamp);     \
      Serial.print(F(" Loops: ")); \
      Serial.println(loops);       \
      loops = 0;                   \
      }
#endif



void set_temperature (int16_t *encoder_value)
{
  *encoder_value = max(MIN_TEMP * STEPS_PER_DEGREE, *encoder_value);
  *encoder_value = min(MAX_TEMP * STEPS_PER_DEGREE, *encoder_value);
  setpoint = (float) *encoder_value / STEPS_PER_DEGREE;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Setpoint: "));
    Serial.println(setpoint);
  #endif
}



void configure (int16_t *encoder_value)
{
}



// SETUP
void setup()
{

  // Setup Serial
  Serial.begin(SERIAL_SPEED);
  #if DEBUG > 0
    Serial.println(F("Setup started."));
  #endif

#ifdef WITH_LCD
  // Setup LCD
  lcd.begin(16, 2);
#endif

#ifdef DS3231
  // Setup RTC
  Rtc.Begin();
  set_zone(TIMEZONE);

  // Here we convert the __DATE__ and __TIME__ preprocessor macros to a "time_t value"
  // to initialize the RTC with it in case it is "older" than
  // the compile time (i.e: it was wrongly set. But your PC clock might be wrong too!)
  // or in case it is invalid.
  // This is *very* crude, it would be MUCH better to take the time from a reliable
  // source (GPS or NTP), or even set it "by hand", but -hey!-, this is just an example!!
  // N.B.: We always set the RTC to the compile time when we are debugging.
  #define COMPILE_DATE_TIME (__DATE__ " " __TIME__)
  time_t compiled_time_t = str20ToTime(COMPILE_DATE_TIME);


  // Now we check the health of our RTC and in case we try to "fix it"
  // Common causes for it being invalid are:
  //    1) first time you ran and the device wasn't running yet
  //    2) the battery on the device is low or even missing


  // Check if the time is valid.
  #ifndef RESET_RTC_TIME
  if (!Rtc.IsDateTimeValid())
  #endif
  {
    #ifndef RESET_RTC_TIME
    Serial.println(F("WARNING: RTC invalid time, setting RTC with compile time."));
    #else
    Serial.println(F("Forcing setting RTC with compile time."));
    #endif
    Rtc.SetTime(&compiled_time_t);
  }

  // Check if the RTC clock is running (Yes, it can be stopped, if you wish!)
  if (!Rtc.GetIsRunning())
  {
    Serial.println(F("WARNING: RTC wasn't running, starting it now."));
    Rtc.SetIsRunning(true);
  }

  // Get the time from the RTC
  now = Rtc.GetTime();

  // Reset the RTC if "now" is older than "Compile time"
  if (now < compiled_time_t)
  {
    Serial.println(F("WARNING: RTC is older than compile time, setting RTC with compile time!"));
    Rtc.SetTime(&compiled_time_t);
  }

  // Reset the DS3231 RTC status in case it was wrongly configured
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
#endif


#ifdef MCP9808_TEMP
  // Setup MCP9808
  //
  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example
  if (!tempsensor.begin())
  {
    Serial.println(F("Couldn't find MCP9808!"));
  }
#endif


#ifdef WITH_ENCODER
  // Setup ENCODER
  encoder = new ClickEncoder(ENCODER_PINS);
  Timer1.initialize(ENCODER_TIMER);
  Timer1.attachInterrupt(timerIsr);
  temperature_handler.function = &set_temperature;
  temperature_handler.encoder_last = -1;
  configuration_handler.function = &configure;
  configuration_handler.encoder_last = -1;
  current_handler = &temperature_handler;
#endif


  // Initialize global variables
  setpoint = MIN_TEMP;
  temperature = MIN_TEMP;
  valve_target = false;
  valve_status = false;
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);
  prev_valve_time = now;
  localtime_r(&now, &now_tm);
  strcpy(timestamp, isotime(&now_tm));
  now_tow = now_tm.tm_wday * 10000 + now_tm.tm_hour * 100 + now_tm.tm_min;

  // Initialize the weekly schedule
  init_schedule();

  #if DEBUG > 0
    Serial.println(F("Setup done."));
    Serial.println("");
  #endif

  // End of Setup
}



// Main loop
void loop()
{

  valve_status = (bool) digitalRead(VALVE_PIN);
  unsigned long current_millis = millis();

  #if DEBUG > 90
    loops++;
  #endif

  handle_encoder(&current_handler);

  if (current_millis - prev_millis >= POLLING_TIME)
  {
    prev_millis += POLLING_TIME;

    #if DEBUG > 90
      PrintLoops();
    #endif

    GetTemperature();

    // Get the time and set timestamp
    if (Rtc.IsDateTimeValid())
    {
      now = Rtc.GetTime();
      localtime_r(&now, &now_tm);
      strcpy(timestamp, isotime(&now_tm));
      now_tow = now_tm.tm_wday * 10000 + now_tm.tm_hour * 100 + now_tm.tm_min;
    }
    else
    {
      setpoint = MIN_TEMP;
      Serial.println(F("RTC clock failed!"));
    }

    check_schedule();

    select_valve_status();
  }

  // Display status on LCD
  display_status();

  // End of Loop
}



// User interaction
void handle_encoder(struct encoder_handler **encoder_structure)
{

  // Handle encoder
  (*encoder_structure)->encoder_value = (*encoder_structure)->encoder_value + encoder->getValue();
  if ((*encoder_structure)->encoder_value != (*encoder_structure)->encoder_last)
  {
    (*encoder_structure)->function(&(*encoder_structure)->encoder_value);
    (*encoder_structure)->encoder_last = (*encoder_structure)->encoder_value;
  }

  // Handle encoder button
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open)
  {
    #if DEBUG > 1
      Serial.print(timestamp);
      Serial.print(F(" Button "));
    #endif
    switch (b)
    {
      case ClickEncoder::Pressed:
        #if DEBUG > 1
          Serial.println(F("Pressed - I've never seen that!"));
        #endif
        break;
      case ClickEncoder::Held:
        #if DEBUG > 1
          Serial.println(F("Held"));
        #endif
        break;
      case ClickEncoder::Released:
        #if DEBUG > 1
          Serial.print(F("Released"));
        #endif
        if (*encoder_structure == &temperature_handler)
        {
          Serial.println(F(" - Switching to configuration handler."));
          *encoder_structure = &configuration_handler;
        }
        else
        {
          Serial.println(F(" - Switching to temperature handler."));
          *encoder_structure = &temperature_handler;
        }
        break;
      case ClickEncoder::Clicked:
        #if DEBUG > 1
          Serial.println(F("Clicked"));
        #endif
        break;
      case ClickEncoder::DoubleClicked:
        #if DEBUG > 1
          Serial.println(F("DoubleClicked"));
          Serial.print(F("Acceleration "));
          Serial.println((encoder->getAccelerationEnabled()) ? F("OFF") : F("ON"));
        #endif
        encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
        break;
    }
  }
}



// GetTemperature function
void GetTemperature()
{
#ifdef MCP9808_TEMP
  // MCP9808 Temperature
  #define MCP9808_DELAY 250
  if (tempsensor.begin(MCP9808_ADDRESS))
  {
    tempsensor.shutdown_wake(0);
    temperature = tempsensor.readTempC();
    delay(MCP9808_DELAY);
    tempsensor.shutdown_wake(1);
  }
  else
  {
    Serial.print(timestamp);
    Serial.println(F(" MCP9808 failed!"));
    temperature = MAX_TEMP;
  }
#endif

#ifdef DS3231_TEMP
  // RTC Temperature
  temperature = Rtc.GetTemperature();
#endif
}



// See if valve status should be modified
void select_valve_status()
{
  if ((abs(setpoint - temperature) > TEMP_HYSTERESIS))
  {
    if (setpoint > temperature)
    {
      valve_target = true;
    }
    else
    {
      valve_target = false;
    }

    if (valve_status != valve_target)
    {
      if (now - prev_valve_time > VALVE_ACTIVATION_TIME)
      {
        digitalWrite(VALVE_PIN, valve_target ? HIGH : LOW);
        prev_valve_time = now;
        valve_status = valve_target;
        #if DEBUG > 0
          Serial.print(timestamp);
          Serial.println(valve_status ? F(" Turned on.") : F(" Turned off."));
        #endif
      }
      #if DEBUG > 0
      else
      {
        Serial.print(timestamp);
        Serial.print(F(" Setpoint: "));
        Serial.print(setpoint);
        Serial.print(F(" Temp: "));
        Serial.print(temperature);
        Serial.println(valve_target ? F(" Turn on!") : F(" Turn off!"));
      }
      #endif
    }
  }
}



void check_schedule()
{
  int step = 0;
  float newtemp = MIN_TEMP - 1;

  while (step < MAX_WEEKLY_STEPS)
  {
    if (schedule[step].tow > now_tow) break;
    newtemp = schedule[step].temperature;
    step++;
  }

  if (newtemp < MIN_TEMP) return;

  if (step == current_step) return;

  #if DEBUG > 2
    Serial.print(timestamp);
    DUMP(step);
  #endif

  setpoint = newtemp;
  temperature_handler.encoder_value = setpoint * STEPS_PER_DEGREE;
  current_step = step;

}



void init_schedule()
{
  // Initialize the weekly schedule
  for (int step = 0; step < MAX_WEEKLY_STEPS; step++)
  {
    schedule[step].tow = 100000; // Set to invalid time.
  }

  // This is just for testing. Must be replaced with code to read values from EEPROM)
  schedule[0].tow = now_tow - 1;
  schedule[0].temperature = 11.0;

  schedule[1].tow = now_tow + 1;
  schedule[1].temperature = 12.0;

  schedule[2].tow = now_tow + 2;
  schedule[2].temperature = 13.0;

  schedule[3].tow = ((now_tm.tm_wday + 1) % 7) * 10000;     // Tomorrow's 00:00:00
  schedule[3].temperature = 21.0;

  schedule[4].tow = ((now_tm.tm_wday + 1) % 7) * 10000 + 1; // Tomorrow's 00:01:00
  schedule[4].temperature = 22.0;

  schedule[5].tow = ((now_tm.tm_wday + 1) % 7) * 10000 + 2; // Tomorrow's 00:02:00
  schedule[5].temperature = 23.0;
}



// Display status on LCD
void display_status ()
{
#ifdef WITH_LCD
  char str_temp[16];
  dtostrf(temperature, 6, 2, str_temp);
  if (strlen(str_temp) > 6) str_temp[6] = '\0';
  sprintf(lcd_line1, "Amb:%6s %2i:%2.2i", str_temp, now_tm.tm_hour, now_tm.tm_min);
  dtostrf(setpoint, 6, 2, str_temp);
  if (strlen(str_temp) > 6) str_temp[6] = '\0';
  sprintf(lcd_line2, "Set:%6s   %3s", str_temp, (valve_status == valve_target ? (valve_target ? " ON" : "OFF") : (valve_target ? " on" : "off")));
  lcd.setCursor(0, 0);
  lcd.print(lcd_line1);
  for (int k = strlen(lcd_line1); k < 16; k++) lcd.print("");
  lcd.setCursor(0, 1);
  lcd.print(lcd_line2);
  for (int k = strlen(lcd_line2); k < 16; k++) lcd.print("");
#endif
}



void shutdown()
{
  digitalWrite(VALVE_PIN, LOW);
  valve_status = false;
  valve_target = false;
  temperature = 0;
  setpoint = 0;
  display_status();
  while (true);
}
