// Astronomy formulae adapted from app-pixels/sun-moon v1.0.0 (MIT).
// Copyright (c) 2026 app-pixels. See licenses/sun-moon-MIT.txt.
#include "watch_astronomy.h"
#include <math.h>

namespace watch_astronomy {
namespace {
constexpr double Pi = 3.14159265358979323846, Rad = Pi/180.0;
constexpr double Synodic = 29.530588853, NewMoon = 947182440.0;
bool leap(int y) { return y%4 == 0 && (y%100 != 0 || y%400 == 0); }
bool valid_date(Date d) {
    const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return d.year >= 2000 && d.year <= 2099 && d.month >= 1 && d.month <= 12 &&
           d.day >= 1 && d.day <= lengths[d.month-1]+(d.month == 2 && leap(d.year));
}
void solar_params(Date d, double hour, double &equation, double &declination) {
    const int n = int(day_number(d)-day_number({d.year,1,1}))+1;
    const double g = 2*Pi/(leap(d.year) ? 366 : 365)*(n-1+(hour-12)/24);
    equation = 229.18*(0.000075+0.001868*cos(g)-0.032077*sin(g)-0.014615*cos(2*g)-0.040849*sin(2*g));
    declination = 0.006918-0.399912*cos(g)+0.070257*sin(g)-0.006758*cos(2*g)+
                  0.000907*sin(2*g)-0.002697*cos(3*g)+0.00148*sin(3*g);
}
int last_sunday(int year, int month) {
    // March and October both have 31 days; Unix day zero was Thursday.
    return 31-int((day_number({year,month,31})+4)%7);
}
double clamp(double value) { return value < -1 ? -1 : value > 1 ? 1 : value; }
}

int64_t day_number(Date d) {
    int y = d.year-(d.month <= 2);
    const int era = (y >= 0 ? y : y-399)/400;
    const unsigned yoe = unsigned(y-era*400);
    const unsigned doy = (153*(d.month+(d.month > 2 ? -3 : 9))+2)/5+d.day-1;
    return int64_t(era)*146097+yoe*365+yoe/4-yoe/100+doy-719468;
}
Date date_from_day(int64_t day) {
    day += 719468;
    const int64_t era = (day >= 0 ? day : day-146096)/146097;
    const unsigned doe = unsigned(day-era*146097);
    const unsigned yoe = (doe-doe/1460+doe/36524-doe/146096)/365;
    int year = int(yoe+era*400);
    const unsigned doy = doe-(365*yoe+yoe/4-yoe/100);
    const unsigned mp = (5*doy+2)/153;
    const int dom = int(doy-(153*mp+2)/5+1), month = int(mp)+(mp < 10 ? 3 : -9);
    year += month <= 2;
    return {year,month,dom};
}
Date add_days(Date d, int days) { return date_from_day(day_number(d)+days); }
bool valid_clock(int64_t utc) { return utc >= 946684800LL && utc < 4102444800LL; }
int madrid_offset(int64_t utc) {
    const int year = date_from_day(utc/86400).year;
    const int64_t start = day_number({year,3,last_sunday(year,3)})*86400+3600;
    const int64_t end = day_number({year,10,last_sunday(year,10)})*86400+3600;
    return utc >= start && utc < end ? 7200 : 3600;
}
Date madrid_date(int64_t utc) { return date_from_day((utc+madrid_offset(utc))/86400); }
int madrid_minutes(int64_t utc) { return int(((utc+madrid_offset(utc))%86400)/60); }

Sun sun(Date date, int64_t now, double latitude, double longitude) {
    Sun result;
    if (!valid_date(date) || !isfinite(latitude) || !isfinite(longitude) ||
        latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) return result;
    const double lat = latitude*Rad;
    double eq, decl;
    solar_params(date,12,eq,decl);
    const int64_t midnight = day_number(date)*86400;
    result.noon = midnight+int64_t(llround((720-4*longitude-eq)*60));
    // Avoid tangent singularities at the poles.
    const double denominator = cos(lat)*cos(decl);
    const double numerator = cos(90.833*Rad)-sin(lat)*sin(decl);
    const double cos_hour = fabs(denominator) < 1e-12 ? (numerator > 0 ? 2 : -2) : numerator/denominator;
    result.valid = true;
    result.polar_day = cos_hour < -1;
    result.polar_night = cos_hour > 1;
    if (!result.polar_day && !result.polar_night) {
        // Refine declination/equation at each event instead of reusing noon.
        const double hour_angle = acos(clamp(cos_hour))/Rad;
        double minutes[] = {720-4*(longitude+hour_angle)-eq,720-4*(longitude-hour_angle)-eq};
        for (int event = 0; event < 2; ++event) {
            for (int iteration = 0; iteration < 2; ++iteration) {
                solar_params(date,minutes[event]/60,eq,decl);
                const double c = (cos(90.833*Rad)-sin(lat)*sin(decl))/(cos(lat)*cos(decl));
                const double ha = acos(clamp(c))/Rad;
                minutes[event] = 720-4*(longitude+(event ? -ha : ha))-eq;
            }
        }
        result.rise = midnight+int64_t(llround(minutes[0]*60));
        result.set = midnight+int64_t(llround(minutes[1]*60));
    }
    const auto utc_date = date_from_day(now/86400);
    solar_params(utc_date,double(now%86400)/3600,eq,decl);
    const double ha = (double(now%86400)/60+eq+4*longitude)/4-180;
    result.elevation = 90-acos(clamp(sin(lat)*sin(decl)+cos(lat)*cos(decl)*cos(ha*Rad)))/Rad;
    return result;
}

Moon moon(int64_t utc) {
    Moon result;
    double age = fmod((double(utc)-NewMoon)/86400,Synodic);
    if (age < 0) age += Synodic;
    result.age_days = age; result.phase = age/Synodic;
    result.illumination = 50*(1-cos(2*Pi*result.phase));
    double until_full = 0.5-result.phase;
    if (until_full < 0) until_full += 1;
    result.next_full = utc+int64_t(llround(until_full*Synodic*86400));
    result.next_new = utc+int64_t(llround((1-result.phase)*Synodic*86400));
    const double p = result.phase;
    result.name = p < .03 || p > .97 ? "New Moon" : p < .22 ? "Waxing Crescent" :
        p < .28 ? "First Quarter" : p < .47 ? "Waxing Gibbous" : p < .53 ? "Full Moon" :
        p < .72 ? "Waning Gibbous" : p < .78 ? "Last Quarter" : "Waning Crescent";
    return result;
}
}
