#pragma once
#include <stdint.h>
#include <math.h>

// JPL SSD approximate Keplerian elements, Table 1 (1800–2050).
// https://ssd.jpl.nasa.gov/planets/approx_pos.html
// UTC approximates TDB here: adequate for this educational orbital diagram.
namespace watch_planets {
struct Position { double longitude, distance_au; };
struct Elements { double a,e,i,l,p,n, da,de,di,dl,dp,dn; };
static constexpr Elements elements[] = {
 { .38709927,.20563593,7.00497902,252.25032350,77.45779628,48.33076593,.00000037,.00001906,-.00594749,149472.67411175,.16047689,-.12534081 },
 { .72333566,.00677672,3.39467605,181.97909950,131.60246718,76.67984255,.00000390,-.00004107,-.00078890,58517.81538729,.00268329,-.27769418 },
 { 1.00000261,.01671123,-.00001531,100.46457166,102.93768193,0,.00000562,-.00004392,-.01294668,35999.37244981,.32327364,0 },
 { 1.52371034,.09339410,1.84969142,-4.55343205,-23.94362959,49.55953891,.00001847,.00007882,-.00813131,19140.30268499,.44441088,-.29257343 },
 { 5.202887,.04838624,1.30439695,34.39644051,14.72847983,100.47390909,-.00011607,-.00013253,-.00183714,3034.74612775,.21252668,.20469106 },
 { 9.53667594,.05386179,2.48599187,49.95424423,92.59887831,113.66242448,-.00125060,-.00050991,.00193609,1222.49362201,-.41897216,-.28867794 },
 { 19.18916464,.04725744,.77263783,313.23810451,170.95427630,74.01692503,-.00196176,-.00004397,-.00242939,428.48202785,.40805281,.04240589 },
 { 30.06992276,.00859048,1.77004347,-55.12002969,44.96476227,131.78422574,.00026291,.00005105,.00035372,218.45945325,-.32241464,-.00508664 }
};
inline bool valid_clock(int64_t utc) { return utc >= 946684800LL && utc < 2556144000LL; } // 2000–2050
inline Position position(unsigned planet, int64_t utc) {
    if (planet >= 8 || !valid_clock(utc)) return {NAN,NAN};
    const auto &v = elements[planet];
    constexpr double rad = 3.14159265358979323846/180;
    const double t = (double(utc)/86400+2440587.5-2451545)/36525;
    const double a=v.a+v.da*t, e=v.e+v.de*t, i=(v.i+v.di*t)*rad;
    const double node=(v.n+v.dn*t)*rad, peri=(v.p+v.dp*t)*rad;
    const double m=remainder((v.l+v.dl*t)*rad-peri,2*3.14159265358979323846);
    double eccentric=m;
    for (int j=0;j<10;++j) eccentric -= (eccentric-e*sin(eccentric)-m)/(1-e*cos(eccentric));
    const double x=a*(cos(eccentric)-e), y=a*sqrt(1-e*e)*sin(eccentric), w=peri-node;
    const double xx=(cos(w)*cos(node)-sin(w)*sin(node)*cos(i))*x+(-sin(w)*cos(node)-cos(w)*sin(node)*cos(i))*y;
    const double yy=(cos(w)*sin(node)+sin(w)*cos(node)*cos(i))*x+(-sin(w)*sin(node)+cos(w)*cos(node)*cos(i))*y;
    double longitude=atan2(yy,xx);
    if (longitude<0) longitude+=2*3.14159265358979323846;
    return {longitude,sqrt(x*x+y*y)};
}
}
