#include "daemon/serial.h"
#include "daemon/scheduler.h"
#include "iosched/protocol.h"
#include "daemon/request_queue.h"
#include "iosched/types.h"
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define DEFAULT_TIMEOUT_MS 1000
#define MAX_PAYLOAD_LEN    2

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

static uint8_t uart_to_ipc_status(const uint8_t uart_status) {
    switch (uart_status) {
        case STATUS_OK:              return IPC_STATUS_OK;
        case STATUS_ERR_UNKNOWN_DEV: return IPC_STATUS_ERR_UNKNOWN_DEV;
        case STATUS_ERR_UNKNOWN_CMD: return IPC_STATUS_ERR_UNKNOWN_CMD;
        case STATUS_ERR_DEV_TIMEOUT: return IPC_STATUS_ERR_DEV_TIMEOUT;
        default:                     return IPC_STATUS_ERR_DEV_TIMEOUT;
    }
}

/* Initialize scheduler: set up request queue, bind policy and serial context. */
int sched_init(scheduler_t *sched, sched_policy_t policy,
               serial_ctx_t *serial, size_t queue_cap) {
    if (rq_init(&sched->queue, queue_cap) < 0)
        return -1;

    sched->policy        = policy;
    sched->serial        = serial;
    sched->total_reqs    = 0;
    sched->total_wait_us = 0;
    sched->sink          = NULL;
    sched->sink_ctx      = NULL;

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

/* Enqueue a new I/O request into the scheduler's queue.
 * Rejects requests whose payload exceeds the UART frame capacity (2 bytes). */
int sched_submit(scheduler_t *sched, io_request_t *req) {
    if (req->payl_len > MAX_PAYLOAD_LEN)
        return -1;
    return rq_push(&sched->queue, req);
}

/* Select next request via policy, send to device, and collect stats.
 * Returns:  1 = queue empty (idle)
 *           0 = dispatched successfully
 *          -1 = serial error */
int sched_dispatch(scheduler_t *sched) {
    rq_node_t *node = sched->policy.select(&sched->queue, sched->policy.ctx);
    if (node == NULL)
        return 1;

    if (node->req->deadline_ms != 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec  - node->req->arrive_ts.tv_sec)  * 1000L
                        + (now.tv_nsec - node->req->arrive_ts.tv_nsec) / 1000000L;
        if (elapsed_ms > (long)node->req->deadline_ms) {
            rq_remove(&sched->queue, node);
            if (sched->sink) {
                sched->sink(sched->sink_ctx,
                            node->req->cid,
                            node->req->req_id,
                            IPC_STATUS_ERR_DEADLINE,
                            NULL,
                            0);
            }
            free(node->req);
            free(node);
            return 0;
        }
    }

    uart_frame_t frame = build_frame(node->req);
    uint32_t dl = node->req->deadline_ms > 0 ? node->req->deadline_ms : DEFAULT_TIMEOUT_MS;
    int timeout = dl > (uint32_t)INT_MAX ? INT_MAX : (int)dl;
    bool    ok     = false;
    uint8_t status = IPC_STATUS_ERR_DEV_TIMEOUT;

    for (int i = 0; i < 3; i++) {
        if (serial_send(sched->serial, &frame) >= 0 &&
            serial_recv(sched->serial, &frame, timeout) >= 0) {
            ok     = true;
            status = uart_to_ipc_status(frame.cmd);
            break;
        }
    }

    if (!ok) {
        rq_remove(&sched->queue, node);
        if (sched->sink) {
            sched->sink(sched->sink_ctx,
                        node->req->cid,
                        node->req->req_id,
                        IPC_STATUS_ERR_DEV_TIMEOUT,
                        NULL,
                        0);
        }
        free(node->req);
        free(node);
        return -1;
    }

    if (sched->sink) {
        sched->sink(sched->sink_ctx,
                    node->req->cid,
                    node->req->req_id,
                    status,
                    (const uint8_t *)&frame.payload,
                    2);
    }

    /* Compute turnaround time: now − arrival (monotonic, microseconds) */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long nsec_diff = now.tv_nsec - node->req->arrive_ts.tv_nsec;
    uint64_t wait_us = (uint64_t)(now.tv_sec - node->req->arrive_ts.tv_sec) * 1000000ULL;
    if (nsec_diff < 0) {
        wait_us -= 1000000;
        nsec_diff += 1000000000L;
    }
    wait_us += (uint64_t)nsec_diff / 1000;

    rq_remove(&sched->queue, node);
    free(node->req);
    free(node);

    sched->total_reqs++;
    sched->total_wait_us += wait_us;

    return 0;
}

void sched_set_response_sink(scheduler_t *sched, response_sink_t sink, void *ctx) {
    sched->sink = sink;
    sched->sink_ctx = ctx;
}
