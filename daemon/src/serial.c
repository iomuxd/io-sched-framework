#include <poll.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include "daemon/serial.h"
#include "iosched/protocol.h"

/* Convert integer baudrate to termios speed flag */
static speed_t baudrate_to_flag(int baudrate) {
    switch (baudrate) {
        case 9600:      return B9600;
        case 19200:     return B19200;
        case 38400:     return B38400;
        case 57600:     return B57600;
        case 115200:    return B115200;
        default:        return B9600;
    }
}

/* Compute CRC-8 checksum over data buffer using CRC8_POLY */
static uint8_t crc8_calc(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ CRC8_POLY;
            else
                crc <<= 1;
        }
    }

    return crc;
}

/*
 * Open and configure a serial port for raw 8N1 communication.
 * Returns 0 on success, -1 on failure.
 */
int serial_open(serial_ctx_t *ctx, const char *port, int baudrate) {
    /* Open port in non-controlling, synchronous mode */
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
        return -1;

    /* Save original termios for restoration on close */
    ctx->fd = fd;
    int tc = tcgetattr(fd, &ctx->orig);
    if (tc < 0) {
        close(fd);
        ctx->fd = -1;
        return -1;
    }

    /* Configure raw mode on a working copy */
    struct termios tty = ctx->orig;
    cfmakeraw(&tty);

    /* Set baudrate */
    speed_t speed = baudrate_to_flag(baudrate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1, no hardware flow control, enable receiver */
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= (CLOCAL | CREAD);

    /* Blocking read, return after at least 1 byte */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    /* Apply configuration */
    tc = tcsetattr(fd, TCSANOW, &tty);
    if (tc < 0) {
        close(fd);
        ctx->fd = -1;
        return -1;
    }

    return 0;
}

/*
 * Drain pending output, restore original termios, and close the port.
 */
void serial_close(serial_ctx_t *ctx) {
    if (ctx->fd < 0)
        return;

    /* Flush remaining data before restoring settings */
    tcdrain(ctx->fd);
    tcsetattr(ctx->fd, TCSADRAIN, &ctx->orig);
    close(ctx->fd);

    ctx->fd = -1;
}

/*
 * Build and send a UART frame: set SOF/EOF, compute CRC, write to port.
 * Returns 0 on success, -1 on failure.
 */
int serial_send(serial_ctx_t *ctx, uart_frame_t *frame) {
    /* Fill frame markers and CRC (caller's frame is modified in-place) */
    frame->sof = UART_SOF;
    frame->eof = UART_EOF;
    frame->crc = crc8_calc(&frame->seq, 5);

    ssize_t written = write(ctx->fd, frame, UART_FRAME_SIZE);
    if (written != UART_FRAME_SIZE)
        return -1;

    return 0;
}

/*
 * Receive a UART frame: wait for SOF, read remaining bytes, verify CRC.
 * Returns 0 on success, -1 on timeout or error.
 */
int serial_recv(serial_ctx_t *ctx, uart_frame_t *frame, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = ctx->fd;
    pfd.events = POLLIN;

    /* Wait for incoming data within the timeout window */
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret <= 0 || !(pfd.revents & POLLIN))
        return -1;

    /* Scan for SOF byte, bail out after max_scan attempts to avoid hang */
    uint8_t byte = 0;
    int max_scan = UART_FRAME_SIZE * 2;
    while (byte != UART_SOF) {
        ssize_t n = read(ctx->fd, &byte, 1);

        if (n <= 0 || --max_scan <= 0)
            return -1;
    }

    /* Read remaining 7 bytes (seq through eof) after SOF */
    frame->sof = UART_SOF;
    uint8_t *buf = (uint8_t *)&frame->seq;
    size_t remaining = UART_FRAME_SIZE - 1;
    while (remaining > 0) {
        ssize_t n = read(ctx->fd, buf, remaining);

        if (n <= 0)
            return -1;

        buf += n;
        remaining -= n;
    }

    /* Validate frame integrity: EOF marker and CRC */
    if (frame->eof != UART_EOF)
        return -1;

    uint8_t calc_crc = crc8_calc(&frame->seq, 5);
    if (frame->crc != calc_crc)
        return -1;

    return 0;
}
