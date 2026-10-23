/* GPL-2.0-or-later. Exercise the actual acceptance/coalescing policy. */
#include "request_policy.h"
#include <assert.h>
int main(void) {
    Policy p={0};p.latest.id=1;p.latest.target_ms=29950;begin_latest(&p);
    assert(!candidate_ready(&p));p.command_done=1;assert(!candidate_ready(&p));
    p.seek_seen=1;p.restarted=1;assert(!candidate_ready(&p)); /* no decoded frame */
    p.frame_seen=1;assert(candidate_ready(&p)&&candidate_current(&p));
    p.latest.id=2;p.latest.target_ms=1000;assert(candidate_ready(&p)&&!candidate_current(&p));
    for(unsigned i=3;i<1000;++i){p.latest.id=i;p.latest.target_ms=i;}
    begin_latest(&p);assert(p.active.id==999&&p.active.target_ms==999&&!candidate_ready(&p));
    p.command_done=p.seek_seen=p.restarted=p.frame_seen=1;p.presented=1;assert(!candidate_ready(&p));
    p.presented=0;p.busy=0;assert(!candidate_ready(&p));return 0;
}
