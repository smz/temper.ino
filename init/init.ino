#include <time.h>
#include <EEPROM.h>

// Default configuration values
#define DEFAULT_SERIAL_SPEED 9600
#define DEFAULT_ADDRESS 1
#define DEFAULT_SETPOINT 5.0
#define DEFAULT_TEMP_MIN 5.0
#define DEFAULT_TEMP_MAX 35.0
#define DEFAULT_TEMP_INCREMENT 0.5
#define DEFAULT_TEMP_HYSTERESIS 0.5
#define DEFAULT_SLEEP_AFTER 60
#define DEFAULT_RELAY_QUIESCENT_TIME 15
#define DEFAULT_OVERRIDE_TIME 3600
#define DEFAULT_TIMEZONE (1 * ONE_HOUR)
#define DEFAULT_DST_RULE 1

// Constant parameters
#define MAX_WEEKLY_STEPS 70

// Configuration object
typedef struct
{
  uint8_t myAddress;
  uint16_t serialSpeed;
  float tempMin;
  float tempMax;
  float tempIncrement;
  float tempHysteresis;
  time_t sleepAfter;
  time_t relayQuiescentTime;
  time_t overrideTimeDefault;
  time_t timeZone;
  time_t dstRule;
} configuration_t;
configuration_t config;


// Status object
typedef struct
{
  bool on;
  float setpoint;
  time_t overrideTime;
} status_t;
status_t status;


// Schedule table
typedef struct {uint16_t tow; float temperature;} programStep;
programStep defaultStep;


// EEPROM storage addresses
#define CONFIG_ADDR (0)
#define STATUS_ADDR (CONFIG_ADDR + sizeof(configuration_t))
#define STEPS_ADDR (STATUS_ADDR + sizeof(status_t)) 
#define EEPROMstepAddress(x) (STEPS_ADDR + x * sizeof(programStep))


// Setup function
void setup()
{

  Serial.begin(DEFAULT_SERIAL_SPEED);
  
  config.myAddress = DEFAULT_ADDRESS;
  config.serialSpeed = DEFAULT_SERIAL_SPEED;
  config.tempMin = DEFAULT_TEMP_MIN;
  config.tempMax = DEFAULT_TEMP_MAX;
  config.tempIncrement = DEFAULT_TEMP_INCREMENT;
  config.tempHysteresis = DEFAULT_TEMP_HYSTERESIS;
  config.sleepAfter = DEFAULT_SLEEP_AFTER;
  config.relayQuiescentTime = DEFAULT_RELAY_QUIESCENT_TIME;
  config.overrideTimeDefault = DEFAULT_OVERRIDE_TIME;
  config.timeZone = DEFAULT_TIMEZONE;
  config.dstRule = DEFAULT_DST_RULE;
  EEPROM.put(CONFIG_ADDR, config);
  
  status.on = true;
  status.setpoint = DEFAULT_TEMP_MIN;
  status.overrideTime = 0;
  EEPROM.put(STATUS_ADDR, status);

  defaultStep.tow = 62400;
  defaultStep.temperature = DEFAULT_TEMP_MIN;
  
  for (int i = 0; i < MAX_WEEKLY_STEPS; i++)
  {
    EEPROM.put(EEPROMstepAddress(i), defaultStep);
  }

  Serial.println("Done");
}


// Main loop
void loop()
{
}
