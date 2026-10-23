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
    p.presented=0;p.busy=0;assert(!candidate_ready(&p));double target=-1;
    assert(hover_target(12,12,308,7200000,&target)&&target==0);
    assert(hover_target(308,12,308,7200000,&target)&&target==7200000);
    assert(hover_target(160,12,308,7200000,&target)&&target==3600000);
    assert(!hover_target(-9,12,308,7200000,&target)&&target==0);
    assert(!hover_target(999,12,308,7200000,&target)&&target==7200000);
    assert(!hover_target(20,12,12,7200000,&target));
    assert(!hover_target(20,12,308,0,&target));
    assert(hover_target(TRACK_LEFT,TRACK_LEFT,TRACK_RIGHT,10800000,&target)&&target==0);
    assert(hover_target(TRACK_RIGHT,TRACK_LEFT,TRACK_RIGHT,10800000,&target)&&target==10800000);
    assert(hover_target(UI_WIDTH/2,TRACK_LEFT,TRACK_RIGHT,10800000,&target)&&target==5400000);
    return 0;
}
