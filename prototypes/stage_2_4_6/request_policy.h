/* Independent implementation. GPL-2.0-or-later; see ../../COPYING. */
#ifndef PREVIEW_REQUEST_POLICY_H
#define PREVIEW_REQUEST_POLICY_H
#include <stdint.h>
typedef struct {
    unsigned id;
    double target_ms, emitted_ms;
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
#endif
