#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#include <daemon/scheduler.h>

#define MAX_CLIENTS 16

/* Per-client state */
typedef struct {
    int      fd;           /* socket fd, -1 = unused slot */
    uint8_t  cid;          /* assigned client ID (1–255, 0 = unassigned) */
    char     app_name[64]; /* application name from IPC_CONNECT */
} client_t;

/* Client handler context — owns the listen socket and client table */
typedef struct {
    int           listen_fd;
    client_t      clients[MAX_CLIENTS];
    size_t        client_cnt;
    scheduler_t  *sched;
} client_handler_t;

/* Lifecycle */
int  ch_init(client_handler_t *ch, scheduler_t *sched);
void ch_destroy(client_handler_t *ch);

/* Connection management */
int  ch_accept(client_handler_t *ch);
void ch_disconnect(client_handler_t *ch, int idx);

/* Message processing */
int  ch_handle_message(client_handler_t *ch, int idx);

/* Event loop */
int  ch_poll(client_handler_t *ch);

#endif /* CLIENT_HANDLER_H */
