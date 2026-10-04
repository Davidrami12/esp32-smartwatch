#include "watch_time_tools.h"
#include <assert.h>
#include <stdio.h>
int main()
{
    WatchTimeTool tool;
    tool.reset(100);
    tool.toggle(100);
    tool.advance(5100);
    assert(tool.elapsed_ms == 5000 && tool.running);
    tool.toggle(6100);
    assert(tool.elapsed_ms == 6000 && !tool.running);
    tool.advance(12000);
    assert(tool.elapsed_ms == 6000);
    tool.toggle(13000);
    tool.advance(73000); // Screen-off / navigation gap.
    assert(tool.elapsed_ms == 66000);
    tool.reset(0xfffffff0u);
    tool.toggle(0xfffffff0u);
    tool.advance(0x20u);
    assert(tool.elapsed_ms == 48); // uint32 tick rollover.
    tool.reset(20);
    assert(tool.elapsed_ms == 0 && !tool.running);
    puts("PASS: start, pause, resume, screen-off gap, reset, tick rollover");
}
