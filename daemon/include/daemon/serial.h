#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stdint.h>
#include <iosched/protocol.h>

/** Serial port context: holds fd and original termios for restoration */
typedef struct {
    /** Serial port file descriptor */
    int fd;
    /** Original termios saved on open */
    struct termios orig;
    /** Frame sequence number, auto-incremented on each send */
    uint8_t seq;
} serial_ctx_t;

// Serial port functions
int serial_open(serial_ctx_t *ctx, const char *port, int baudrate);
void serial_close(serial_ctx_t *ctx);

// Serial communication functions
int serial_send(serial_ctx_t *ctx, uart_frame_t *frame);
int serial_recv(serial_ctx_t *ctx, uart_frame_t *frame, int timeout_ms);

#endif /* SERIAL_H */
