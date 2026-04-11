#include <poll.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include "iosched/types.h"
#include "daemon/request_queue.h"
#include "daemon/client_handler.h"

int ch_init(client_handler_t *ch, scheduler_t *sched) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ch->clients[i].fd  = -1;
        ch->clients[i].cid = 0;
    }
    ch->client_cnt = 0;
    ch->next_cid   = 0;

    ch->sched = sched;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;

    unlink(IPC_SOCKET_PATH);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        close(fd);
        return -1;
    }
    
    if (listen(fd, IPC_MAX_BACKLOG)) {
        close(fd);
        unlink(IPC_SOCKET_PATH);
        return -1;
    }

    ch->listen_fd = fd;

    return 0;
}

void ch_destroy(client_handler_t *ch) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        ch_disconnect(ch, i);
    close(ch->listen_fd);
    unlink(IPC_SOCKET_PATH);
    ch->listen_fd = -1;
}

int ch_accept(client_handler_t *ch) {
    int client_fd = accept(ch->listen_fd, NULL, NULL);
    if (client_fd < 0)
        return -1;

    if (ch->client_cnt >= MAX_CLIENTS) {
        close(client_fd);
        return -1;
    }

    int free_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ch->clients[i].fd == -1) {
            free_idx = i;
            break;
        }
    }
    if (free_idx == -1) {
        close(client_fd);
        return -1;
    }

    ch->clients[free_idx].fd  = client_fd;
    ch->clients[free_idx].cid = ++ch->next_cid;
    ch->client_cnt++;

    return free_idx;
}

void ch_disconnect(client_handler_t *ch, int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS)
        return;

    if (ch->clients[idx].fd == -1)
        return;

    rq_remove_by_cid(&ch->sched->queue, ch->clients[idx].cid);

    close(ch->clients[idx].fd);

    ch->clients[idx].fd = -1;
    ch->clients[idx].cid = 0;
    memset(ch->clients[idx].app_name, 0, sizeof(ch->clients[idx].app_name));

    ch->client_cnt--;
}

int ch_handle_message(client_handler_t *ch, int idx) {
    ipc_header_t header;
    int fd = ch->clients[idx].fd;
    ssize_t n = read(fd, &header, sizeof(header));
    if (n <= 0) {
        ch_disconnect(ch, idx);
        return -1;
    }

    switch (header.type) {
        case IPC_CONNECT: {
            uint8_t name_len;
            read(fd, &name_len, sizeof(name_len));

            if (name_len >= sizeof(ch->clients[idx].app_name))
                name_len = sizeof(ch->clients[idx].app_name) - 1;

            read(fd, ch->clients[idx].app_name, name_len);
            ch->clients[idx].app_name[name_len] = '\0';

            ipc_connect_ack_t ack;
            ack.header.type = IPC_CONNECT_ACK;
            ack.header.cid  = ch->clients[idx].cid;
            ack.header.len  = sizeof(ack);
            ack.status = IPC_STATUS_OK;

            write(fd, &ack, sizeof(ack));
            break;
        }
        case IPC_IO_REQUEST: {
            uint32_t req_id;
            uint8_t  cmd, dev, pri;
            uint32_t deadline;
            uint16_t payl_len;

            read(fd, &req_id,    sizeof(req_id));
            read(fd, &cmd,       sizeof(cmd));
            read(fd, &dev,       sizeof(dev));
            read(fd, &pri,       sizeof(pri));
            read(fd, &deadline,  sizeof(deadline));
            read(fd, &payl_len,  sizeof(payl_len));

            io_request_t *ioreq = malloc(sizeof(io_request_t) + payl_len);
            if (ioreq == NULL)
                return -1;

            if (payl_len > 0)
                read(fd, ioreq->payload, payl_len);

            ioreq->req_id      = req_id;
            ioreq->cid         = header.cid;
            ioreq->cmd         = cmd;
            ioreq->dev         = dev;
            ioreq->pri         = pri;
            ioreq->deadline_ms = deadline;
            ioreq->payl_len    = payl_len;
            clock_gettime(CLOCK_MONOTONIC, &ioreq->arrive_ts);

            if (sched_submit(ch->sched, ioreq) < 0) {
                free(ioreq);
                ipc_io_response_t err;
                err.header.type = IPC_IO_RESPONSE;
                err.header.cid  = header.cid;
                err.header.len  = sizeof(err);
                err.req_id = req_id;
                err.status = IPC_STATUS_ERR_QUEUE_FULL;
                err.payl_len = 0;
                write(fd, &err, sizeof(err));
                return -1;
            }
            break;
        }
        case IPC_DISCONNECT:
            ch_disconnect(ch, idx);
            break;
        case IPC_GET_STATUS: {
            ipc_status_report_t report;
            report.header.type = IPC_STATUS_REPORT;
            report.header.cid  = header.cid;
            report.header.len  = sizeof(report);
            report.pol         = ch->sched->policy.id;
            report.q_len       = (uint8_t)rq_len(&ch->sched->queue);
            report.total_reqs  = ch->sched->total_reqs;
            report.c_cnt       = (uint8_t)ch->client_cnt;
            report.avg_wait    = ch->sched->total_reqs > 0
                ? (uint16_t)(ch->sched->total_wait_us / ch->sched->total_reqs / 1000) : 0;
            write(fd, &report, sizeof(report));
            break;
        }
        default:
            break;
    }

    return 0;
}

int ch_poll(client_handler_t *ch) {
    struct pollfd fds[1 + MAX_CLIENTS];
    fds[0].fd     = ch->listen_fd;
    fds[0].events = POLLIN;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i + 1].fd     = ch->clients[i].fd;
        fds[i + 1].events = POLLIN;
    }
    
    int n = poll(fds, 1 + MAX_CLIENTS, -1);

    if (n == -1) {
        if (errno == EINTR)
            return 0;
        else
            return -1;
    }

    if (fds[0].revents & POLLIN)
        ch_accept(ch);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fds[i + 1].revents & POLLIN)
            ch_handle_message(ch, i);
    }

    return n;
}
