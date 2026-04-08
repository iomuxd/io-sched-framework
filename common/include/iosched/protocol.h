#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Frame makers
#define UART_SOF        0xAA
#define UART_EOF        0x55
#define UART_FRAME_SIZE 8

// CRC-8 polynomial
#define CRC8_POLY   0x07

// Command
#define CMD_READ    0x01
#define CMD_WRITE   0x02
#define CMD_PING    0x03
#define CMD_INFO    0x04

// Status
#define STATUS_OK               0x00
#define STATUS_ERR_UNKNOWN_DEV  0x01
#define STATUS_ERR_UNKNOWN_CMD  0x02
#define STATUS_ERR_CRC          0x03
#define STATUS_ERR_DEV_TIMEOUT  0x04

// Frame structure
typedef struct __attribute__((packed)) {
    uint8_t sof;
    uint8_t seq;
    uint8_t cmd;
    uint8_t dev;
    uint16_t payload;
    uint8_t crc;
    uint8_t eof;
} uart_frame_t;

#endif /* PROTOCOL_H */
