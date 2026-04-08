#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// IPC message types
#define IPC_CONNECT         0x01
#define IPC_CONNECT_ACK     0x02
#define IPC_DISCONNECT      0x03
#define IPC_IO_REQUEST      0x10
#define IPC_IO_RESPONSE     0x11
#define IPC_GET_STATUS      0x20
#define IPC_STATUS_REPORT   0x21

// IPC status codes
#define IPC_STATUS_OK                   0x00
#define IPC_STATUS_ERR_UNKNOWN_DEV      0x01
#define IPC_STATUS_ERR_UNKNOWN_CMD      0x02
#define IPC_STATUS_ERR_DEADLINE         0x03
#define IPC_STATUS_ERR_DEV_TIMEOUT      0x04
#define IPC_STATUS_ERR_QUEUE_FULL       0x05
#define IPC_STATUS_ERR_INVALID_CLIENT   0x06

// Scheduling policies
#define POLICY_FIFO 0x01
#define POLICY_PRI  0x02
#define POLICY_SJF  0x03
#define POLICY_SRTF 0x04
#define POLICY_EDF  0x05
#define POLICY_RR   0x06

// IPC socket path and backlog
#define IPC_SOCKET_PATH "/tmp/io_scheduler.sock"
#define IPC_MAX_BACKLOG 16

// IPC message structures
typedef struct __attribute__((packed)) {
    uint16_t len;
    uint8_t type;
    uint8_t cid;
} ipc_header_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
    uint8_t name_len;
    char app_name[];
} ipc_connect_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
    uint8_t status;
} ipc_connect_ack_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
} ipc_disconnect_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
    uint32_t req_id;
    uint8_t cmd;
    uint8_t dev;
    uint8_t pri;
    uint32_t deadline;
    uint16_t payl_len;
    uint8_t payload[];
} ipc_io_request_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
    uint32_t req_id;
    uint8_t status;
    uint16_t payl_len;
    uint8_t payload[];
} ipc_io_response_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
} ipc_get_status_t;

typedef struct __attribute__((packed)) {
    ipc_header_t header;
    uint8_t pol;
    uint8_t q_len;
    uint32_t total_reqs;
    uint8_t c_cnt;
    uint16_t avg_wait;
} ipc_status_report_t;

#endif /* TYPES_H */
