#include "temperino.h"

void serialEvent()
{
  #define SERBUF_SIZE 20
  #define MIN_CMD 1
  #define MAX_CMD 6

  int addr;
  int cmd;
  int stepIdx;
  programStep step;
  uint8_t len;
  char serbuf[SERBUF_SIZE];
  char cmdString[SERBUF_SIZE];
  static char delimiters[] = " ,";
  char *token;
  bool ok;
  float tempTemp;
  time_t tempTime;

  len = mySerial.readBytesUntil('\n', serbuf, SERBUF_SIZE-1);
  serbuf[len] = '\0';
  strcpy(cmdString, serbuf);
  addr = atoi(strtok(cmdString, delimiters));

  if (addr == MY_ADDR)
  {
    cmd = atoi(strtok(NULL, delimiters));

    if (cmd >= MIN_CMD && cmd <= MAX_CMD)
    {
      ok = true;
      switch (cmd)
      {
        case 1: // Read temperature
          mySerial.print(MY_ADDR);
          mySerial.print(',');
          mySerial.print(cmd);
          mySerial.print(',');
          mySerial.println(temperature);
          break;
        case 2: // Read/Set setpoint
          if ((token = strtok(NULL, delimiters)) != NULL)
          {
            tempTemp = atof(token);
            if (tempTemp >= TEMP_MIN && tempTemp <= TEMP_MAX)
            {
              setpoint = tempTemp;
              if (overrideTime <= now)
              {
                overrideTime += DEFAULT_OVERRIDE_TIME;
                EEPROM.put(EEPROMoverrideTimeAddress, overrideTime);
              }
            }
            else
            {
              ok = false;
            }
          }
          if (ok)
          {
            mySerial.print(MY_ADDR);
            mySerial.print(',');
            mySerial.print(cmd);
            mySerial.print(',');
            mySerial.println(setpoint);
          }
          break;
        case 3: // Read/Set time
          if ((token = strtok(NULL, delimiters)) != NULL)
          {
            tempTime = atot(token);
            if (tempTime >= 0)
            {
              now = tempTime - UNIX_OFFSET;
              Rtc.SetTime(&now);
              localtime_r(&now, &tmNow);
            }
            else
            {
              ok = false;
            }
          }
          if (ok)
          {
            mySerial.print(MY_ADDR);
            mySerial.print(',');
            mySerial.print(cmd);
            mySerial.print(',');
            mySerial.println(now + UNIX_OFFSET);
          }
          break;
        case 4: // Read/Set override time
          if ((token = strtok(NULL, delimiters)) != NULL)
          {
            tempTime = atot(token);
            if (tempTime >= 0)
            {
              overrideTime = tempTime - UNIX_OFFSET;
              EEPROM.put(EEPROMoverrideTimeAddress, overrideTime);
            }
            else
            {
              ok = false;
            }
          }
          if (ok)
          {
            mySerial.print(MY_ADDR);
            mySerial.print(',');
            mySerial.print(cmd);
            mySerial.print(',');
            mySerial.println(overrideTime + UNIX_OFFSET);
          }
          break;
        case 5: // Read timestamp
          mySerial.print(MY_ADDR);
          mySerial.print(',');
          mySerial.print(cmd);
          mySerial.print(',');
          mySerial.println(timestamp);
          break;
        case 6: // Read/Set schedule
          if ((token = strtok(NULL, delimiters)) == NULL)
          {
            for (stepIdx = 0; stepIdx < MAX_WEEKLY_STEPS; stepIdx++)
            {
              EEPROM.get(EEPROMstepAddress(stepIdx), step);
              mySerial.print(MY_ADDR);
              mySerial.print(',');
              mySerial.print(cmd);
              mySerial.print(',');
              mySerial.print(stepIdx);
              mySerial.print(',');
              mySerial.print(step.tow);
              mySerial.print(',');
              mySerial.println(step.temperature);
            }
          }
          else
          {
            stepIdx = atoi(token);
            if (stepIdx < 0 || stepIdx >= MAX_WEEKLY_STEPS)
            {
              ok = false;
            }
            else
            {
              if ((token = strtok(NULL, delimiters)) == NULL)
              {
                EEPROM.get(EEPROMstepAddress(stepIdx), step);
                mySerial.print(MY_ADDR);
                mySerial.print(',');
                mySerial.print(cmd);
                mySerial.print(',');
                mySerial.print(stepIdx);
                mySerial.print(',');
                mySerial.print(step.tow);
                mySerial.print(',');
                mySerial.println(step.temperature);
              }
              else
              {
                step.tow = atoi(token);
                if (step.tow < 0 || step.tow >= 62359)
                {
                  ok = false;
                }
                else
                {
                  if ((token = strtok(NULL, delimiters)) == NULL)
                  {
                    ok = false;
                  }
                  else
                  {
                    step.temperature = atof(token);
                    if (step.temperature < TEMP_MIN || step.temperature > TEMP_MAX)
                    {
                      ok = false;
                    }
                    else
                    {
                      PutStepToEEPROM(stepIdx, step);
                      mySerial.print(MY_ADDR);
                      mySerial.print(',');
                      mySerial.print(cmd);
                      mySerial.print(',');
                      mySerial.print(stepIdx);
                      mySerial.print(',');
                      mySerial.print(step.tow);
                      mySerial.print(',');
                      mySerial.println(step.temperature);
                    }
                  }
                }
              }
            }
          }
          break;
        default:
          ok = false;
      }
    }
    else
    {
      ok = false;
    }

    if (!ok)
    {
      mySerial.print(MY_ADDR);
      mySerial.print(",0,\"");
      mySerial.print(serbuf);
      mySerial.println('"');
    }

  }
}


time_t atot(char *str)
{
  time_t t = 0;
  uint8_t i;
  uint8_t len = strlen(str);

  for (i = 0; i < len; i++)
  {
    t *= 10;
    t += str[i] - '0';
  }

  return t;
}


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
  EEPROM.put(EEPROMstatusAddress, status);
}


void WakingUp()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Waking Up."));
  #endif
  SleepHandler.DisplayFunction = &DisplayTemperature;
}


void CheckIdle()
{
  if (handler == &TemperatureHandler && now > lastTouched + SLEEP_AFTER)
  {
    SleepHandler.DisplayFunction = &DisplayOFF;
    handler = &SleepHandler;
   }

  if (handler == &SleepHandler &&
      SleepHandler.DisplayFunction == DisplayTemperature &&
      now > lastTouched + 1)
  {
    SwitchToTemperature();
  }
}


void SwitchToSetTime()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Switching to Time"));
  #endif
  localtime_r(&now, &tmSettings);
  tmSettings.tm_sec = 0;
  SetYearHandler.tm_base =    &tmSettings;
  SetMonthHandler.tm_base =   &tmSettings;
  SetDayHandler.tm_base =     &tmSettings;
  SetHoursHandler.tm_base =   &tmSettings;
  SetMinutesHandler.tm_base = &tmSettings;
  SetSecondsHandler.tm_base = &tmSettings;
  settingOverride = false;
  SwitchToSetYear();
}


void SwitchToSetOverride()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Switching to Override"));
  #endif
  if (overrideTime < now)
  {
    overrideTime = now + DEFAULT_OVERRIDE_TIME;
    EEPROM.put(EEPROMoverrideTimeAddress, overrideTime);
  }
  localtime_r(&overrideTime, &tmOverride);
  tmOverride.tm_sec = 0;
  SetYearHandler.tm_base =    &tmOverride;
  SetMonthHandler.tm_base =   &tmOverride;
  SetDayHandler.tm_base =     &tmOverride;
  SetHoursHandler.tm_base =   &tmOverride;
  SetMinutesHandler.tm_base = &tmOverride;
  SetSecondsHandler.tm_base = &tmOverride;
  settingOverride = true;
  SwitchToSetYear();
}


void SwitchToSetYear()
{
  SetYearHandler.uint16_value = &SetYearHandler.tm_base->tm_year;
  handler = &SetYearHandler;
}


void SwitchToSetMonth()
{
  SetMonthHandler.uint8_value = &SetMonthHandler.tm_base->tm_mon;
  handler = &SetMonthHandler;
}


void SwitchToSetDay()
{
  SetDayHandler.uint8_value = &SetDayHandler.tm_base->tm_mday;
  handler = &SetDayHandler;
}


void SwitchToSetHours()
{
  SetHoursHandler.uint8_value = &SetHoursHandler.tm_base->tm_hour;
  handler = &SetHoursHandler;
}


void SwitchToSetMinutes()
{
  SetMinutesHandler.uint8_value = &SetMinutesHandler.tm_base->tm_min;
  handler = &SetMinutesHandler;
}


void SwitchToSetSeconds()
{
  if (settingOverride)
  {
    SetTime();
  }
  else
  {
    SetSecondsHandler.uint8_value = &SetSecondsHandler.tm_base->tm_sec;
    handler = &SetSecondsHandler;
  }
}


void SetTime()
{
  if (settingOverride)
  {
    overrideTime = mktime(&tmOverride);
    EEPROM.put(EEPROMoverrideTimeAddress, overrideTime);
  }
  else
  {
    now = mktime(&tmSettings);
    Rtc.SetTime(&now);
    lastTouched = now;
    localtime_r(&now, &tmNow);
  }
  SwitchToTemperature();
}


void SwitchToTemperature()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Switching to Temperature"));
  #endif
  handler = &TemperatureHandler;
}


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
    mySerial.print(timestamp);
    mySerial.print(F(" Value (float): "));
    mySerial.println(*handler->float_value);
  #endif
}


void UpdateUint8Value (int16_t value)
{
  int increment = value * handler->Increment;

  if (increment < 0 && *handler->uint8_value < abs(increment))
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
    mySerial.print(timestamp);
    mySerial.print(F(" Value (uint8): "));
    mySerial.println(*handler->uint8_value);
  #endif
}


void UpdateUint16Value (int16_t value)
{
  int increment = value * handler->Increment;

  if (increment < 0 && *handler->uint16_value < abs(increment))
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
    mySerial.print(timestamp);
    mySerial.print(F(" Value (uint16): "));
    mySerial.println(*handler->uint16_value);
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
      if (handler == &TemperatureHandler & overrideTime < now)
      {
        overrideTime = now + DEFAULT_OVERRIDE_TIME;
        EEPROM.put(EEPROMoverrideTimeAddress, overrideTime);
      }
    }
  }
  else
  {
    #if DEBUG > 1
      mySerial.print(timestamp);
      mySerial.print(F(" Button "));
    #endif
    lastTouched = now;
    switch (b)
    {
      case ClickEncoder::Clicked:
        #if DEBUG > 1
          mySerial.println(F("Clicked"));
        #endif
        handler->ButtonClickedFunction();
        break;
      case ClickEncoder::DoubleClicked:
        #if DEBUG > 1
          mySerial.println(F("DoubleClicked"));
        #endif
        handler->ButtonDoubleClickedFunction();
        break;
      case ClickEncoder::Held:
        #if DEBUG > 1
          mySerial.println(F("Held"));
        #endif
        handler->ButtonHeldFunction();
        break;
      case ClickEncoder::Released:
        #if DEBUG > 1
          mySerial.println(F("Released"));
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
    clockFailed = false;
  }
  else
  {
    now = 0;
    clockFailed = true;
    setpoint = TemperatureHandler.Min;
    #if DEBUG > 0
      mySerial.println(F("RTC clock failed!"));
    #endif
  }

  localtime_r(&now, &tmNow);
  strcpy(timestamp, isotime(&tmNow));
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
    tempFailed = false;
  }
  else
  {
    #if DEBUG > 0
      mySerial.print(timestamp);
      mySerial.println(F(" MCP9808 failed!"));
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
    relayTarget = setpoint > temperature;

    if (relayStatus != relayTarget)
    {
      if (now - prevActivationTime > RELAY_QUIESCENT_TIME)
      {
        digitalWrite(RELAY_PIN, relayTarget ? HIGH : LOW);
        prevActivationTime = now;
        relayStatus = relayTarget;
        #if DEBUG > 0
          mySerial.print(timestamp);
          mySerial.println(relayStatus ? F(" Turned on.") : F(" Turned off."));
        #endif
      }
      #if DEBUG > 0
      else
      {
        mySerial.print(timestamp);
        mySerial.print(F(" Setpoint: "));
        mySerial.print(setpoint);
        mySerial.print(F(" Temp: "));
        mySerial.print(temperature);
        mySerial.println(relayTarget ? F(" Turn on!") : F(" Turn off!"));
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

  if (overrideTime > now) return;

  while (stepIdx < MAX_WEEKLY_STEPS)
  {
    EEPROM.get(EEPROMstepAddress(stepIdx), step);
    if (step.tow > nowTOW) break;
    newtemp = step.temperature;
    stepIdx++;
  }

  setpoint = newtemp;

  #if DEBUG > 2
    if (setpoint != newtemp)
    {
      mySerial.print(timestamp);
      mySerial.print(F(" New setpoint: "));
      mySerial.println(setpoint);
    }
  #endif
}


void PutStepToEEPROM(int stepIdx, programStep step)
{
  programStep tempStep;
  stepIdx = EEPROMstepAddress(stepIdx);

  #if DEBUG > 3
    mySerial.print(F("Storing "));
    mySerial.print(step.tow);
    mySerial.print(F("/"));
    mySerial.print(step.temperature);
    mySerial.print(F(" @ "));
    mySerial.print(stepIdx);
  #endif

  EEPROM.get(stepIdx, tempStep);

  if (step.tow != tempStep.tow || step.temperature != tempStep.temperature)
  {
    EEPROM.put(stepIdx, step);
    #if DEBUG > 3
      mySerial.println(F(" Done!"));
    #endif
  }
  #if DEBUG > 3
  else
  {
      mySerial.println(F(" Same, skipping!"));
  }
  #endif

}


// Init the weekly schedule (TEMPORARY FUNCTION, TO BE REMOVED)
void SetSchedule()
{
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
  dtostrf(temperature, 5, 1, tempString);
  sprintf(lcdLine1, "A%5s  %2.2i:%2.2i:%2.2i",
    tempString,
    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  dtostrf(setpoint, 5, 1, tempString);
  sprintf(lcdLine2, "S%5s  %3s  %3s",
    tempString,
    (overrideTime > now ? "OVR" : ""),
    (relayStatus == relayTarget ? (relayTarget ? " ON" : "OFF") : (relayTarget ? " on" : "off")));
  RefreshLCD();
}


// Display page for the time setting
void DisplayTimeSetting()
{
    if (settingOverride)
    {
      strcpy(lcdLine1, OVERRIDE_TIME_SETTINGS_MSG);
      isotime_r(&tmOverride, tempString);
    }
    else
    {
      strcpy(lcdLine1, TIME_SETTINGS_MSG);
      isotime_r(&tmSettings, tempString);
    }
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
    if (tempFailed)
    {
      u8g2.drawStr(32, 14, TEMP_FAILED_MSG);
    }
    else
    {
      dtostrf(temperature, 5, 1, tempString);
      if (temperature > 0)
      {
        u8g2.drawStr(32, 14, &tempString[1]);
      }
      else
      {
        u8g2.drawStr(32, 14, tempString);
      }
    }

    // Status
    u8g2.setFont(SMALL_FONT);
    u8g2.drawStr(108, 9, (relayStatus == relayTarget ? (relayTarget ? " ON" : "OFF") : (relayTarget ? " on" : "off")));

    // Setpoint
    u8g2.setFont(BIG_FONT);
    u8g2.drawStr(0, 41, "T");
    u8g2.setFont(SMALL_FONT);
    u8g2.drawStr(10, 41, "S:");
    u8g2.setFont(BIG_FONT);
    dtostrf(setpoint, 5, 1, tempString);
    u8g2.drawStr(32, 41, &tempString[1]);

    // Time
    static const char * months[] = {MONTHS};
    static const char * weekdays[] = {WEEKDAYS};
    u8g2.setFont(SMALL_FONT);
    if (clockFailed)
    {
      u8g2.drawStr(0, 63, CLOCK_FAILED_MSG);
    }
    else
    {
      sprintf(tempString, "%2s %i %3s", weekdays[tmNow.tm_wday], tmNow.tm_mday, months[tmNow.tm_mon]);
      u8g2.drawStr(0, 63, tempString);
      sprintf(tempString, "%2.2i:%2.2i:%2.2i", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
      u8g2.drawStr(73, 63, tempString);
    }

    //Override
    if (overrideTime > now)
    {
      u8g2.drawStr(108, 41, "OVR");
    }
  } while (u8g2.nextPage());
}


// Display page for the time setting
void DisplayTimeSetting()
{
  u8g2_uint_t x0 = 0;
  u8g2_uint_t x1 = 0;
  u8g2_uint_t y = 0;

  if (settingOverride)
  {
    isotime_r(&tmOverride, tempString);
    tempString[16] = '\0';
  }
  else
  {
    isotime_r(&tmSettings, tempString);
  }

  tempString[10] = '\0';

  if (handler == &SetYearHandler)
  {
      x0 = 0;
      x1 = 45;
      y = 40;
  }
  else if (handler == &SetMonthHandler)
  {
      x0 = 60;
      x1 = 81;
      y = 40;
  }
  else if (handler == &SetDayHandler)
  {
      x0 = 96;
      x1 = 117;
      y = 40;
  }
  else if (handler == &SetHoursHandler)
  {
      x0 = 0;
      x1 = 21;
      y = 63;
  }
  else if (handler == &SetMinutesHandler)
  {
      x0 = 36;
      x1 = 57;
      y = 63;
  }
  else if (handler == &SetSecondsHandler)
  {
      x0 = 72;
      x1 = 93;
      y = 63;
  }

  u8g2.firstPage();
  do {
    u8g2.setFont(SMALL_FONT);
    if (settingOverride)
    {
      u8g2.drawStr(0, 13, OVERRIDE_TIME_SETTINGS_MSG);
    }
    else
    {
      u8g2.drawStr(0, 13, TIME_SETTINGS_MSG);
    }
    u8g2.setFont(BIG_FONT);
    u8g2.drawStr(0, 37, tempString);
    u8g2.drawStr(0, 60, &tempString[11]);
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
      u8g2.setFont(BIG_FONT);
      u8g2.drawStr(45, 40, "OFF");
    } while (u8g2.nextPage());
  }
}
#endif


// SETUP
void setup()
{

  // Setup mySerial
  mySerial.begin(SERIAL_SPEED);
  #if DEBUG > 0
    mySerial.println(F("Setup started."));
  #endif


  // Setup LCD
  #ifdef LCD
    lcd.begin(16, 2);
  #endif


  // Setup SH1106 OLED
  #ifdef SH1106
    u8g2.begin();
  #endif


  // Setup RTC
  // Print debug info
  #if DEBUG > 0
    mySerial.print(F("Using "));
    #ifdef DS3231
      mySerial.print (F("DS3231"));
    #else
      mySerial.print (F("DS1307"));
    #endif
    mySerial.println(F(" RTC"));
  #endif

  Rtc.Begin();
  set_zone(TIMEZONE);
  #if DST_RULES == EU
    set_dst(&eu_dst);
  #endif

  // Check if the RTC clock is running (Yes, it can be stopped!)
  if (!Rtc.GetIsRunning())
  {
    #if DEBUG > 0
      mySerial.println(F("WARNING: RTC wasn't running, starting it now."));
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
      tempFailed = true;
      #if DEBUG > 0
        mySerial.println(F("Couldn't find MCP9808!"));
      #endif
    }
    tempsensor.setResolution(MCP9808_TEMP_RESOLUTION);
    #if DEBUG > 0
      uint8_t temperatureResolution = tempsensor.getResolution();
      mySerial.print(F("MCP9808 temperature resolution: "));
      mySerial.println(temperatureResolution);
    #endif
  #endif


  // Setup rotary encoder
  encoder = new ClickEncoder(ENCODER_PINS);
  Timer1.initialize(ENCODER_TIMER);
  Timer1.attachInterrupt(timerIsr);


  // Configure handlers
  TemperatureHandler.float_value =                  &setpoint;
  TemperatureHandler.Min =                          TEMP_MIN;
  TemperatureHandler.Max =                          TEMP_MAX;
  TemperatureHandler.Increment =                    TEMP_INCREMENT;
  TemperatureHandler.EncoderRotatedFunction =       &UpdateFloatValue;
  TemperatureHandler.DisplayFunction =              &DisplayTemperature;
  TemperatureHandler.ButtonClickedFunction =        &SwitchToSetOverride;
  TemperatureHandler.ButtonDoubleClickedFunction =  &SwitchToSetTime;
  TemperatureHandler.ButtonHeldFunction =           &NullFunction;
  TemperatureHandler.ButtonReleasedFunction =       &ChangeStatus;

  SetYearHandler.tm_base =                          &tmSettings;
  SetYearHandler.uint16_value =                     &SetYearHandler.tm_base->tm_year;
  SetYearHandler.Min =                              117;
  SetYearHandler.Max =                              199;
  SetYearHandler.Increment =                        1;
  SetYearHandler.EncoderRotatedFunction =           &UpdateUint16Value;
  SetYearHandler.DisplayFunction =                  &DisplayTimeSetting;
  SetYearHandler.ButtonClickedFunction =            &SwitchToSetMonth;
  SetYearHandler.ButtonDoubleClickedFunction =      &SwitchToTemperature;
  SetYearHandler.ButtonHeldFunction =               &NullFunction;
  SetYearHandler.ButtonReleasedFunction =           &SwitchToTemperature;

  SetMonthHandler.tm_base =                         &tmSettings;
  SetMonthHandler.uint8_value =                     &SetYearHandler.tm_base->tm_mon;
  SetMonthHandler.Min =                             0;
  SetMonthHandler.Max =                             11;
  SetMonthHandler.Increment =                       1;
  SetMonthHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetMonthHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetMonthHandler.ButtonClickedFunction =           &SwitchToSetDay;
  SetMonthHandler.ButtonDoubleClickedFunction =     &SwitchToSetYear;
  SetMonthHandler.ButtonHeldFunction =              &NullFunction;
  SetMonthHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetDayHandler.tm_base =                           &tmSettings;
  SetDayHandler.uint8_value =                       &SetYearHandler.tm_base->tm_mday;
  SetDayHandler.Min =                               1;
  SetDayHandler.Max =                               31;
  SetDayHandler.Increment =                         1;
  SetDayHandler.EncoderRotatedFunction =            &UpdateUint8Value;
  SetDayHandler.DisplayFunction =                   &DisplayTimeSetting;
  SetDayHandler.ButtonClickedFunction =             &SwitchToSetHours;
  SetDayHandler.ButtonDoubleClickedFunction =       &SwitchToSetMonth;
  SetDayHandler.ButtonHeldFunction =                &NullFunction;
  SetDayHandler.ButtonReleasedFunction =            &SwitchToTemperature;

  SetHoursHandler.tm_base =                         &tmSettings;
  SetHoursHandler.uint8_value =                     &SetYearHandler.tm_base->tm_hour;
  SetHoursHandler.Min =                             0;
  SetHoursHandler.Max =                             23;
  SetHoursHandler.Increment =                       1;
  SetHoursHandler.EncoderRotatedFunction =          &UpdateUint8Value;
  SetHoursHandler.DisplayFunction =                 &DisplayTimeSetting;
  SetHoursHandler.ButtonClickedFunction =           &SwitchToSetMinutes;
  SetHoursHandler.ButtonDoubleClickedFunction =     &SwitchToSetDay;
  SetHoursHandler.ButtonHeldFunction =              &NullFunction;
  SetHoursHandler.ButtonReleasedFunction =          &SwitchToTemperature;

  SetMinutesHandler.tm_base =                       &tmSettings;
  SetMinutesHandler.uint8_value =                   &SetYearHandler.tm_base->tm_min;
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

  SetSecondsHandler.tm_base =                       &tmSettings;
  SetSecondsHandler.uint8_value =                   &SetYearHandler.tm_base->tm_sec;
  SetSecondsHandler.Min =                           0;
  SetSecondsHandler.Max =                           59;
  SetSecondsHandler.Increment =                     1;
  SetSecondsHandler.EncoderRotatedFunction =        &UpdateUint8Value;
  SetSecondsHandler.DisplayFunction =               &DisplayTimeSetting;
  SetSecondsHandler.ButtonClickedFunction =         &SetTime;
  SetSecondsHandler.ButtonDoubleClickedFunction =   &SwitchToSetMinutes;
  SetSecondsHandler.ButtonHeldFunction =            &NullFunction;
  SetSecondsHandler.ButtonReleasedFunction =        &SwitchToTemperature;

  SleepHandler.uint8_value =                        NULL;
  SleepHandler.Min =                                0;
  SleepHandler.Max =                                0;
  SleepHandler.Increment =                          0;
  SleepHandler.EncoderRotatedFunction =             &WakingUp;
  SleepHandler.DisplayFunction =                    &DisplayOFF;
  SleepHandler.ButtonClickedFunction =              &WakingUp;
  SleepHandler.ButtonDoubleClickedFunction =        &WakingUp;
  SleepHandler.ButtonHeldFunction =                 &NullFunction;
  SleepHandler.ButtonReleasedFunction =             &WakingUp;

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


  // Init global variables
  setpoint = TemperatureHandler.Min;
  temperature = TemperatureHandler.Min;
  relayTarget = false;
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  prevMillis = millis() - POLLING_TIME;


  // Init the weekly schedule (TEMPORARY FUNCTION, TO BE REMOVED)
  SetSchedule();
  

  // Get previous status from EEPROM
  EEPROM.get(EEPROMstatusAddress, status);
  if (status)
  {
     handler = &TemperatureHandler;
  }
  else
  {
     handler = &OffHandler;
  }


  // Get previous overrideTime from EEPROM
  EEPROM.get(EEPROMoverrideTimeAddress, overrideTime);


  // Init the time
  GetTime();
  prevActivationTime = now;
  lastTouched = now;


  #if DEBUG > 0
    strcpy(timestamp, isotime(&tmNow));
    mySerial.print(timestamp);
    mySerial.println(F(" Starting"));
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

      if (!clockFailed)
      {
        CheckSchedule();
      }

      SetRelay();

      CheckIdle();
    }
  }

  // Display status on LCD
  handler->DisplayFunction();

  // End of Loop
}
