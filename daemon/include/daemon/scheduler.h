#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include <daemon/request_queue.h>
#include <daemon/serial.h>

/* Scheduling policy vtable — set once at daemon startup, immutable thereafter */
typedef struct {
    const char *name;
    int  (*init)(void *ctx);
    void (*destroy)(void *ctx);
    rq_node_t *(*select)(request_queue_t *q, void *ctx);
    void *ctx;
} sched_policy_t;

/* Scheduler core */
typedef struct {
    request_queue_t   queue;
    sched_policy_t    policy;
    serial_ctx_t     *serial;
    uint32_t          total_reqs;
    uint64_t          total_wait_us;
} scheduler_t;

/* Lifecycle */
int  sched_init(scheduler_t *sched, sched_policy_t policy,
                serial_ctx_t *serial, size_t queue_cap);
void sched_destroy(scheduler_t *sched);

/* Operations */
int  sched_submit(scheduler_t *sched, io_request_t *req);
int  sched_dispatch(scheduler_t *sched);

#endif /* SCHEDULER_H */
