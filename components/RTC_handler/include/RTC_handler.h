#ifndef RTC_HAND
#define RTC_HAND

#include <time.h>

void rtc_init_();
void set_rtc_time(int day, int week_day, int month, int year, int hour, int min, int sec);
void readClock(struct tm *time);


#endif