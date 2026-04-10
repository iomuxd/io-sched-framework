#include "daemon/scheduler.h"

/* Return the oldest request in the queue (head = first arrived). */
static rq_node_t *fifo_select(request_queue_t *q, void *ctx) {
    (void)ctx;
    return q->head;
}

/* Create a FIFO policy — no internal state needed. */
sched_policy_t fifo_policy_create(void) {
    sched_policy_t p = {
        .name    = "fifo",
        .init    = NULL,
        .destroy = NULL,
        .select  = fifo_select,
        .ctx     = NULL,
    };
    return p;
}
