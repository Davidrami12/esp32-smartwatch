#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

// Wire/display limits; event text is UTF-8, never LVGL recolor markup.
enum { WATCH_CALENDAR_MAX_EVENTS=32 };
typedef enum { CalendarReady, CalendarOff, CalendarPermission, CalendarNoSelection, CalendarError, CalendarGoogleAuth } watch_calendar_status_t;
typedef struct {
    uint8_t days; // Monday bit 0 ... Sunday bit 6, including multi-day overlap.
    bool all_day;
    uint32_t color; // RGB24 from Android's effective display colour.
    char title[81], location[81], time[41];
} watch_calendar_event_t;
typedef struct {
    uint32_t monday, fetched_at;
    watch_calendar_status_t status;
    uint8_t count;
    bool truncated;
    uint8_t holiday_days; // Imported holiday-calendar occurrences, Monday bit 0.
    watch_calendar_event_t events[WATCH_CALENDAR_MAX_EVENTS];
} watch_calendar_week_t;

#ifdef __cplusplus

// Gregorian arithmetic without the host timezone or libc DST normalization.
inline int64_t watch_calendar_day(uint32_t date) {
    int y=int(date/10000),m=int(date/100%100),d=int(date%100);
    y-=m<=2; const int era=(y>=0 ? y : y-399)/400;
    const unsigned yo=unsigned(y-era*400), mp=unsigned(m+(m>2 ? -3 : 9));
    return int64_t(era)*146097+yo*365+yo/4-yo/100+(153*mp+2)/5+unsigned(d)-1-719468;
}
inline uint32_t watch_calendar_date(int64_t day) {
    day+=719468; const int64_t era=(day>=0 ? day : day-146096)/146097;
    const unsigned doe=unsigned(day-era*146097),yo=(doe-doe/1460+doe/36524-doe/146096)/365;
    int y=int(yo)+int(era)*400;
    const unsigned doy=doe-(365*yo+yo/4-yo/100),mp=(5*doy+2)/153;
    const unsigned d=doy-(153*mp+2)/5+1; const int m=int(mp)+(mp<10 ? 3 : -9);
    y+=m<=2; return uint32_t(y*10000+m*100+int(d));
}
inline bool watch_calendar_valid_date(uint32_t date) {
    return date>=20200101 && date<=20991231 && watch_calendar_date(watch_calendar_day(date))==date;
}
inline uint32_t watch_calendar_monday(uint32_t date) {
    const int64_t day=watch_calendar_day(date);
    return watch_calendar_date(day-(day+3)%7);
}
inline uint32_t watch_calendar_crc(const uint8_t *p,size_t n) {
    uint32_t crc=~0u;
    for (size_t i=0;i<n;++i) { crc^=p[i]; for (unsigned j=0;j<8;++j) crc=(crc>>1)^((crc&1) ? 0xEDB88320u : 0); }
    return ~crc;
}
inline uint32_t watch_calendar_le(const uint8_t *p,unsigned n=4) {
    uint32_t v=0; for (unsigned i=0;i<n;++i) v|=uint32_t(p[i])<<(8*i); return v;
}

// One bounded, atomic week transaction. No partially received events are visible.
class WatchCalendarReceiver {
public:
    static constexpr size_t Capacity=7000;
    uint32_t request=0,monday=0;
    size_t used=0,total=0;
    uint8_t bytes[Capacity]={};
    void select(uint32_t id,uint32_t date) { request=id; monday=date; reset(); }
    void reset() { memset(bytes,0,sizeof(bytes)); used=total=0; }
    bool accept(const uint8_t *p,size_t n,watch_calendar_week_t &out) {
        if (n<=10 || n>244 || p[0]!=1 || p[1]!=1 || !request || watch_calendar_le(p+2)!=request) return false;
        const size_t offset=watch_calendar_le(p+6,2),size=watch_calendar_le(p+8,2),length=n-10;
        if (size<12 || size>Capacity || offset+length>size) return false;
        if (!offset) { reset(); total=size; }
        if (offset!=used || size!=total) return false;
        memcpy(bytes+used,p+10,length); used+=length;
        if (used!=total) return true;
        const bool ok=decode(out); reset(); return ok;
    }
    bool complete(const uint8_t *p,size_t n) const { return n>10 && watch_calendar_le(p+6,2)+n-10==watch_calendar_le(p+8,2); }
private:
    static bool text(char *dst,size_t limit,const uint8_t *p,size_t n) {
        if (n>=limit) return false;
        // Reject malformed UTF-8, controls, overlong sequences and surrogates.
        for (size_t i=0;i<n;) {
            const uint8_t c=p[i++]; uint32_t cp=c; unsigned more=0;
            if (c<0x20 || c==0x7f) return false;
            if (c>=0xc2 && c<=0xdf) { cp=c&31; more=1; }
            else if (c>=0xe0 && c<=0xef) { cp=c&15; more=2; }
            else if (c>=0xf0 && c<=0xf4) { cp=c&7; more=3; }
            else if (c>=0x80) return false;
            if (i+more>n) return false;
            for (unsigned k=0;k<more;++k) { const uint8_t b=p[i++]; if ((b&0xc0)!=0x80) return false; cp=(cp<<6)|(b&63); }
            if ((more==1 && cp<0x80)||(more==2 && cp<0x800)||(more==3 && cp<0x10000)||cp>0x10ffff||(cp>=0xd800 && cp<=0xdfff)) return false;
        }
        memcpy(dst,p,n); dst[n]=0; return true;
    }
    bool decode(watch_calendar_week_t &out) {
        if (watch_calendar_le(bytes+total-4)!=watch_calendar_crc(bytes,total-4) || bytes[0]>CalendarGoogleAuth ||
            bytes[1]>WATCH_CALENDAR_MAX_EVENTS || bytes[2]>1 || bytes[3]>2 || (bytes[0] && bytes[1])) return false;
        memset(&out,0,sizeof(out)); out.monday=monday; out.status=static_cast<watch_calendar_status_t>(bytes[0]);
        out.count=bytes[1]; out.truncated=bytes[2]; out.fetched_at=watch_calendar_le(bytes+4);
        size_t pos=8;
        if (bytes[3]==2) {
            if (pos>=total-4 || bytes[pos]>127 || (bytes[0] && bytes[pos])) return false;
            out.holiday_days=bytes[pos++];
        }
        for (unsigned i=0;i<out.count;++i) {
            if (pos+5>total-4) return false;
            auto &e=out.events[i]; e.days=bytes[pos++]; const uint8_t flags=bytes[pos++];
            const size_t title=bytes[pos++],location=bytes[pos++],time=bytes[pos++];
            if (!e.days || e.days>127 || flags>1 || !title || !time || pos+title+location+time>total-4) return false;
            e.all_day=flags;
            e.color=0x78B8FF;
            if (bytes[3]>=1) {
                if (pos+3+title+location+time>total-4) return false;
                e.color=watch_calendar_le(bytes+pos,3); pos+=3;
            }
            if (!text(e.title,sizeof(e.title),bytes+pos,title)) return false;
            pos+=title;
            if (!text(e.location,sizeof(e.location),bytes+pos,location)) return false;
            pos+=location;
            if (!text(e.time,sizeof(e.time),bytes+pos,time)) return false;
            pos+=time;
        }
        return pos==total-4;
    }
};
#endif
