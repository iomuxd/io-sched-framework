#include <time.h>
#include <stdlib.h>
#include "daemon/request_queue.h"

int rq_init(request_queue_t *q, size_t cap) {
    q->head = NULL;
    q->tail = NULL;
    q->len  = 0;
    q->cap  = cap;

    return 0;
}

void rq_destroy(request_queue_t *q) {
    rq_node_t *node = q->head;

    while (node) {
        rq_node_t *next = node->next;
        free(node->req);
        free(node);
        node = next;
    }

    q->head = NULL;
    q->tail = NULL;
    q->len  = 0;
}

int rq_push(request_queue_t *q, io_request_t *req) {
    if (rq_full(q))
        return -1;

    rq_node_t *node = (rq_node_t *)malloc(sizeof(rq_node_t));
    if (!node)
        return -1;

    node->next = NULL;
    node->req  = req;

    clock_gettime(CLOCK_MONOTONIC, &req->arrive_ts);

    if (q->tail == NULL) {
        node->prev = NULL;
        q->head = node;
    } else {
        node->prev = q->tail;
        q->tail->next = node;
    }

    q->tail = node;
    q->len++;

    return 0;
}

io_request_t *rq_pop(request_queue_t *q) {
    rq_node_t *node = q->head;
    if (!node)
        return NULL;

    rq_remove(q, node);
    io_request_t *req = node->req;
    free(node);

    return req;
}

rq_node_t *rq_peek(const request_queue_t *q) {
    return q->head;
}

void rq_remove(request_queue_t *q, rq_node_t *node) {
    if (q->head == node && q->tail == node) {
        q->head = NULL;
        q->tail = NULL;
    } else if (q->head == node) {
        q->head->next->prev = NULL;
        q->head = q->head->next;
    } else if (q->tail == node) {
        q->tail->prev->next = NULL;
        q->tail = q->tail->prev;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    node->next = NULL;
    node->prev = NULL;
    q->len--;
}

void rq_remove_by_cid(request_queue_t *q, uint8_t cid) {
    rq_node_t *node = q->head;

    while (node) {
        rq_node_t *next = node->next;
        if (node->req->cid == cid) {
            rq_remove(q, node);
            free(node->req);
            free(node);
        }
        node = next;
    }
}

size_t rq_len(const request_queue_t *q) {
    return q->len;
}

int rq_full(const request_queue_t *q) {
    return q->len >= q->cap;
}
