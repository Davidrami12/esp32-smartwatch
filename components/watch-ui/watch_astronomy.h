#pragma once
#include <stdint.h>

// Offline astronomy. UTC instants remain UTC; civil dates/times use Madrid's
// current EU DST rules, independently of the simulator host's timezone.
namespace watch_astronomy {
constexpr double MadridLatitude = 40.4168, MadridLongitude = -3.7038;
struct Date { int year, month, day; };
struct Sun {
    bool valid = false, polar_day = false, polar_night = false;
    int64_t rise = 0, set = 0, noon = 0;
    double elevation = 0;
};
struct Moon {
    double phase = 0, illumination = 0, age_days = 0;
    int64_t next_full = 0, next_new = 0;
    const char *name = "";
};
bool valid_clock(int64_t utc);
int64_t day_number(Date date);
Date date_from_day(int64_t day);
Date add_days(Date date, int days);
int madrid_offset(int64_t utc);
Date madrid_date(int64_t utc);
int madrid_minutes(int64_t utc);
Sun sun(Date date, int64_t now, double latitude = MadridLatitude, double longitude = MadridLongitude);
Moon moon(int64_t utc);
}
