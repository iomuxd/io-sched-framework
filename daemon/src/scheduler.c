#include "daemon/serial.h"
#include "daemon/scheduler.h"
#include "iosched/protocol.h"
#include "daemon/request_queue.h"
#include <time.h>
#include <stdint.h>
#include <string.h>

#define DEFAULT_TIMEOUT_MS 1000

/* Pack io_request_t into a fixed-size UART frame.
 * Copies at most 2 bytes from the flexible-array payload via memcpy
 * to avoid unaligned access and out-of-bounds reads. */
static uart_frame_t build_frame(const io_request_t *req) {
    uint16_t payload = 0;
    if (req->payl_len > 0) {
        size_t copy_len = req->payl_len < 2 ? req->payl_len : 2;
        memcpy(&payload, req->payload, copy_len);
    }

    uart_frame_t frame = {
        .cmd = req->cmd,
        .dev = req->dev,
        .payload = payload,
    };
    return frame;
}

/* Initialize scheduler: set up request queue, bind policy and serial context. */
int sched_init(scheduler_t *sched, sched_policy_t policy,
               serial_ctx_t *serial, size_t queue_cap) {
    rq_init(&sched->queue, queue_cap);

    sched->policy = policy;
    sched->serial = serial;
    sched->total_reqs = 0;
    sched->total_wait_us = 0;

    if (sched->policy.init != NULL) {
        if (sched->policy.init(sched->policy.ctx))
            return -1;
    }

    return 0;
}

/* Tear down policy state and free the request queue. */
void sched_destroy(scheduler_t *sched) {
    if (sched->policy.destroy != NULL)
        sched->policy.destroy(sched->policy.ctx);
    rq_destroy(&sched->queue);
}

/* Enqueue a new I/O request into the scheduler's queue. */
int sched_submit(scheduler_t *sched, io_request_t *req) {
    return rq_push(&sched->queue, req);
}

/* Select next request via policy, send to device, and collect stats.
 * On serial failure the request is removed from the queue and -1 is returned. */
int sched_dispatch(scheduler_t *sched) {
    rq_node_t *node = sched->policy.select(&sched->queue, sched->policy.ctx);
    if (node == NULL)
        return -1;

    uart_frame_t frame = build_frame(node->req);
    int timeout = node->req->deadline_ms > 0 ? node->req->deadline_ms : DEFAULT_TIMEOUT_MS;

    if (serial_send(sched->serial, &frame) < 0) {
        rq_remove(&sched->queue, node);
        return -1;
    }
    if (serial_recv(sched->serial, &frame, timeout) < 0) {
        rq_remove(&sched->queue, node);
        return -1;
    }

    /* Compute turnaround time: now − arrival (monotonic, microseconds) */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t wait_us = (now.tv_sec - node->req->arrive_ts.tv_sec) * 1000000
                     + (now.tv_nsec - node->req->arrive_ts.tv_nsec) / 1000;

    rq_remove(&sched->queue, node);

    sched->total_reqs++;
    sched->total_wait_us += wait_us;

    return 0;
}