#include "temperino.h"


void NullFunction()
{
}


void SwitchToOverride()
{
  current_handler = &configuration_handler;
}


void SwitchToTemperature()
{
  current_handler = &temperature_handler;
}


void ToggleButtonAcceleration()
{
  encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
  #if DEBUG > 1
    Serial.print(F("Acceleration "));
    Serial.println((encoder->getAccelerationEnabled()) ? F("ON") : F("OFF"));
  #endif
}


void set_temperature (int16_t value)
{
  temperature_handler.encoder_value += value;
  temperature_handler.encoder_value = max(MIN_TEMP * STEPS_PER_DEGREE, temperature_handler.encoder_value);
  temperature_handler.encoder_value = min(MAX_TEMP * STEPS_PER_DEGREE, temperature_handler.encoder_value);
  setpoint = (float) temperature_handler.encoder_value / STEPS_PER_DEGREE;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Setpoint: "));
    Serial.println(setpoint);
  #endif
}


void set_override (int16_t value)
{
  configuration_handler.encoder_value += value;
  configuration_handler.encoder_value = max(0, configuration_handler.encoder_value);
  configuration_handler.encoder_value = min(MAX_OVERRIDE / SECONDS_PER_STEP, configuration_handler.encoder_value);
  override_t = configuration_handler.encoder_value * SECONDS_PER_STEP;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Override: "));
    Serial.println(override_t);
  #endif
}


void EncoderDispatcher()
{
  int16_t value = encoder->getValue();
  ClickEncoder::Button b = encoder->getButton();

  if (b == ClickEncoder::Open)
  {
    if (value != 0)
    {
      current_handler->EncoderRotatedFunction(value);
    }
  }
  else
  {
    #if DEBUG > 1
      Serial.print(timestamp);
      Serial.print(F(" Button "));
    #endif
    switch (b)
    {
      case ClickEncoder::Clicked:
        #if DEBUG > 1
          Serial.println(F("Clicked"));
        #endif
        current_handler->ButtonClickedFunction();
        break;
      case ClickEncoder::DoubleClicked:
        #if DEBUG > 1
          Serial.println(F("DoubleClicked"));
        #endif
        current_handler->ButtonDoubleClickedFunction();
        break;
      case ClickEncoder::Held:
        #if DEBUG > 1
          Serial.println(F("Held"));
        #endif
        current_handler->ButtonHeldFunction();
        break;
      case ClickEncoder::Released:
        #if DEBUG > 1
          Serial.println(F("Released"));
        #endif
        current_handler->ButtonReleasedFunction();
        break;
    }
  }
}


// SETUP
void setup()
{

  // Setup Serial
  Serial.begin(SERIAL_SPEED);
  #if DEBUG > 0
    Serial.println(F("Setup started."));
  #endif

  // Setup LCD
  lcd.begin(16, 2);

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


  // Setup ENCODER
  encoder = new ClickEncoder(ENCODER_PINS);
  Timer1.initialize(ENCODER_TIMER);
  Timer1.attachInterrupt(timerIsr);

  // Configure encoder handlers
  temperature_handler.EncoderRotatedFunction = &set_temperature;
  temperature_handler.DisplayFunction = &display_temperature;
  temperature_handler.ButtonClickedFunction = &SwitchToOverride;  
  temperature_handler.ButtonDoubleClickedFunction = &ToggleButtonAcceleration;
  temperature_handler.ButtonHeldFunction = &NullFunction;  
  temperature_handler.ButtonReleasedFunction = &NullFunction;  
  
  configuration_handler.EncoderRotatedFunction = &set_override;
  configuration_handler.DisplayFunction = &display_override;
  configuration_handler.ButtonClickedFunction = &SwitchToTemperature;  
  configuration_handler.ButtonDoubleClickedFunction = &ToggleButtonAcceleration;
  configuration_handler.ButtonHeldFunction = &NullFunction;  
  configuration_handler.ButtonReleasedFunction = &NullFunction;  


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

  EncoderDispatcher();

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

    if (override_t > 0 && current_handler != &configuration_handler)
    {
      override_t -= POLLING_TIME / 1000;
      DUMP(override_t);
    }
    else
    {
      check_schedule();
    }

    select_valve_status();
  }

  // Display status on LCD
  current_handler->DisplayFunction();

  // End of Loop
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



void refresh_lcd()
{
  lcd.setCursor(0, 0);
  lcd.print(lcd_line1);
  for (int k = strlen(lcd_line1); k < 16; k++) lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print(lcd_line2);
  for (int k = strlen(lcd_line2); k < 16; k++) lcd.print(" ");
}


// Display temperature on LCD
void display_temperature ()
{
  char str_temp[16];
  dtostrf(temperature, 6, 2, str_temp);
  if (strlen(str_temp) > 6) str_temp[6] = '\0';
  sprintf(lcd_line1, "Amb:%6s %2i:%2.2i", str_temp, now_tm.tm_hour, now_tm.tm_min);
  dtostrf(setpoint, 6, 2, str_temp);
  if (strlen(str_temp) > 6) str_temp[6] = '\0';
  sprintf(lcd_line2, "Set:%6s   %3s", str_temp, (valve_status == valve_target ? (valve_target ? " ON" : "OFF") : (valve_target ? " on" : "off")));
  refresh_lcd();
}


// Display override on LCD
void display_override ()
{
  uint16_t ovh = override_t / 3600;
  uint16_t ovm = (override_t - ovh * 3600L) / 60;
  sprintf(lcd_line1, "OVR time: %2.2i:%2.2i", ovh, ovm);
  sprintf(lcd_line2, "");
  refresh_lcd();
}
