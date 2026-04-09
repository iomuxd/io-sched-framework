#ifndef SERIAL_H
#define SERIAL_H

#include <iosched/protocol.h>

// Serial port functions
int serial_open(serial_ctx_t *ctx, const char *port, int baudrate);
void serial_close(serial_ctx_t *ctx);

// Serial communication functions
int serial_send(serial_ctx_t *ctx, uart_frame_t *frame);
int serial_recv(serial_ctx_t *ctx, uart_frame_t *frame, int timeout_ms);

#endif /* SERIAL_H */
