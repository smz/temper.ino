#include "temperino.h"


void NullFunction()
{
}


void SwitchToSetTime()
{
  localtime_r(&now, &temp_tm);
  SetYearHandler.value    = temp_tm.tm_year;
  SetMonthHandler.value   = temp_tm.tm_mon; 
  SetDayHandler.value     = temp_tm.tm_mday;
  SetHoursHandler.value   = temp_tm.tm_hour;
  SetMinutesHandler.value = temp_tm.tm_min;
  SetSecondsHandler.value = temp_tm.tm_sec;
  ActiveHandler = &SetYearHandler;
}
void SwitchToSetMonth()
{
  ActiveHandler = &SetMonthHandler;
}
void SwitchToSetDay()
{
  ActiveHandler = &SetDayHandler;
}
void SwitchToSetHours()
{
  ActiveHandler = &SetHoursHandler;
}
void SwitchToSetMinutes()
{
  ActiveHandler = &SetMinutesHandler;
}
void SwitchToSetSeconds()
{
  ActiveHandler = &SetSecondsHandler;
}


void SetTime()
{
  time_t temp = mktime(&temp_tm);
  Rtc.SetTime(&temp);
  SwitchToTemperature();
}


void SwitchToOverrideTime()
{
  // Adjust the actual value (which is decremented by the time running in steps of 1 seconds)
  // to the nearest "Increment" value (5 minutes by default)
  long int temp = (OverrideTimeHandler.value + OverrideTimeHandler.Increment / 2) / OverrideTimeHandler.Increment;
  OverrideTimeHandler.value = (float) temp * OverrideTimeHandler.Increment;

  ActiveHandler = &OverrideTimeHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Override setting mode."));
  #endif
}


void SwitchToTemperature()
{
  ActiveHandler = &TemperatureHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Temperature setting mode."));
  #endif
}


void ToggleButtonAcceleration()
{
  encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
  #if DEBUG > 1
    Serial.print(F("Acceleration "));
    Serial.println((encoder->getAccelerationEnabled()) ? F("ON") : F("OFF"));
  #endif
}


void UpdateActiveHandlerValue (int16_t value)
{
  ActiveHandler->value += value * ActiveHandler->Increment;
  ActiveHandler->value = max(ActiveHandler->Min, ActiveHandler->value);
  ActiveHandler->value = min(ActiveHandler->Max, ActiveHandler->value);
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value: "));
    Serial.println(ActiveHandler->value);
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
      ActiveHandler->EncoderRotatedFunction(value);
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
        ActiveHandler->ButtonClickedFunction();
        break;
      case ClickEncoder::DoubleClicked:
        #if DEBUG > 1
          Serial.println(F("DoubleClicked"));
        #endif
        ActiveHandler->ButtonDoubleClickedFunction();
        break;
      case ClickEncoder::Held:
        #if DEBUG > 1
          Serial.println(F("Held"));
        #endif
        ActiveHandler->ButtonHeldFunction();
        break;
      case ClickEncoder::Released:
        #if DEBUG > 1
          Serial.println(F("Released"));
        #endif
        ActiveHandler->ButtonReleasedFunction();
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

#if DEBUG > 0
    Serial.print(F("Using "));
  #ifdef DS3231
    Serial.print (F("DS3231"));
  #else
    Serial.print (F("DS1307"));
  #endif
    Serial.println(F(" RTC"));
#endif

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
//    Rtc.SetTime(&compiled_time_t);
  }

  // Check if the RTC clock is running (Yes, it can be stopped, if you wish!)
  if (!Rtc.GetIsRunning())
  {
    Serial.println(F("WARNING: RTC wasn't running, starting it now."));
    Rtc.SetIsRunning(true);
  }

  // Get the time from the RTC
  now = Rtc.GetTime();

#ifdef RESET_RTC_OLDER
  // Reset the RTC if "now" is older than "Compile time"
  if (now < compiled_time_t)
  {
    Serial.println(F("WARNING: RTC is older than compile time, setting RTC with compile time!"));
    Rtc.SetTime(&compiled_time_t);
  }
#endif

#ifdef DS3231
  // Reset the DS3231 RTC status in case it was wrongly configured
//Rtc.Enable32kHzPin(false); // <--- troubles, bug, fubar, check!
//Rtc.Enable32kHzPin(true);  // <--- troubles, bug, fubar, check!
//  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
#endif


#ifdef MCP9808_TEMP
  // Setup MCP9808
  if (!tempsensor.begin(MCP9808_I2C_ADDRESS))
  {
    Serial.println(F("Couldn't find MCP9808!"));
    while (true);
  }
  tempsensor.setResolution(MCP9808_TEMP_RESOLUTION);
  #if DEBUG > 0
    uint8_t temperatureResolution = tempsensor.getResolution();
    Serial.print(F("MCP9808"));
    DUMP(temperatureResolution);
  #endif
#endif


  // Setup ENCODER
  encoder = new ClickEncoder(ENCODER_PINS);
  Timer1.initialize(ENCODER_TIMER);
  Timer1.attachInterrupt(timerIsr);

  // Configure handlers
  TemperatureHandler.Min =                         TEMP_MIN;
  TemperatureHandler.Max =                         TEMP_MAX;
  TemperatureHandler.Increment =                   TEMP_INCREMENT;
  TemperatureHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  TemperatureHandler.DisplayFunction =             &DisplayTemperature;
  TemperatureHandler.ButtonClickedFunction =       &SwitchToOverrideTime;  
  TemperatureHandler.ButtonDoubleClickedFunction = &NullFunction;
  TemperatureHandler.ButtonHeldFunction =          &NullFunction;  
  TemperatureHandler.ButtonReleasedFunction =      &SwitchToSetTime;  
  
  OverrideTimeHandler.Min =                         0;
  OverrideTimeHandler.Max =                         OVERRIDE_TIME_MAX;
  OverrideTimeHandler.Increment =                   OVERRIDE_TIME_INCREMENT;
  OverrideTimeHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  OverrideTimeHandler.DisplayFunction =             &DisplayOverrideTime;
  OverrideTimeHandler.ButtonClickedFunction =       &SwitchToTemperature;  
  OverrideTimeHandler.ButtonDoubleClickedFunction = &NullFunction;
  OverrideTimeHandler.ButtonHeldFunction =          &NullFunction;  
  OverrideTimeHandler.ButtonReleasedFunction =      &NullFunction;  

  SetYearHandler.Min =                         117;
  SetYearHandler.Max =                         199;
  SetYearHandler.Increment =                   1;
  SetYearHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetYearHandler.DisplayFunction =             &DisplayDateSetting;
  SetYearHandler.ButtonClickedFunction =       &SwitchToSetMonth;
  SetYearHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetYearHandler.ButtonHeldFunction =          &NullFunction;
  SetYearHandler.ButtonReleasedFunction =      &NullFunction;

  SetMonthHandler.Min =                         0;
  SetMonthHandler.Max =                         11;
  SetMonthHandler.Increment =                   1;
  SetMonthHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetMonthHandler.DisplayFunction =             &DisplayDateSetting;
  SetMonthHandler.ButtonClickedFunction =       &SwitchToSetDay;
  SetMonthHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetMonthHandler.ButtonHeldFunction =          &NullFunction;
  SetMonthHandler.ButtonReleasedFunction =      &NullFunction;

  SetDayHandler.Min =                         1;
  SetDayHandler.Max =                         31;
  SetDayHandler.Increment =                   1;
  SetDayHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetDayHandler.DisplayFunction =             &DisplayDateSetting;
  SetDayHandler.ButtonClickedFunction =       &SwitchToSetHours;
  SetDayHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetDayHandler.ButtonHeldFunction =          &NullFunction;
  SetDayHandler.ButtonReleasedFunction =      &NullFunction;

  SetHoursHandler.Min =                         0;
  SetHoursHandler.Max =                         23;
  SetHoursHandler.Increment =                   1;
  SetHoursHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetHoursHandler.DisplayFunction =             &DisplayTimeSetting;
  SetHoursHandler.ButtonClickedFunction =       &SwitchToSetMinutes;
  SetHoursHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetHoursHandler.ButtonHeldFunction =          &NullFunction;
  SetHoursHandler.ButtonReleasedFunction =      &ToggleButtonAcceleration;

  SetMinutesHandler.Min =                         0;
  SetMinutesHandler.Max =                         59;
  SetMinutesHandler.Increment =                   1;
  SetMinutesHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetMinutesHandler.DisplayFunction =             &DisplayTimeSetting;
  SetMinutesHandler.ButtonClickedFunction =       &SwitchToSetSeconds;
  SetMinutesHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetMinutesHandler.ButtonHeldFunction =          &NullFunction;
  SetMinutesHandler.ButtonReleasedFunction =      &NullFunction;

  SetSecondsHandler.Min =                         0;
  SetSecondsHandler.Max =                         59;
  SetSecondsHandler.Increment =                   1;
  SetSecondsHandler.EncoderRotatedFunction =      &UpdateActiveHandlerValue;
  SetSecondsHandler.DisplayFunction =             &DisplayTimeSetting;
  SetSecondsHandler.ButtonClickedFunction =       &SetTime;
  SetSecondsHandler.ButtonDoubleClickedFunction = &NullFunction;
  SetSecondsHandler.ButtonHeldFunction =          &NullFunction;
  SetSecondsHandler.ButtonReleasedFunction =      &NullFunction;

  // Initialize global variables
  TemperatureHandler.value = TemperatureHandler.Min;
  temperature = TemperatureHandler.Min;
  valve_target = false;
  valve_status = false;
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);
  prev_valve_time = now;
  localtime_r(&now, &now_tm);
  strcpy(timestamp, isotime(&now_tm));

  now = Rtc.GetTime();
  localtime_r(&now, &now_tm);
  strcpy(timestamp, isotime(&now_tm));

  // Initialize the weekly schedule
  init_schedule();

  #if DEBUG > 0
    Serial.print(F("Setup done.\n\n"));
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
//      now_tow = now_tm.tm_wday * 10000UL + now_tm.tm_hour * 100UL + now_tm.tm_min;
      now_tow = now_tm.tm_wday * 10000 + now_tm.tm_hour * 100 + now_tm.tm_min;
      #if DEBUG > 3
        Serial.print(timestamp);
        Serial.print(" year=");
        Serial.print(now_tm.tm_year);
        Serial.print(" month=");
        Serial.print(now_tm.tm_mon);
        Serial.print(" mday=");
        Serial.print(now_tm.tm_mday);
        Serial.print(" hour=");
        Serial.print(now_tm.tm_hour);
        Serial.print(" min=");
        Serial.print(now_tm.tm_min);
        Serial.print(" sec=");
        Serial.print(now_tm.tm_sec);
        Serial.print(" wday=");
        Serial.print(now_tm.tm_wday);
        Serial.print(" tow=");
        Serial.println(now_tow);
      #endif
    }
    else
    {
      TemperatureHandler.value = TemperatureHandler.Min;
      Serial.println(F("RTC clock failed!"));
    }

    if (OverrideTimeHandler.value > 0 && ActiveHandler != &OverrideTimeHandler)
    {
      OverrideTimeHandler.value -= POLLING_TIME / 1000;
    }
    else
    {
      check_schedule();
    }

    select_valve_status();
  }

  // Display status on LCD
  ActiveHandler->DisplayFunction();

  // End of Loop
}



// GetTemperature function
void GetTemperature()
{
#ifdef MCP9808_TEMP
  // MCP9808 Temperature
  if (tempsensor.begin(MCP9808_I2C_ADDRESS))
  {
    tempsensor.wakeup();
    temperature = tempsensor.readTempC();
    delay(tempsensor.getSamplingTime());
    tempsensor.shutdown();
 }
  else
  {
    Serial.print(timestamp);
    Serial.println(F(" MCP9808 failed!"));
    temperature = TemperatureHandler.Max;
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
  if ((abs(TemperatureHandler.value - temperature) > TEMP_HYSTERESIS))
  {
    if (TemperatureHandler.value > temperature)
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
        Serial.print(TemperatureHandler.value);
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
  float newtemp = TemperatureHandler.Min - 1;

  while (step < MAX_WEEKLY_STEPS)
  {
    if (schedule[step].tow > now_tow) break;
    newtemp = schedule[step].temperature;
    step++;
  }

  if (newtemp < TemperatureHandler.Min) return;

  if (step == current_step) return;

  #if DEBUG > 2
    Serial.print(timestamp);
    DUMP(step);
  #endif

  TemperatureHandler.value = newtemp;

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



// Actually print the content of the two line buffers on the LCD
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
void DisplayTemperature ()
{
  char str_temp[16];
  char str_ovt[6];
  dtostrf(temperature, 5, 1, str_temp);
  if (strlen(str_temp) > 5) str_temp[5] = '\0';
  sprintf(lcd_line1, "A%5s  %2i:%2.2i:%2.2i", str_temp, now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
  dtostrf(TemperatureHandler.value, 5, 1, str_temp);
  if (strlen(str_temp) > 5) str_temp[5] = '\0';
  sprintf(lcd_line2, "S%5s %6.6s %2s", str_temp, GetFormattedOverrideTime(), (valve_status == valve_target ? (valve_target ? "ON" : "OF") : (valve_target ? "on" : "of")));
  refresh_lcd();
}


// GetFormattedOverrideTime
char* GetFormattedOverrideTime (void)
{
  #define OVT_ROUNDING 59UL
  static char str[6];
  uint16_t ovh = (OverrideTimeHandler.value + OVT_ROUNDING) / 3600;
  uint16_t ovm = ((OverrideTimeHandler.value + OVT_ROUNDING) - ovh * 3600L) / 60;
  sprintf(str, "%2.2i:%2.2i", ovh, ovm);
  return str;
}


// Display override time on LCD
void DisplayOverrideTime ()
{
  sprintf(lcd_line1, "OVR time: %5.5s", GetFormattedOverrideTime());
  sprintf(lcd_line2, "");
  refresh_lcd();
}


void DisplayDateSetting()
{
  strcpy(lcd_line1, "Set the date:");
  temp_tm.tm_year = SetYearHandler.value;
  temp_tm.tm_mon = SetMonthHandler.value;
  temp_tm.tm_mday = SetDayHandler.value;
  temp_tm.tm_hour = SetHoursHandler.value;
  temp_tm.tm_min = SetMinutesHandler.value;
  temp_tm.tm_sec = SetSecondsHandler.value;
  isotime_r(&temp_tm, lcd_line2);
  lcd_line2[10] = '\0';
  refresh_lcd();
}

void DisplayTimeSetting()
{
  char str_temp[16];
  strcpy(lcd_line1, "Set the time:");
  temp_tm.tm_year = SetYearHandler.value;
  temp_tm.tm_mon = SetMonthHandler.value;
  temp_tm.tm_mday = SetDayHandler.value;
  temp_tm.tm_hour = SetHoursHandler.value;
  temp_tm.tm_min = SetMinutesHandler.value;
  temp_tm.tm_sec = SetSecondsHandler.value;
  isotime_r(&temp_tm, str_temp);
  strcpy(lcd_line2, &str_temp[11]);
  refresh_lcd();
}