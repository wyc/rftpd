#ifndef _SESSION_H
#define _SESSION_H

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

typedef struct tftpd_conn {
        struct sockaddr_in client;
        struct sockaddr_in serv;
        socklen_t client_len;
        socklen_t serv_len;
        int fd;
        int finished;

        uint16_t opcode;
        uint16_t block;
        FILE *fh;
        
        struct timeval last_modified;
} tftpd_conn;

int add_session(const tftpd_conn *conn, tftpd_conn **new_conn);
int del_session(tftpd_conn *conn);
int count_sessions();
/*
int del_old_sessions(tftpd_conn *conn, struct timeval elapsed);
int del_all_sessions();
*/

/* @todo better sessioning system */
extern tftpd_conn *sessions[MAX_SESSIONS];
extern int sessions_ct;
#endif

