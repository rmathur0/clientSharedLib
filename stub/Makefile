#
#Makefile for load-balanced tcp client module
#
CC = gcc  # C compiler
CFLAGS = -Wall -Wextra -O2 -g  # C flags
LDFLAGS = -L/usr/local/src/tcp_lb/mod_tcpclient -ltcp_clb -lpthread -levent_pthreads  # linking flags
RM = rm -f   # rm command
TARGET_LIB = stub  # target

SRCS = $(shell echo src/*.c)  # source files
#HDRS = $(shell echo include/*.h)  # header files
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)
