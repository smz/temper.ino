#include "temperino.h"

// Listen to commands on the serial connection and take action.
void serialEvent()
{
  char inChar;
  uint8_t oldPtr;

  while (mySerial.available())
  {
    char inChar = (char) mySerial.read();
    if (inChar == '\n')
    {
      serbuf[serbufPtr][serbufIdx] = '\0';
      oldPtr = serbufPtr;
      serbufPtr = (serbufPtr + 1) % 2;
      serbufIdx = 0;
      ParseCommand(serbuf[oldPtr]);
    }
    else
    {
      if (serbufIdx < SERBUF_SIZE - 1 && inChar != '\r')
      {
        serbuf[serbufPtr][serbufIdx++] = inChar;
      }
    }
  }
}


// Parse command recevied on the serial connection
void ParseCommand(char *cmdString)
{
  int addr;
  int cmd;
  int stepIdx;
  programStep step;
  uint8_t len;
  static char delimiters[] = " ,";
  char *token;
  bool ok;
  float tempTemp;
  time_t tempTime;

  strcpy(tempString, cmdString);
  addr = atoi(strtok(cmdString, delimiters));

  if (addr == config.myAddress)
  {
    cmd = atoi(strtok(NULL, delimiters));

    ok = true;
    switch (cmd)
    {
      case CMD_ON_OFF: // Turn ON/OFF (or get status)
        if ((token = strtok(NULL, delimiters)) != NULL)
        {
          if ((bool) atoi(token) != prevStatus.on)
          {
            ChangeStatus();
          }
        }
        PrintAnswer(cmd, status.on);
        break;
      case CMD_GET_TEMPERATURE: // Get temperature
        PrintAnswer(cmd, temperature);
        break;
      case CMD_GET_SET_SETPOINT: // Get-Set setpoint
        if ((token = strtok(NULL, delimiters)) != NULL)
        {
          tempTemp = atof(token);
          if (tempTemp >= config.tempMin && tempTemp <= config.tempMax)
          {
            status.setpoint = tempTemp;
            if (status.overrideTime <= now)
            {
              SetOverride(NextStepTime());
            }
          }
          else
          {
            ok = false;
          }
        }
        if (ok)
        {
          PrintAnswer(cmd, status.setpoint);
        }
        break;
      case CMD_GET_SET_OVERRIDE: // Get-Set override time
        if ((token = strtok(NULL, delimiters)) != NULL)
        {
          tempTime = atot(token);
          if (tempTime >= 0)
          {
            if (tempTime < UNIX_OFFSET)
            {
              tempTime = UNIX_OFFSET;
            }
            SetOverride(tempTime - UNIX_OFFSET);
          }
          else
          {
            ok = false;
          }
        }
        if (ok)
        {
          PrintAnswer(cmd, status.overrideTime + UNIX_OFFSET);
        }
        break;
      case CMD_GET_SET_STEPS: // Get-Set schedule step(s)
        if ((token = strtok(NULL, delimiters)) == NULL)
        {
          for (stepIdx = 0; stepIdx < MAX_WEEKLY_STEPS; stepIdx++)
          {
            PrintStep(stepIdx);
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
              PrintStep(stepIdx);
            }
            else
            {
              step.tow = atoi(token);
              if (step.tow < 0 || step.tow > 62400)
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
                  if (step.temperature < config.tempMin || step.temperature > config.tempMax)
                  {
                    ok = false;
                  }
                  else
                  {
                    PutStepToEEPROM(stepIdx, step);
                    PrintStep(stepIdx);
                  }
                }
              }
            }
          }
        }
        break;
      case CMD_GET_SET_TIME: // Get-Set system time
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
          PrintAnswer(cmd, now + UNIX_OFFSET);
        }
        break;
      default:
        ok = false;
    }

    if (!ok)
    {
      mySerial.print(config.myAddress);
      mySerial.print(",0,\"");
      mySerial.print(tempString);
      mySerial.println('"');
    }

  }
}


// Convert a string to a time_t value
// Similar to atoi() and atof()...
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


// Print an answer to the serial connection
void PrintAnswer(int cmd, int answer)
{
  StartAnswer(cmd);
  mySerial.println(answer);
}
void PrintAnswer(int cmd, float answer)
{
  StartAnswer(cmd);
  mySerial.println(answer);
}
void PrintAnswer(int cmd, time_t answer)
{
  StartAnswer(cmd);
  mySerial.println(answer);
}
void StartAnswer(int cmd)
{
  mySerial.print(config.myAddress);
  mySerial.print(',');
  mySerial.print(cmd);
  mySerial.print(',');
}


// Read a schedule step and print it to the serial connection as an answer
void PrintStep(int stepIdx)
{
  programStep step;

  EEPROM.get(EEPROMstepAddress(stepIdx), step);
  StartAnswer(CMD_GET_SET_STEPS);
  mySerial.print(stepIdx);
  mySerial.print(',');
  mySerial.print(step.tow);
  mySerial.print(',');
  mySerial.println(step.temperature);
}


// Does nothing, but I need it
void NullFunction()
{
}


// Write current status to EEPROM if changed
void StoreStatus()
{
  if (status.on != prevStatus.on ||
      status.setpoint != prevStatus.setpoint ||
      status.overrideTime != prevStatus.overrideTime)
  {
    EEPROM.put(STATUS_ADDR, status);
    EEPROM.get(STATUS_ADDR, prevStatus);
  }
}


// Toggle the system status (ON/OFF)
void ChangeStatus()
{
  if (status.on)
  {
    status.on = false;
    digitalWrite(RELAY_PIN, LOW);
    handler = &OffHandler;
  }
  else
  {
    status.on = true;
    handler = &TemperatureHandler;
  }
}


// Set the global overrideTime and store it to EEPROM
void SetOverride(time_t time)
{
  status.overrideTime = time;
}


// 1st stage of waking up: the display is enabled,
// but the rotary encoder is still associated to the SleepHandler
// in order to "consume" extra clicks
void WakingUp()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Waking Up."));
  #endif
  SleepHandler.DisplayFunction = &DisplayTemperature;
}


// Check if the display should be switched off because of lack of human interaction
// Also 2nd stage of waking up (rotary encoder associated to temperature setting)
void CheckIdle()
{
  if (handler == &TemperatureHandler && now > lastTouched + config.sleepAfter)
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


// Switch rotary encoder and display handling to the "Set the time" function
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


// Switch rotary encoder and display handling to the "Set the override time" function
void SwitchToSetOverride()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Switching to Override"));
  #endif
  if (status.overrideTime < now)
  {
    SetOverride(now);
  }
  localtime_r(&status.overrideTime, &tmOverride);
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


// Sub-handler for setting the year
void SwitchToSetYear()
{
  SetYearHandler.uint16_value = &SetYearHandler.tm_base->tm_year;
  handler = &SetYearHandler;
}


// Sub-handler for setting the month
void SwitchToSetMonth()
{
  SetMonthHandler.uint8_value = &SetMonthHandler.tm_base->tm_mon;
  handler = &SetMonthHandler;
}


// Sub-handler for setting the day of the month
void SwitchToSetDay()
{
  SetDayHandler.uint8_value = &SetDayHandler.tm_base->tm_mday;
  handler = &SetDayHandler;
}


// Sub-handler for setting the hour
void SwitchToSetHours()
{
  SetHoursHandler.uint8_value = &SetHoursHandler.tm_base->tm_hour;
  handler = &SetHoursHandler;
}


// Sub-handler for setting the minute
void SwitchToSetMinutes()
{
  SetMinutesHandler.uint8_value = &SetMinutesHandler.tm_base->tm_min;
  handler = &SetMinutesHandler;
}


// Sub-handler for setting the second
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


// Set the system or override time and swich back to temperature setting
void SetTime()
{
  if (settingOverride)
  {
    SetOverride(mktime(&tmOverride));
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


// Set the handler to temperature setting
void SwitchToTemperature()
{
  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.println(F(" Switching to Temperature"));
  #endif
  handler = &TemperatureHandler;
}


// Generic function used by the rotary encoder handlers to set a float value
void UpdateFloatValue (int16_t value)
{
  float temp = *handler->float_value + value * handler->Increment;

  if (temp > handler->Max)
  {
    temp = handler->Max;
  }

  if (temp < handler->Min)
  {
    temp = handler->Min;
  }

  if (handler == &TemperatureHandler && temp != *handler->float_value && status.overrideTime < now)
  {
    SetOverride(NextStepTime());
  }

  *handler->float_value = temp;

  #if DEBUG > 1
    mySerial.print(timestamp);
    mySerial.print(F(" Value (float): "));
    mySerial.println(*handler->float_value);
  #endif
}


// Generic function used by the rotary encoder handlers to set a uint8_8 value
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


// Generic function used by the rotary encoder handlers to set a uint16_t value
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


// Triggered by the rotary encoder, dispatch to the current "handler"
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


// Get the time from the RTC
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
    status.setpoint = config.tempMin;
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
    temperature = config.tempMax;
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
  if ((abs(status.setpoint - temperature) > config.tempHysteresis))
  {
    relayTarget = status.setpoint > temperature;

    if (relayStatus != relayTarget)
    {
      if (now - prevActivationTime > config.relayQuiescentTime)
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
        mySerial.print(status.setpoint);
        mySerial.print(F(" Temp: "));
        mySerial.print(temperature);
        mySerial.println(relayTarget ? F(" Turn on!") : F(" Turn off!"));
      }
      #endif
    }
  }
}


// Find the "current" schedule step and apply its temperature setting
void CheckSchedule()
{
  int stepIdx = 0;
  float newtemp = status.setpoint;
  programStep step;

  if (status.overrideTime > now) return;

  while (stepIdx < MAX_WEEKLY_STEPS)
  {
    EEPROM.get(EEPROMstepAddress(stepIdx), step);
    if (step.tow > nowTOW) break;
    newtemp = step.temperature;
    stepIdx++;
  }

  status.setpoint = newtemp;

  #if DEBUG > 2
    if (status.setpoint != newtemp)
    {
      mySerial.print(timestamp);
      mySerial.print(F(" New setpoint: "));
      mySerial.println(status.setpoint);
    }
  #endif
}


// Find the "next" schedule step time
time_t NextStepTime()
{
  int stepIdx = 0;
  programStep step;
  struct tm tmNext;
  time_t nextTime;
  int nextDay;

  nextTime = now;
  localtime_r(&now, &tmNext);

  while (stepIdx < MAX_WEEKLY_STEPS)
  {
    EEPROM.get(EEPROMstepAddress(stepIdx), step);
    if (step.tow < 62400 && step.tow > nowTOW) break;
    stepIdx++;
  }

  if (stepIdx == MAX_WEEKLY_STEPS)
  {
    EEPROM.get(EEPROMstepAddress(0), step);
  }
  
  if (step.tow < 62400)
  {

    nextDay = step.tow / 10000;

    tmNext.tm_hour = (step.tow - nextDay * 10000) / 100;
    tmNext.tm_min = (step.tow - nextDay * 10000) - tmNext.tm_hour * 100;

    tmNext.tm_sec = 0;
    nextTime = mktime(&tmNext);
    
    nextDay = nextDay - tmNow.tm_wday;
    if (nextDay < 0)
    {
      nextDay += 7;
    }

    nextTime += nextDay * 86400;
  }

  return nextTime;
}


// Write a schedule's step to EEPROM
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
  dtostrf(temperature, -5, 1, tempString);
  sprintf(lcdLine1, "A%5s  %2.2i:%2.2i:%2.2i",
    tempString,
    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  dtostrf(status.setpoint, -5, 1, tempString);
  sprintf(lcdLine2, "S%5s  %3s  %3s",
    tempString,
    (status.overrideTime > now ? "OVR" : ""),
    (relayStatus == relayTarget ? (relayTarget ? " ON" : "OFF") : (relayTarget ? " on" : "off")));
  RefreshLCD();
}


// Display page for time settings (system or override time)
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


// Display the OFF message when the system is in the "OFF" state
void DisplayOffStatus()
{
  if (now > lastTouched + config.sleepAfter)
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
      dtostrf(temperature, -5, 1, tempString);
      u8g2.drawStr(32, 14, tempString);
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
    dtostrf(status.setpoint, -5, 1, tempString);
    u8g2.drawStr(32, 41, tempString);

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
    if (status.overrideTime > now)
    {
      u8g2.drawStr(108, 41, "OVR");
    }
  } while (u8g2.nextPage());
}


// Display page for time settings (system or override time)
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


// Display the OFF message when the system is in the "OFF" state
void DisplayOffStatus()
{
  if (now > lastTouched + config.sleepAfter)
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


// Setup function
void setup()
{

  // Get configuration from EEPROM
  EEPROM.get(CONFIG_ADDR, config);

  // Get the last stored status
  EEPROM.get(STATUS_ADDR, status);
  EEPROM.get(STATUS_ADDR, prevStatus);

  // Setup serial communication
  mySerial.begin(config.serialSpeed);
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
  #if DEBUG > 0
    mySerial.print(F("Using "));
    #ifdef DS3231
      mySerial.print (F("DS3231"));
    #else
      mySerial.print (F("DS1307"));
    #endif
    mySerial.println(F(" RTC"));
  #endif

  // Instantiate the RTC object
  Rtc.Begin();

  // Set timezone values
  set_zone(config.timeZone);
  if (config.dstRule == 1)
  {
    set_dst(&eu_dst);
  }

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
  TemperatureHandler.float_value =                  &status.setpoint;
  TemperatureHandler.Min =                          config.tempMin;
  TemperatureHandler.Max =                          config.tempMax;
  TemperatureHandler.Increment =                    config.tempIncrement;
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
  temperature = config.tempMin;
  relayTarget = false;
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  prevMillis = millis() - POLLING_TIME;


  // Set the handler according to the current status
  if (status.on)
  {
     handler = &TemperatureHandler;
  }
  else
  {
     handler = &OffHandler;
  }


  // Init the time
  GetTime();
  prevActivationTime = now;
  lastTouched = now;


  #if DEBUG > 0
    strcpy(timestamp, isotime(&tmNow));
    mySerial.print(timestamp);
    mySerial.println(F(" Starting"));
  #endif
}


// Main loop
void loop()
{
  #if DEBUG > 8
    loops++;
  #endif

  relayStatus = (bool) digitalRead(RELAY_PIN);

  EncoderDispatcher();

  if (millis() - prevMillis >= POLLING_TIME)
  {
    prevMillis += POLLING_TIME;

    #if DEBUG > 8
      PrintLoops();
    #endif

    GetTime();

    if (status.on)
    {
      GetTemperature();

      if (!clockFailed)
      {
        CheckSchedule();
      }

      SetRelay();

      CheckIdle();

    }

    // Store status on EEPROM (if changed)
    StoreStatus();
  }

  // Display status on LCD
  handler->DisplayFunction();

}
