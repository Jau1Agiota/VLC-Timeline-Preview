/* Independent implementation. GPL-2.0-or-later; see ../../COPYING. */
#ifndef PREVIEW_REQUEST_POLICY_H
#define PREVIEW_REQUEST_POLICY_H
#include <stdint.h>
#include <math.h>
enum { UI_WIDTH=800, TRACK_LEFT=16, TRACK_RIGHT=784 };
typedef struct {
    unsigned id;
    double target_ms, emitted_ms, input_age_ms;
} Request;
typedef struct {
    Request active, latest;
    int busy, command_done, seek_seen, restarted, frame_seen, presented;
} Policy;
static int candidate_ready(const Policy *p)
{
    return p->busy && p->command_done && p->seek_seen && p->restarted && p->frame_seen && !p->presented;
}
static int candidate_current(const Policy *p)
{
    return candidate_ready(p) && p->active.id == p->latest.id;
}
static void begin_latest(Policy *p)
{
    p->active=p->latest; p->busy=1;
    p->command_done=p->seek_seen=p->restarted=p->frame_seen=p->presented=0;
}
/* Latest pending request wins scheduling. In-flight output may be shown with its
 * OWN request label, but can never overwrite a newer completed image. */
static int candidate_can_present(const Policy *p,unsigned displayed_id)
{
    return candidate_ready(p) && p->active.id>displayed_id;
}
/* Actual track boundaries; clamps for drawing, rejects hover outside hit region. */
static int hover_target(int x,int left,int right,double duration,double *target)
{
    if(right<=left||!isfinite(duration)||duration<=0)return 0;
    int inside=x>=left&&x<=right;
    if(x<left)x=left;
    if(x>right)x=right;
    *target=duration*(double)(x-left)/(right-left);
    return inside;
}
#endif
