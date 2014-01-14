CC = gcc
CFLAGS = -Wall -g -pedantic
LDFLAGS = -lpthread -lrt

EXEC = tftpd
SRCS = tftpd.c protocol.c session.c
OBJS = ${SRCS:.c=.o}

all: $(EXEC)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(EXEC) $(OBJS)

clean:
	rm -f $(EXEC) $(OBJS)

