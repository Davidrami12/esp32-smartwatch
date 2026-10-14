#include "watch_voice_window.h"
#include <cassert>
#include <cstdio>

int main()
{
    WatchVoiceWindow window;
    assert(!window.grant(0,1,0,4,82560));
    assert(!window.grant(77,0,0,4,82560));
    assert(!window.grant(77,1,1,4,82560));
    assert(!window.grant(77,1,0,5,82560));
    assert(window.grant(77,1,0,4,82560));
    assert(!window.grant(77,1,0,4,82560)); // Replay does not refill credits.
    assert(!window.grant(78,2,0,4,82560));
    for (unsigned i = 0; i < 4; ++i) {
        assert(window.sequence == i);
        const auto count = window.payload(512,82560);
        assert(count == 496 && window.last(count,82560) == (i == 3));
        window.sent(count,82560);
    }
    assert(window.credits == 0 && window.payload(512,82560) == 0);
    assert(window.grant(77,2,496,4,82560)); // Retransmit a verified prefix.
    unsigned frames = 0, grants = 0;
    window.clear();
    uint32_t offset = 0;
    while (offset < 82560) {
        assert(window.grant(77,++grants,offset,4,82560));
        while (window.credits) {
            auto count = window.payload(512,82560);
            assert(count && !(count & 1));
            window.sent(count,82560); ++frames;
        }
        offset = window.offset;
    }
    assert(frames == 167 && grants == 42 && offset == 82560);
    window.clear(); assert(window.payload(512,82560) == 0);
    puts("PASS: bounded notification credits, session/epoch replay rejection, retry offset and 167-frame/42-grant baseline simulation");
}
