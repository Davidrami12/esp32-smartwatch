#include "watch_astronomy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
using namespace watch_astronomy;

static int64_t utc(int y, int m, int d, int h = 0, int minute = 0) {
    return day_number({y,m,d})*86400+h*3600+minute*60;
}
int main() {
    for (int y = 2000; y <= 2099; ++y) {
        for (int m = 1; m <= 12; ++m) {
            Date date{y,m,1}; const auto roundtrip = date_from_day(day_number(date));
            assert(roundtrip.year == y && roundtrip.month == m && roundtrip.day == 1);
        }
    }
    const auto leap = add_days({2024,3,1},-1); assert(leap.month == 2 && leap.day == 29);
    const auto year = add_days({2026,12,31},1); assert(year.year == 2027 && year.month == 1 && year.day == 1);
    assert(!valid_clock(0) && valid_clock(utc(2026,1,1)) && !valid_clock(utc(2100,1,1)));
    assert(madrid_offset(utc(2026,3,29,0,59)) == 3600);
    assert(madrid_offset(utc(2026,3,29,1)) == 7200);
    assert(madrid_minutes(utc(2026,3,29,0,59)) == 119);
    assert(madrid_minutes(utc(2026,3,29,1)) == 180);
    assert(madrid_offset(utc(2026,10,25,0,59)) == 7200);
    assert(madrid_offset(utc(2026,10,25,1)) == 3600);
    assert(madrid_date(utc(2026,6,20,23)).day == 21);
    const auto summer = sun({2026,6,21},utc(2026,6,21,12));
    const auto winter = sun({2026,12,21},utc(2026,12,21,12));
    // Broad seasonal sanity checks, not a claim of ±2-minute accuracy.
    assert(summer.valid && !summer.polar_day && !summer.polar_night);
    assert(abs(madrid_minutes(summer.rise)-(6*60+44)) < 12);
    assert(abs(madrid_minutes(summer.set)-(21*60+49)) < 12);
    assert(abs(madrid_minutes(winter.rise)-(8*60+34)) < 12);
    assert(abs(madrid_minutes(winter.set)-(17*60+51)) < 12);
    assert(summer.set-summer.rise > 15*3600 && winter.set-winter.rise < 10*3600);
    assert(summer.rise < summer.noon && summer.noon < summer.set);
    assert(summer.elevation > winter.elevation);
    for (auto date : {Date{2026,3,29},Date{2026,10,25}}) {
        auto day = sun(date,utc(date.year,date.month,date.day,12));
        auto local = madrid_date(day.rise);
        assert(local.day == date.day && local.month == date.month);
    }
    assert(sun({2026,6,21},utc(2026,6,21),69.65,18.96).polar_day);
    assert(sun({2026,12,21},utc(2026,12,21),69.65,18.96).polar_night);
    assert(sun({2026,6,21},utc(2026,6,21),90,0).polar_day);
    assert(sun({2026,12,21},utc(2026,12,21),-90,0).polar_day);
    assert(!sun({2026,2,30},utc(2026,1,1)).valid);
    assert(!sun({2026,1,1},utc(2026,1,1),91,0).valid);
    assert(!sun({2026,1,1},utc(2026,1,1),0,NAN).valid);
    const auto anchor = moon(947182440); assert(anchor.phase == 0 && anchor.illumination == 0);
    const auto before_anchor = moon(utc(2000,1,1)); assert(before_anchor.phase >= 0 && before_anchor.phase < 1);
    const auto full = moon(utc(2024,3,25,7)); assert(full.illumination > 98);
    const auto eclipse = moon(utc(2024,4,8,18,21)); assert(eclipse.illumination < 2);
    for (int day = 0; day < 366; ++day) {
        const int64_t now = utc(2026,1,1)+day*86400;
        auto phase = moon(now);
        assert(phase.phase >= 0 && phase.phase < 1 && phase.illumination >= 0 && phase.illumination <= 100);
        assert(phase.next_full >= now && phase.next_full-now <= 30*86400);
        assert(phase.next_new > now && phase.next_new-now <= 30*86400);
    }
    printf("PASS: astronomy dates, Madrid DST, seasonal sun, polar cases, bounded moon estimates\n");
}
