#include "temperino.h"


void NullFunction()
{
}

void ChangeStatus()
{
  if (status)
  {
    status = false;
    digitalWrite(RELAY_PIN, LOW);
    handler = &OffHandler;
  }
  else
  {
    status = true;
    handler = &TemperatureHandler;
  }
  EEPROM.put(0, status);
}

void SwitchToSetTime()
{
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Time"));
  #endif
  localtime_r(&now, &tmSettings);
  tmSettings.tm_sec = 0;
  SwitchToSetYear();
}

void SwitchToSetYear()
{
  handler = &SetYearHandler;
}

void SwitchToSetMonth()
{
  handler = &SetMonthHandler;
}

void SwitchToSetDay()
{
  handler = &SetDayHandler;
}

void SwitchToSetHours()
{
  handler = &SetHoursHandler;
}

void SwitchToSetMinutes()
{
  handler = &SetMinutesHandler;
}

void SwitchToSetSeconds()
{
  handler = &SetSecondsHandler;
}

void SetTime()
{
  now = mktime(&tmSettings);
  Rtc.SetTime(&now);
  lastTouched = now;
  localtime_r(&now, &tmNow);
  SwitchToTemperature();
}


void SwitchToOverrideTime()
{
  // Adjust the actual value (which is decremented by the time running in steps of 1 seconds)
  // to the nearest "Increment" value (5 minutes by default)
  uint16_t temp = (overrideTime + OverrideTimeHandler.Increment / 2) / OverrideTimeHandler.Increment;
  overrideTime = temp * OverrideTimeHandler.Increment;

  handler = &OverrideTimeHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Override"));
  #endif
}


void SwitchToTemperature()
{
  handler = &TemperatureHandler;
  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.println(F(" Switching to Temperature"));
  #endif
}


// void ToggleButtonAcceleration()
// {
//   encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
//   #if DEBUG > 1
//     Serial.print(F("Acceleration "));
//     Serial.println((encoder->getAccelerationEnabled()) ? F("ON") : F("OFF"));
//   #endif
// }


void UpdateFloatValue (int16_t value)
{
  *handler->float_value += value * handler->Increment;
  if (*handler->float_value > handler->Max)
  {
    *handler->float_value = handler->Max;
  }
  if (*handler->float_value < handler->Min)
  {
    *handler->float_value = handler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (float): "));
    Serial.println(*handler->float_value);
  #endif
}

void UpdateUint8Value (int16_t value)
{
  int increment = value * handler->Increment;

  if (increment < 0 & *handler->uint8_value < abs(increment))
  {
    *handler->uint8_value = 0;
  }
  else
  {
    *handler->uint8_value += increment;
  }

  if (*handler->uint8_value > handler->Max)
  {
    *handler->uint8_value = handler->Max;
  }
  if (*handler->uint8_value < handler->Min)
  {
    *handler->uint8_value = handler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (uint8): "));
    Serial.println(*handler->uint8_value);
  #endif
}

void UpdateUint16Value (int16_t value)
{
  int increment = value * handler->Increment;

  if (increment < 0 & *handler->uint16_value < abs(increment))
  {
    *handler->uint16_value = 0;
  }
  else
  {
    *handler->uint16_value += increment;
  }

  if (*handler->uint16_value > handler->Max)
  {
    *handler->uint16_value = handler->Max;
  }
  if (*handler->uint16_value < handler->Min)
  {
    *handler->uint16_value = handler->Min;
  }

  #if DEBUG > 1
    Serial.print(timestamp);
    Serial.print(F(" Value (uint16): "));
    Serial.println(*handler->uint16_value);
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
      lastTouched = now;
      handler->EncoderRotatedFunction(value);
    }
  }
  else
  {
    #if DEBUG > 1
      Serial.print(timestamp);
      Serial.print(F(" Button "));
    #endif
    lastTouched = now;
    switch (b)
    {
      case ClickEncoder::Clicked:
        #if DEBUG > 1
          Serial.println(F("Clicked"));
        #endif
        handler->ButtonClickedFunction();
        break;
      case ClickEncoder::DoubleClicked:
        #if DEBUG > 1
          Serial.println(F("DoubleClicked"));
        #endif
        handler->ButtonDoubleClickedFunction();
        break;
      case ClickEncoder::Held:
        #if DEBUG > 1
          Serial.println(F("Held"));
        #endif
        handler->ButtonHeldFunction();
        break;
      case ClickEncoder::Released:
        #if DEBUG > 1
          Serial.println(F("Released"));
        #endif
        handler->ButtonReleasedFunction();
        break;
    }
  }
}


// Get the time
void GetTime()
{
  if (Rtc.IsDateTimeValid())
  {
    now = Rtc.GetTime();
  }
  else
  {
    now = 0;
    clockFailed = true;
    setpoint = TemperatureHandler.Min;
    #if DEBUG > 0
      Serial.println(F("RTC clock failed!"));
    #endif
  }

  localtime_r(&now, &tmNow);
  #if DEBUG > 0
    strcpy(timestamp, isotime(&tmNow));
  #endif
  nowTOW = tmNow.tm_wday * 10000 + tmNow.tm_hour * 100 + tmNow.tm_min;
}


// Get the temperature
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
    tempFailed = true;
  }
#endif

#ifdef DS3231_TEMP
  // RTC Temperature
  temperature = Rtc.GetTemperature();
#endif
  temperature = round(temperature * 10) / 10.0;
}


// See if relay status should be modified
void SetRelay()
{
  if ((abs(setpoint - temperature) > TEMP_HYSTERESIS))
  {
    if (setpoint > temperature)
    {
      relayTarget = true;
    }
    else
    {
      relayTarget = false;
    }

    if (relayStatus != relayTarget)
    {
      if (now - prevActivationTime > RELAY_QUIESCENT_TIME)
      {
        digitalWrite(RELAY_PIN, relayTarget ? HIGH : LOW);
        prevActivationTime = now;
        relayStatus = relayTarget;
        #if DEBUG > 0
          Serial.print(timestamp);
          Serial.println(relayStatus ? F(" Turned on.") : F(" Turned off."));
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
        Serial.println(relayTarget ? F(" Turn on!") : F(" Turn off!"));
      }
      #endif
    }
  }
}


void CheckSchedule()
{
  int stepIdx = 0;
  float newtemp = TemperatureHandler.Min;
  programStep step;

  while (stepIdx < MAX_WEEKLY_STEPS)
  {
    EEPROM.get(programStepsBaseAddress + stepIdx * sizeof(programStep), step);
    if (step.tow > nowTOW) break;
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


void PutStepToEEPROM(int stepIdx, programStep step)
{
  programStep tempStep;
  stepIdx = programStepsBaseAddress + stepIdx * sizeof(programStep);

  #if DEBUG > 3
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
    #if DEBUG > 3
      Serial.println(F(" Done!"));
    #endif
  }
  #if DEBUG > 3
  else
  {
      Serial.println(F(" Same, skipping!"));
  }
  #endif

}


void GetSchedule()
{
  // Initialize the weekly schedule
  programStep tempStep;

  // This is just for testing. Must be replaced with code to get values from the controller...)
  tempStep.tow = nowTOW;
  tempStep.temperature = 25.0;
  PutStepToEEPROM(0, tempStep);

  tempStep.tow = nowTOW + 1;  // This is wrong, I know. OK unless at the turn of the hour...
  tempStep.temperature = TEMP_MAX;
  PutStepToEEPROM(1, tempStep);

  tempStep.tow = nowTOW + 2;  // This is wrong, I know. OK unless at the turn of the hour...
  tempStep.temperature = TEMP_MIN;
  PutStepToEEPROM(2, tempStep);

  tempStep.tow = ((tmNow.tm_wday + 1) % 7) * 10000;     // Tomorrow's 00:00:00
  tempStep.temperature = 25.0;
  PutStepToEEPROM(3, tempStep);

  tempStep.tow = ((tmNow.tm_wday + 1) % 7) * 10000 + 1; // Tomorrow's 00:01:00
  tempStep.temperature = TEMP_MAX;
  PutStepToEEPROM(4, tempStep);

  tempStep.tow = ((tmNow.tm_wday + 1) % 7) * 10000 + 2; // Tomorrow's 00:02:00
  tempStep.temperature = TEMP_MIN;
  PutStepToEEPROM(5, tempStep);

  tempStep.tow = 65535;
  tempStep.temperature = TEMP_MIN;

  for (int step = 6; step < MAX_WEEKLY_STEPS; step++)
  {
    PutStepToEEPROM(step, tempStep);
  }
}


// GetFormattedOverrideTime
char* GetFormattedOverrideTime (void)
{
  #define OVT_ROUNDING 59UL
  static char str[6];
  uint16_t ovh = (overrideTime + OVT_ROUNDING) / 3600;
  uint16_t ovm = ((overrideTime + OVT_ROUNDING) - ovh * 3600L) / 60;
  sprintf(str, "%2.2i:%2.2i", ovh, ovm);
  return str;
}


#ifdef LCD
// Actually print the content of the two line buffers on the LCD
void RefreshLCD()
{
  lcd.setCursor(0, 0);
  lcd.print(lcdLine1);
  for (int k = strlen(lcdLine1); k < 16; k++) lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print(lcdLine2);
  for (int k = strlen(lcdLine2); k < 16; k++) lcd.print(" ");
}


// Main display page
void DisplayTemperature()
{
  char str_temp[16];
  char str_ovt[6];
  dtostrf(temperature, 5, 1, str_temp);
  sprintf(lcdLine1, "A%5s  %2i:%2.2i:%2.2i", str_temp, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  dtostrf(setpoint, 5, 1, str_temp);
  sprintf(lcdLine2, "S%5s %6.6s %2s", str_temp, GetFormattedOverrideTime(), (relayStatus == relayTarget ? (relayTarget ? "ON" : "OF") : (relayTarget ? "on" : "of")));
  RefreshLCD();
}


// Display page for the override time setting
void DisplayOverrideTime()
{
  strcpy(lcdLine1, OVERRIDE_TIME_SETTINGS_MSG);
  strcpy(lcdLine2, GetFormattedOverrideTime());
  RefreshLCD();
}


// Display page for the time setting
void DisplayTimeSetting()
{
  strcpy(lcdLine1, TIME_SETTINGS_MSG);
  isotime_r(&tmSettings, tempString);
  tempString[16] = '\0';
  strcpy(lcdLine2, tempString);
  RefreshLCD();
}


// Turn the display off
void DisplayOFF()
{
  lcdLine1[0] = '\0';
  lcdLine2[0] = '\0';
  RefreshLCD();
}


// Display the OFF message when the system is inactive
void DisplayOffStatus()
{
  if (now > lastTouched + SLEEP_AFTER)
  {
    DisplayOFF();
  }
  else
  {
    strcpy(lcdLine1, "OFF");
    lcdLine2[0] = '\0';
    RefreshLCD();
  }
}
#endif



#ifdef SH1106
// Main display page
void DisplayTemperature()
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
    if (tempFailed)
    {
      u8g2.print(TEMP_FAILED_MSG);
    }
    else
    {
      dtostrf(temperature, 5, 1, tempString);
      if (temperature > 0)
      {
        u8g2.print(&tempString[1]);
      }
      else
      {
        u8g2.print(tempString);
      }
    }

    // Status
    u8g2.setFont(SMALL_FONT);
    if (relayTarget)
    {
      u8g2.drawStr(115, 9, (relayStatus == relayTarget ? "ON" : "on"));
    }
    else
    {
      u8g2.drawStr(108, 9, (relayStatus == relayTarget ? "OFF" : "off"));
    }

    // Setpoint
    u8g2.setFont(BIG_FONT);
    u8g2.drawStr(0, 41, "T");
    u8g2.setFont(SMALL_FONT);
    u8g2.drawStr(10, 41, "S:");
    u8g2.setFont(BIG_FONT);
    u8g2.setCursor(32,41);
    dtostrf(setpoint, 5, 1, tempString);
    u8g2.print(&tempString[1]);

    // Time
    static const char * months[] = {MONTHS};
    static const char * weekdays[] = {WEEKDAYS};
    u8g2.setFont(SMALL_FONT);
    u8g2.setCursor(0,63);
    if (clockFailed)
    {
      u8g2.print(CLOCK_FAILED_MSG);
    }
    else
    {
      u8g2.print(weekdays[tmNow.tm_wday]);
      u8g2.print(' ');
      u8g2.print(tmNow.tm_mday);
      u8g2.print(' ');
      u8g2.print(months[tmNow.tm_mon]);
      sprintf(tempString, "%2.2i:%2.2i:%2.2i", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
      u8g2.setCursor(73,63);
      u8g2.print(tempString);
    }

    //Override
    if (overrideTime > 0)
    {
      u8g2.setCursor(94,41);
      u8g2.print(GetFormattedOverrideTime());
    }
  } while (u8g2.nextPage());
}


// Display page for the override time setting
void DisplayOverrideTime()
{
  u8g2.firstPage();
  do {
    u8g2.setFont(MEDIUM_FONT);
    u8g2.drawStr(0, 16, OVERRIDE_TIME_SETTINGS_MSG);
    u8g2.setFont(HUGE_FONT);
    u8g2.drawStr(0, 45, GetFormattedOverrideTime());
  } while (u8g2.nextPage());
}


// Display page for the time setting
void DisplayTimeSetting()
{
  u8g2_uint_t x0 = 0;
  u8g2_uint_t x1 = 0;
  u8g2_uint_t y = 0;

  isotime_r(&tmSettings, tempString);
  tempString[10] = '\0';
  switch (handler->id)
  {
    case 1:
      x0 = 0;
      x1 = 30;
      y = 40;
      break;
    case 2:
      x0 = 40;
      x1 = 54;
      y = 40;
      break;
    case 3:
      x0 = 64;
      x1 = 78;
      y = 40;
      break;
    case 4:
      x0 = 0;
      x1 = 14;
      y = 63;
      break;
    case 5:
      x0 = 24;
      x1 = 38;
      y = 63;
      break;
    case 6:
      x0 = 48;
      x1 = 62;
      y = 63;
      break;
  }
  u8g2.firstPage();
  do {
    u8g2.setFont(MEDIUM_FONT);
    u8g2.drawStr(0, 13, TIME_SETTINGS_MSG);
    u8g2.drawStr(0, 38, tempString);
    u8g2.drawStr(0, 61, &tempString[11]);
    u8g2.drawLine(x0, y, x1, y);
  } while (u8g2.nextPage());
}


// Turn the display off
void DisplayOFF()
{
  u8g2.clearDisplay();
}


// Display the OFF message when the system is inactive
void DisplayOffStatus()
{
  if (now > lastTouched + SLEEP_AFTER)
  {
    DisplayOFF();
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
  #if DST_RULES == EU
    set_dst(&eu_dst);
  #endif

  // Check if the RTC clock is running (Yes, it can be stopped!)
  if (!Rtc.GetIsRunning())
  {
    #if DEBUG > 0
      Serial.println(F("WARNING: RTC wasn't running, starting it now."));
    #endif
    Rtc.SetIsRunning(true);
  }

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
  TemperatureHandler.id =                           0;
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

  SetYearHandler.id =                               1;
  SetYearHandler.uint16_value =                     &tmSettings.tm_year;
  SetYearHandler.Min =                              117;
  SetYearHandler.Max =                              199;
  SetYearHandler.Increment =                        1;
  SetYearHandler.EncoderRotatedFunction =           &UpdateUint16Value;
  SetYearHandler.DisplayFunction =                  &DisplayTimeSetting;
  SetYearHandler.ButtonClickedFunction =            &SwitchToSetMonth;
  SetYearHandler.ButtonDoubleClickedFunction =      &SwitchToTemperature;
  SetYearHandler.ButtonHeldFunction =               &NullFunction;
  SetYearHandler.ButtonReleasedFunction =           &SwitchToTemperature;

  SetMonthHandler.id =                              2;
  SetMonthHandler.uint8_value =                     &tmSettings.tm_mon;
  SetMonthHandler.Min =                             0;
  SetMonthHandler.Max =                             11;
  SetMonthHandler.Increment =                       1;
  SetMonthHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetMonthHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetMonthHandler.ButtonClickedFunction =           &SwitchToSetDay;
  SetMonthHandler.ButtonDoubleClickedFunction =     &SwitchToSetYear;
  SetMonthHandler.ButtonHeldFunction =              &NullFunction;
  SetMonthHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetDayHandler.id =                                3;
  SetDayHandler.uint8_value =                       &tmSettings.tm_mday;
  SetDayHandler.Min =                               1;
  SetDayHandler.Max =                               31;
  SetDayHandler.Increment =                         1;
  SetDayHandler.EncoderRotatedFunction =            &UpdateUint8Value;
  SetDayHandler.DisplayFunction =                   &DisplayTimeSetting;
  SetDayHandler.ButtonClickedFunction =             &SwitchToSetHours;
  SetDayHandler.ButtonDoubleClickedFunction =       &SwitchToSetMonth;
  SetDayHandler.ButtonHeldFunction =                &NullFunction;
  SetDayHandler.ButtonReleasedFunction =            &SwitchToTemperature;

  SetHoursHandler.id =                              4;
  SetHoursHandler.uint8_value =                     &tmSettings.tm_hour;
  SetHoursHandler.Min =                             0;
  SetHoursHandler.Max =                             23;
  SetHoursHandler.Increment =                       1;
  SetHoursHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetHoursHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetHoursHandler.ButtonClickedFunction =           &SwitchToSetMinutes;
  SetHoursHandler.ButtonDoubleClickedFunction =     &SwitchToSetDay;
  SetHoursHandler.ButtonHeldFunction =              &NullFunction;
  SetHoursHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetMinutesHandler.id =                            5;
  SetMinutesHandler.uint8_value =                   &tmSettings.tm_min;
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

  SetSecondsHandler.id  =                           6;
  SetSecondsHandler.uint8_value =                   &tmSettings.tm_sec;
  SetSecondsHandler.Min =                           0;
  SetSecondsHandler.Max =                           59;
  SetSecondsHandler.Increment =                     1;
  SetSecondsHandler.EncoderRotatedFunction =        &UpdateUint8Value;
  SetSecondsHandler.DisplayFunction =               &DisplayTimeSetting;
  SetSecondsHandler.ButtonClickedFunction =         &SetTime;
  SetSecondsHandler.ButtonDoubleClickedFunction =   &SwitchToSetMinutes;
  SetSecondsHandler.ButtonHeldFunction =            &NullFunction;
  SetSecondsHandler.ButtonReleasedFunction =        &SwitchToTemperature;

  OverrideTimeHandler.id =                          7;
  OverrideTimeHandler.uint16_value =                &overrideTime;
  OverrideTimeHandler.Min =                         0;
  OverrideTimeHandler.Max =                         OVERRIDE_TIME_MAX;
  OverrideTimeHandler.Increment =                   OVERRIDE_TIME_INCREMENT;
  OverrideTimeHandler.EncoderRotatedFunction =      &UpdateUint16Value;
  OverrideTimeHandler.DisplayFunction =             &DisplayOverrideTime;
  OverrideTimeHandler.ButtonClickedFunction =       &SwitchToTemperature;
  OverrideTimeHandler.ButtonDoubleClickedFunction = &NullFunction;
  OverrideTimeHandler.ButtonHeldFunction =          &NullFunction;
  OverrideTimeHandler.ButtonReleasedFunction =      &NullFunction;

  SleepHandler.id  =                                8;
  SleepHandler.uint8_value =                        NULL;
  SleepHandler.Min =                                0;
  SleepHandler.Max =                                0;
  SleepHandler.Increment =                          0;
  SleepHandler.EncoderRotatedFunction =             &SwitchToTemperature;
  SleepHandler.DisplayFunction =                    &DisplayOFF;
  SleepHandler.ButtonClickedFunction =              &SwitchToTemperature;
  SleepHandler.ButtonDoubleClickedFunction =        &SwitchToTemperature;
  SleepHandler.ButtonHeldFunction =                 &NullFunction;
  SleepHandler.ButtonReleasedFunction =             &SwitchToTemperature;

  OffHandler.id  =                                  0;
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
  relayTarget = false;
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Initialize the weekly schedule
  GetTime();
  GetSchedule();
  prevActivationTime = now;
  lastTouched = now;

  // Get previous status from EEPROM
  EEPROM.get(0, status);
  if (status)
  {
     handler = &TemperatureHandler;
  }
  else
  {
     handler = &OffHandler;
  }

  #if DEBUG > 0
    strcpy(timestamp, isotime(&tmNow));
    Serial.print(timestamp);
    Serial.println(F(" Starting"));
  #endif

  // End of Setup
}


// Main loop
void loop()
{

  relayStatus = (bool) digitalRead(RELAY_PIN);

  #if DEBUG > 8
    loops++;
  #endif

  EncoderDispatcher();

  if (millis() - prevMillis >= POLLING_TIME)
  {
    prevMillis += POLLING_TIME;

    #if DEBUG > 8
      PrintLoops();
    #endif

    GetTime();

    if (status)
    {
      GetTemperature();

      if (overrideTime > 0 && handler != &OverrideTimeHandler)
      {
        overrideTime -= POLLING_TIME / 1000;
      }
      else if (!clockFailed)
      {
        CheckSchedule();
      }

      SetRelay();

      if (handler == &TemperatureHandler & now > lastTouched + SLEEP_AFTER)
      {
        handler = &SleepHandler;
      }
    }
  }

  // Display status on LCD
  handler->DisplayFunction();

  // End of Loop
}
