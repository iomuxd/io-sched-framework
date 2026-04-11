#include <stdio.h>
#include <signal.h>
#include "daemon/serial.h"
#include "daemon/scheduler.h"
#include "daemon/client_handler.h"

#define DEFAULT_SERIAL_PORT "/dev/ttyUSB0"
#define DEFAULT_BAUDRATE    115200
#define DEFAULT_QUEUE_CAP   64

extern sched_policy_t fifo_policy_create(void);

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : DEFAULT_SERIAL_PORT;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    serial_ctx_t serial;
    if (serial_open(&serial, port, DEFAULT_BAUDRATE) < 0) {
        fprintf(stderr, "ioschedd: failed to open serial port: %s\n", port);
        return 1;
    }

    sched_policy_t policy = fifo_policy_create();

    scheduler_t sched;
    if (sched_init(&sched, policy, &serial, DEFAULT_QUEUE_CAP) < 0) {
        fprintf(stderr, "ioschedd: failed to initialize scheduler\n");
        serial_close(&serial);
        return 1;
    }

    client_handler_t ch;
    if (ch_init(&ch, &sched) < 0) {
        fprintf(stderr, "ioschedd: failed to initialize client handler\n");
        sched_destroy(&sched);
        serial_close(&serial);
        return 1;
    }

    while (g_running) {
        if (ch_poll(&ch) < 0)
            break;
        sched_dispatch(&sched);
    }

    ch_destroy(&ch);
    sched_destroy(&sched);
    serial_close(&serial);

    return 0;
}
