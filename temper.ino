#include "temperino.h"


void NullFunction()
{
}

void ChangeStatus()
{
  if (status)
  {
    status = false;
    digitalWrite(VALVE_PIN, LOW);
    ActiveHandler = &OffHandler;
  }
  else
  {
    status = true;
    ActiveHandler = &TemperatureHandler;
  }
  EEPROM.put(0, status);
}

void SwitchToSetTime()
{
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Time"));
  #endif
  localtime_r(&now, &temp_tm);
  temp_tm.tm_sec  = 0;
  SwitchToSetYear();
}

void SwitchToSetYear()
{
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
  now = mktime(&temp_tm);
  Rtc.SetTime(&now);
  LastTouched = now;
  localtime_r(&now, &now_tm);
  SwitchToTemperature();
}


void SwitchToOverrideTime()
{
  // Adjust the actual value (which is decremented by the time running in steps of 1 seconds)
  // to the nearest "Increment" value (5 minutes by default)
  uint16_t temp = (OverrideTime + OverrideTimeHandler.Increment / 2) / OverrideTimeHandler.Increment;
  OverrideTime = temp * OverrideTimeHandler.Increment;

  ActiveHandler = &OverrideTimeHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Override"));
  #endif
}


void SwitchToTemperature()
{
  ActiveHandler = &TemperatureHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Temperature"));
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


void UpdateFloatValue (int16_t value)
{
  *ActiveHandler->float_value += value * ActiveHandler->Increment;
  if (*ActiveHandler->float_value > ActiveHandler->Max)
  {
    *ActiveHandler->float_value = ActiveHandler->Max;
  }
  if (*ActiveHandler->float_value < ActiveHandler->Min)
  {
    *ActiveHandler->float_value = ActiveHandler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (float): "));
    Serial.println(*ActiveHandler->float_value);
  #endif
}

void UpdateUint8Value (int16_t value)
{
  int increment = value * ActiveHandler->Increment;

  if (increment < 0 & *ActiveHandler->uint8_value < abs(increment))
  {
    *ActiveHandler->uint8_value = 0;
  }
  else
  {
    *ActiveHandler->uint8_value += increment;
  }

  if (*ActiveHandler->uint8_value > ActiveHandler->Max)
  {
    *ActiveHandler->uint8_value = ActiveHandler->Max;
  }
  if (*ActiveHandler->uint8_value < ActiveHandler->Min)
  {
    *ActiveHandler->uint8_value = ActiveHandler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (uint8): "));
    Serial.println(*ActiveHandler->uint8_value);
  #endif
}

void UpdateUint16Value (int16_t value)
{
  int increment = value * ActiveHandler->Increment;

  if (increment < 0 & *ActiveHandler->uint16_value < abs(increment))
  {
    *ActiveHandler->uint16_value = 0;
  }
  else
  {
    *ActiveHandler->uint16_value += increment;
  }

  if (*ActiveHandler->uint16_value > ActiveHandler->Max)
  {
    *ActiveHandler->uint16_value = ActiveHandler->Max;
  }
  if (*ActiveHandler->uint16_value < ActiveHandler->Min)
  {
    *ActiveHandler->uint16_value = ActiveHandler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (uint16): "));
    Serial.println(*ActiveHandler->uint16_value);
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
      LastTouched = now;
      ActiveHandler->EncoderRotatedFunction(value);
    }
  }
  else
  {
    #if DEBUG > 1
      Serial.print(timestamp);
      Serial.print(F(" Button "));
    #endif
    LastTouched = now;
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
  #if DEBUG > 0
    Serial.begin(SERIAL_SPEED);
    Serial.println(F("Setup started."));
  #endif

  // Setup LCD
  #ifdef LCD
    lcd.begin(16, 2);
  #endif

  // Setup SH1106 OLED
  #ifdef SH1106
    u8g2.begin();
  #endif

  // Print debug info
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
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
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
  TemperatureHandler.id =                           1;
  TemperatureHandler.float_value =                  &setpoint;
  TemperatureHandler.Min =                          TEMP_MIN;
  TemperatureHandler.Max =                          TEMP_MAX;
  TemperatureHandler.Increment =                    TEMP_INCREMENT;
  TemperatureHandler.EncoderRotatedFunction =       &UpdateFloatValue;
  TemperatureHandler.DisplayFunction =              &DisplayTemperature;
  TemperatureHandler.ButtonClickedFunction =        &SwitchToOverrideTime;
  TemperatureHandler.ButtonDoubleClickedFunction =  &SwitchToSetTime;
  TemperatureHandler.ButtonHeldFunction =           &NullFunction;
  TemperatureHandler.ButtonReleasedFunction =       &ChangeStatus;

  OverrideTimeHandler.id =                          2;
  OverrideTimeHandler.uint16_value =                &OverrideTime;
  OverrideTimeHandler.Min =                         0;
  OverrideTimeHandler.Max =                         OVERRIDE_TIME_MAX;
  OverrideTimeHandler.Increment =                   OVERRIDE_TIME_INCREMENT;
  OverrideTimeHandler.EncoderRotatedFunction =      &UpdateUint16Value;
  OverrideTimeHandler.DisplayFunction =             &DisplayOverrideTime;
  OverrideTimeHandler.ButtonClickedFunction =       &SwitchToTemperature;
  OverrideTimeHandler.ButtonDoubleClickedFunction = &NullFunction;
  OverrideTimeHandler.ButtonHeldFunction =          &NullFunction;
  OverrideTimeHandler.ButtonReleasedFunction =      &NullFunction;

  SetYearHandler.id =                               3;
  SetYearHandler.uint16_value =                     &temp_tm.tm_year;
  SetYearHandler.Min =                              117;
  SetYearHandler.Max =                              199;
  SetYearHandler.Increment =                        1;
  SetYearHandler.EncoderRotatedFunction =           &UpdateUint16Value;
  SetYearHandler.DisplayFunction =                  &DisplayTimeSetting;
  SetYearHandler.ButtonClickedFunction =            &SwitchToSetMonth;
  SetYearHandler.ButtonDoubleClickedFunction =      &SwitchToTemperature;
  SetYearHandler.ButtonHeldFunction =               &NullFunction;
  SetYearHandler.ButtonReleasedFunction =           &SwitchToTemperature;

  SetMonthHandler.id =                              4;
  SetMonthHandler.uint8_value =                     &temp_tm.tm_mon;
  SetMonthHandler.Min =                             0;
  SetMonthHandler.Max =                             11;
  SetMonthHandler.Increment =                       1;
  SetMonthHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetMonthHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetMonthHandler.ButtonClickedFunction =           &SwitchToSetDay;
  SetMonthHandler.ButtonDoubleClickedFunction =     &SwitchToSetYear;
  SetMonthHandler.ButtonHeldFunction =              &NullFunction;
  SetMonthHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetDayHandler.id =                                5;
  SetDayHandler.uint8_value =                       &temp_tm.tm_mday;
  SetDayHandler.Min =                               1;
  SetDayHandler.Max =                               31;
  SetDayHandler.Increment =                         1;
  SetDayHandler.EncoderRotatedFunction =            &UpdateUint8Value;
  SetDayHandler.DisplayFunction =                   &DisplayTimeSetting;
  SetDayHandler.ButtonClickedFunction =             &SwitchToSetHours;
  SetDayHandler.ButtonDoubleClickedFunction =       &SwitchToSetMonth;
  SetDayHandler.ButtonHeldFunction =                &NullFunction;
  SetDayHandler.ButtonReleasedFunction =            &SwitchToTemperature;

  SetHoursHandler.id =                              6;
  SetHoursHandler.uint8_value =                     &temp_tm.tm_hour;
  SetHoursHandler.Min =                             0;
  SetHoursHandler.Max =                             23;
  SetHoursHandler.Increment =                       1;
  SetHoursHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetHoursHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetHoursHandler.ButtonClickedFunction =           &SwitchToSetMinutes;
  SetHoursHandler.ButtonDoubleClickedFunction =     &SwitchToSetDay;
  SetHoursHandler.ButtonHeldFunction =              &NullFunction;
  SetHoursHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetMinutesHandler.id =                            7;
  SetMinutesHandler.uint8_value =                   &temp_tm.tm_min;
  SetMinutesHandler.Min =                           0;
  SetMinutesHandler.Max =                           59;
  SetMinutesHandler.Increment =                     1;
  SetMinutesHandler.EncoderRotatedFunction =        &UpdateUint8Value;
  SetMinutesHandler.DisplayFunction =               &DisplayTimeSetting;
  #ifdef LCD
  SetMinutesHandler.ButtonClickedFunction =         &SetTime;
  #else
  SetMinutesHandler.ButtonClickedFunction =         &SwitchToSetSeconds;
  #endif
  SetMinutesHandler.ButtonDoubleClickedFunction =   &SwitchToSetHours;
  SetMinutesHandler.ButtonHeldFunction =            &NullFunction;
  SetMinutesHandler.ButtonReleasedFunction =        &SwitchToTemperature;

  SetSecondsHandler.id  =                           8;
  SetSecondsHandler.uint8_value =                   &temp_tm.tm_sec;
  SetSecondsHandler.Min =                           0;
  SetSecondsHandler.Max =                           59;
  SetSecondsHandler.Increment =                     1;
  SetSecondsHandler.EncoderRotatedFunction =        &UpdateUint8Value;
  SetSecondsHandler.DisplayFunction =               &DisplayTimeSetting;
  SetSecondsHandler.ButtonClickedFunction =         &SetTime;
  SetSecondsHandler.ButtonDoubleClickedFunction =   &SwitchToSetMinutes;
  SetSecondsHandler.ButtonHeldFunction =            &NullFunction;
  SetSecondsHandler.ButtonReleasedFunction =        &SwitchToTemperature;

  SleepHandler.id  =                                9;
  SleepHandler.uint8_value =                        NULL;
  SleepHandler.Min =                                0;
  SleepHandler.Max =                                0;
  SleepHandler.Increment =                          0;
  SleepHandler.EncoderRotatedFunction =             &SwitchToTemperature;
  SleepHandler.DisplayFunction =                    &clearDisplay;
  SleepHandler.ButtonClickedFunction =              &SwitchToTemperature;
  SleepHandler.ButtonDoubleClickedFunction =        &SwitchToTemperature;
  SleepHandler.ButtonHeldFunction =                 &NullFunction;
  SleepHandler.ButtonReleasedFunction =             &SwitchToTemperature;

  OffHandler.id  =                                  10;
  OffHandler.uint8_value =                          NULL;
  OffHandler.Min =                                  0;
  OffHandler.Max =                                  0;
  OffHandler.Increment =                            0;
  OffHandler.EncoderRotatedFunction =               &NullFunction;
  OffHandler.DisplayFunction =                      &DisplayOffStatus;
  OffHandler.ButtonClickedFunction =                &NullFunction;
  OffHandler.ButtonDoubleClickedFunction =          &NullFunction;
  OffHandler.ButtonHeldFunction =                   &NullFunction;
  OffHandler.ButtonReleasedFunction =               &ChangeStatus;

  // Initialize global variables
  setpoint = TemperatureHandler.Min;
  temperature = TemperatureHandler.Min;
  valve_target = false;
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);
  prev_valve_time = now;
  LastTouched = now;
  localtime_r(&now, &now_tm);

  // Initialize the weekly schedule
  now_tow = now_tm.tm_wday * 10000 + now_tm.tm_hour * 100 + now_tm.tm_min;
  init_schedule();

  #if DEBUG > 0
    strcpy(timestamp, isotime(&now_tm));
    Serial.print(timestamp);
    Serial.println(F(" Starting"));
  #endif

  // Get previous status from EEPROM
  EEPROM.get(0, status);
  if (status)
  {
     ActiveHandler = &TemperatureHandler;
  }
  else
  {
     ActiveHandler = &OffHandler;
  }

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

    // Get the time and set timestamp
    if (Rtc.IsDateTimeValid())
    {
      now = Rtc.GetTime();
      localtime_r(&now, &now_tm);
      now_tow = now_tm.tm_wday * 10000 + now_tm.tm_hour * 100 + now_tm.tm_min;
      #if DEBUG > 0
        strcpy(timestamp, isotime(&now_tm));
      #endif
    }
    else
    {
      setpoint = TemperatureHandler.Min;
      #if DEBUG > 0
        Serial.println(F("RTC clock failed!"));
      #endif
    }

    if (status)
    {

      GetTemperature();

      if (OverrideTime > 0 && ActiveHandler != &OverrideTimeHandler)
      {
        OverrideTime -= POLLING_TIME / 1000;
      }
      else
      {
        check_schedule();
      }

      if (status & now > LastTouched + SLEEP_AFTER)
      {
        ActiveHandler = &SleepHandler;
      }

      select_valve_status();
    }
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
    #if DEBUG > 0
      Serial.print(timestamp);
      Serial.println(F(" MCP9808 failed!"));
    #endif
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
  int stepIdx = 0;
  float newtemp = TemperatureHandler.Min;
  programStep step;

  while (stepIdx < MAX_WEEKLY_STEPS)
  {
    EEPROM.get(programStepsBaseAddress + stepIdx * sizeof(programStep), step);
    if (step.tow > now_tow) break;
    newtemp = step.temperature;
    stepIdx++;
  }

  if (stepIdx == currentStep) return;  // Otherwise we reset any temporary manual override!

  #if DEBUG > 2
    if (setpoint != newtemp)
    {
      Serial.print(timestamp);
      Serial.print(F(" New setpoint: "));
      Serial.println(newtemp);
    }
  #endif

  setpoint = newtemp;
  currentStep = stepIdx;
}


void putStep(int stepIdx, programStep step)
{
  programStep tempStep;
  stepIdx = programStepsBaseAddress + stepIdx * sizeof(programStep);

  #if DEBUG > 2
    Serial.print(F("Storing "));
    Serial.print(step.tow);
    Serial.print(F("/"));
    Serial.print(step.temperature);
    Serial.print(F(" @ "));
    Serial.print(stepIdx);
  #endif

  EEPROM.get(stepIdx, tempStep);

  if (step.tow != tempStep.tow || step.temperature != tempStep.temperature)
  {
    EEPROM.put(stepIdx, step);
    #if DEBUG > 2
      Serial.println(F(" Done!"));
    #endif
  }
  #if DEBUG > 2
  else
  {
      Serial.println(F(" Same, skipping!"));
  }
  #endif

}


void init_schedule()
{
  // Initialize the weekly schedule
  programStep tempStep;

  // This is just for testing. Must be replaced with code to get values from the controller...)
  tempStep.tow = now_tow;
  tempStep.temperature = 25.0;
  putStep(0, tempStep);

  tempStep.tow = now_tow + 1;  // This is wrong, I know. OK unless at the turn of the hour...
  tempStep.temperature = TEMP_MAX;
  putStep(1, tempStep);

  tempStep.tow = now_tow + 2;  // This is wrong, I know. OK unless at the turn of the hour...
  tempStep.temperature = TEMP_MIN;
  putStep(2, tempStep);

  tempStep.tow = ((now_tm.tm_wday + 1) % 7) * 10000;     // Tomorrow's 00:00:00
  tempStep.temperature = 25.0;
  putStep(3, tempStep);

  tempStep.tow = ((now_tm.tm_wday + 1) % 7) * 10000 + 1; // Tomorrow's 00:01:00
  tempStep.temperature = TEMP_MAX;
  putStep(4, tempStep);

  tempStep.tow = ((now_tm.tm_wday + 1) % 7) * 10000 + 2; // Tomorrow's 00:02:00
  tempStep.temperature = TEMP_MIN;
  putStep(5, tempStep);

  tempStep.tow = 65535;
  tempStep.temperature = TEMP_MIN;

  for (int step = 6; step < MAX_WEEKLY_STEPS; step++)
  {
    putStep(step, tempStep);
  }
}


// GetFormattedOverrideTime
char* GetFormattedOverrideTime (void)
{
  #define OVT_ROUNDING 59UL
  static char str[6];
  uint16_t ovh = (OverrideTime + OVT_ROUNDING) / 3600;
  uint16_t ovm = ((OverrideTime + OVT_ROUNDING) - ovh * 3600L) / 60;
  sprintf(str, "%2.2i:%2.2i", ovh, ovm);
  return str;
}


#ifdef LCD
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
  dtostrf(setpoint, 5, 1, str_temp);
  if (strlen(str_temp) > 5) str_temp[5] = '\0';
  sprintf(lcd_line2, "S%5s %6.6s %2s", str_temp, GetFormattedOverrideTime(), (valve_status == valve_target ? (valve_target ? "ON" : "OF") : (valve_target ? "on" : "of")));
  refresh_lcd();
}


// Display override time on LCD
void DisplayOverrideTime ()
{
  strcpy(lcd_line1, "Override time:");
  strcpy(lcd_line2, GetFormattedOverrideTime());
  refresh_lcd();
}


void DisplayTimeSetting()
{
  strcpy(lcd_line1, "Set the time:");
  isotime_r(&temp_tm, tempString);
  tempString[16] = '\0';
  strcpy(lcd_line2, tempString);
  refresh_lcd();
}

void clearDisplay()
{
  lcd_line1[0] = '\0';
  lcd_line2[0] = '\0';
  refresh_lcd();
}

void DisplayOffStatus()
{
  if (now > LastTouched + SLEEP_AFTER)
  {
    clearDisplay();
  }
  else
  {
    strcpy(lcd_line1, "OFF");
    lcd_line2[0] = '\0';
    refresh_lcd();
  }
}
#endif



#ifdef SH1106
// Display temperature on LCD
void DisplayTemperature ()
{
  u8g2.firstPage();
  do {
    // Temperature
    u8g2.setFont(BIG_FONT);
    u8g2.drawStr(0, 14, "T");
    u8g2.setFont(SMALL_FONT);
    u8g2.drawStr(10, 14, "A:");
    u8g2.setFont(BIG_FONT);
    u8g2.setCursor(32,14);
    dtostrf(temperature, 5, 1, tempString);
    if (strlen(tempString) > 5) tempString[5] = '\0';
    u8g2.print(&tempString[1]);

    // Status
    u8g2.setFont(SMALL_FONT);
    if (valve_target)
    {
      u8g2.drawStr(115, 9, (valve_status == valve_target ? "ON" : "on"));
    }
    else
    {
      u8g2.drawStr(108, 9, (valve_status == valve_target ? "OFF" : "off"));
    }

    // Setpoint
    u8g2.setFont(BIG_FONT);
    u8g2.drawStr(0, 41, "T");
    u8g2.setFont(SMALL_FONT);
    u8g2.drawStr(10, 41, "S:");
    u8g2.setFont(BIG_FONT);
    u8g2.setCursor(32,41);
    dtostrf(setpoint, 5, 1, tempString);
    if (strlen(tempString) > 5) tempString[5] = '\0';
    u8g2.print(&tempString[1]);

    // Time
    static const char * months[] = {MONTHS};
    static const char * weekdays[] = {WEEKDAYS};
    u8g2.setFont(SMALL_FONT);
    u8g2.setCursor(0,63);
    u8g2.print(weekdays[now_tm.tm_wday]);
    u8g2.print(' ');
    u8g2.print(now_tm.tm_mday);
    u8g2.print(' ');
    u8g2.print(months[now_tm.tm_mon]);

    sprintf(tempString, "%2.2i:%2.2i:%2.2i", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
    u8g2.setCursor(73,63);
    u8g2.print(tempString);

    //Override
    if (OverrideTime > 0)
    {
      u8g2.setCursor(94,41);
      u8g2.print(GetFormattedOverrideTime());
    }
  } while (u8g2.nextPage());
}


// Display override time on LCD
void DisplayOverrideTime ()
{
  u8g2.firstPage();
  do {
    u8g2.setFont(MEDIUM_FONT);
    u8g2.drawStr(0, 16, "Override time:");
    u8g2.setFont(HUGE_FONT);
    u8g2.drawStr(0, 45, GetFormattedOverrideTime());
  } while (u8g2.nextPage());
}


void DisplayTimeSetting()
{
  u8g2_uint_t x0 = 0;
  u8g2_uint_t x1 = 0;
  u8g2_uint_t y = 0;

  isotime_r(&temp_tm, tempString);
  tempString[10] = '\0';
  switch (ActiveHandler->id)
  {
    case 3:
      x0 = 0;
      x1 = 30;
      y = 40;
      break;
    case 4:
      x0 = 40;
      x1 = 54;
      y = 40;
      break;
    case 5:
      x0 = 64;
      x1 = 78;
      y = 40;
      break;
    case 6:
      x0 = 0;
      x1 = 14;
      y = 63;
      break;
    case 7:
      x0 = 24;
      x1 = 38;
      y = 63;
      break;
    case 8:
      x0 = 48;
      x1 = 62;
      y = 63;
      break;
  }
  u8g2.firstPage();
  do {
    u8g2.setFont(MEDIUM_FONT);
    u8g2.drawStr(0, 13, "Set the time:");
    u8g2.drawStr(0, 38, tempString);
    u8g2.drawStr(0, 61, &tempString[11]);
    u8g2.drawLine(x0, y, x1, y);
  } while (u8g2.nextPage());
}

void clearDisplay()
{
  u8g2.clearDisplay();
}

void DisplayOffStatus()
{
  if (now > LastTouched + SLEEP_AFTER)
  {
    clearDisplay();
  }
  else
  {
    u8g2.firstPage();
    do {
      u8g2.setFont(HUGE_FONT);
      u8g2.drawStr(45, 40, "OFF");
    } while (u8g2.nextPage());
  }
}
#endif
