// European DST function
// Code by Edgar Bonet
// Found at: https://savannah.nongnu.org/bugs/?44327

/*
    Given the time stamp and time zone parameters provided, the Daylight Saving function must
    return a value appropriate for the tm structures' tm_isdst element. That is...
    
    0 : If Daylight Saving is not in effect.
    
    -1 : If it cannot be determined if Daylight Saving is in effect.
    
    A positive integer : Represents the number of seconds a clock is advanced for Daylight Saving.
    This will typically be ONE_HOUR.
    
    Daylight Saving 'rules' are subject to frequent change. For production applications it is
    recommended to write your own DST function, which uses 'rules' obtained from, and modifiable by,
    the end user ( perhaps stored in EEPROM ).
    
*/

int eu_dst(const time_t * timer, int32_t * z)
{
  uint32_t t = *timer;
  if ((uint8_t)(t >> 24) >= 194) t -= 3029443200U;
  t = (t + 655513200) / 604800 * 28;
  if ((uint16_t)(t % 1461) < 856) return 3600;
  else return 0;
}
