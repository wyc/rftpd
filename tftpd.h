#ifndef _TFTPD_H
#define _TFTPD_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#define MAX_SESSIONS 16

#define LISTENQ 20
#define BUF_LEN 8192
#define PATHNAME_LEN 128

#define OP_RRQ          1
#define OP_WRQ          2
#define OP_DATA         3
#define OP_ACK          4
#define OP_ERROR        5
#define OP_OACK         6

#endif

