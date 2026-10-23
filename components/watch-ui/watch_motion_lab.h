#pragma once
#include <cmath>
#include <cstdint>

// Accelerometer-only tilt: gravity gives pitch/roll, never absolute heading.
struct WatchMotionLab {
    float x=0,y=0,z=1,roll=0,pitch=0,zero_roll=0,zero_pitch=0;
    float history[80][3]={};
    unsigned count=0,next=0;
    uint32_t received=0;
    bool valid=false;
    bool fresh(uint32_t now) const { return valid && uint32_t(now-received)<600; }
    bool sample(float ax,float ay,float az,uint32_t now) {
        if (!std::isfinite(ax) || !std::isfinite(ay) || !std::isfinite(az) ||
            fabsf(ax)>50 || fabsf(ay)>50 || fabsf(az)>50) { valid=false; return false; }
        x=ax/9.80665f; y=ay/9.80665f; z=az/9.80665f;
        const float r=atan2f(y,z),p=atan2f(-x,sqrtf(y*y+z*z));
        if (!fresh(now)) { roll=r; pitch=p; }
        else {
            roll+=.18f*atan2f(sinf(r-roll),cosf(r-roll));
            pitch+=.18f*(p-pitch);
        }
        history[next][0]=x; history[next][1]=y; history[next][2]=z;
        next=(next+1)%80; if (count<80) ++count;
        received=now; valid=true; return true;
    }
    bool center(uint32_t now) {
        if (!fresh(now) || x*x+y*y+z*z<.25f) return false;
        zero_roll=roll; zero_pitch=pitch; return true;
    }
    float relative_roll() const { return atan2f(sinf(roll-zero_roll),cosf(roll-zero_roll)); }
    float relative_pitch() const { return pitch-zero_pitch; }
};
