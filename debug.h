// Support macros for debugging

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
