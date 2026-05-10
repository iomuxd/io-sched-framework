CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -D_GNU_SOURCE
INCLUDES := -Icommon/include -Idaemon/include -Ilibiosched/include -Ifirmware

SRCDIR  := daemon/src
BUILDDIR := build

SRCS := $(SRCDIR)/main.c \
        $(SRCDIR)/client_handler.c \
        $(SRCDIR)/scheduler.c \
        $(SRCDIR)/request_queue.c \
        $(SRCDIR)/serial.c \
        $(SRCDIR)/device_registry.c \
        $(SRCDIR)/policy/fifo.c

OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
TARGET := $(BUILDDIR)/ioschedd

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)
