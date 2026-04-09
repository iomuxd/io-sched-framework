#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* Internal I/O request node — unpacked from ipc_io_request_t wire format */
typedef struct {
    uint32_t req_id;
    uint8_t  cid;
    uint8_t  cmd;
    uint8_t  dev;
    uint8_t  pri;
    uint32_t deadline_ms;       /* 0 = no deadline */
    struct timespec arrive_ts;  /* monotonic clock at enqueue time */
    uint16_t payl_len;
    uint8_t  payload[];         /* flexible array — heap-allocated with extra room */
} io_request_t;

/* Doubly-linked list node wrapping an io_request_t */
typedef struct rq_node {
    struct rq_node *prev;
    struct rq_node *next;
    io_request_t   *req;
} rq_node_t;

/* Request queue (intrusive doubly-linked list) */
typedef struct {
    rq_node_t *head;
    rq_node_t *tail;
    size_t     len;
    size_t     cap;
} request_queue_t;

/* Lifecycle */
int  rq_init(request_queue_t *q, size_t cap);
void rq_destroy(request_queue_t *q);

/* Enqueue / dequeue */
int          rq_push(request_queue_t *q, io_request_t *req);
io_request_t *rq_pop(request_queue_t *q);
rq_node_t    *rq_peek(const request_queue_t *q);

/* Arbitrary removal (for scheduler policy pick & deadline expiry) */
void rq_remove(request_queue_t *q, rq_node_t *node);

/* Bulk removal by client ID (for disconnect / crash cleanup) */
void rq_remove_by_cid(request_queue_t *q, uint8_t cid);

/* Query */
size_t rq_len(const request_queue_t *q);
int    rq_full(const request_queue_t *q);

#endif /* REQUEST_QUEUE_H */
