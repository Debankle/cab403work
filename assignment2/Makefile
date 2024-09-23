C_SOURCES = $(wildcard *.c)
OBJECTS = $(C_SOURCES:.c=.o)
COMPONENTS = $(C_SOURCES:.c=)

CC = gcc
CFLAGS_DEBUG = -Wextra -Werror -g -DDEBUG -MMD -MP
CFLAGS = -Wall -pthread

ifdef DEBUG
CFLAGS += $(CFLAGS_DEBUG)
endif

.PHONY: all clean

all: $(COMPONENTS)

%: %.o
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(COMPONENTS) *.d

-include $(wildcard *.d)