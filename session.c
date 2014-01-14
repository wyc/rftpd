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

#include "tftpd.h"
#include "session.h"

tftpd_conn *sessions[MAX_SESSIONS];
int sessions_ct;

int add_session(const tftpd_conn *conn, tftpd_conn **new_conn)
{
        int i;
        tftpd_conn *copy;
        for (i = 0; i < MAX_SESSIONS; i++) {
                if (sessions[i]) {
                        continue;
                }

                copy = malloc(sizeof(tftpd_conn));
                if (!copy) {
                        perror("malloc");
                        return -1;
                }
                memcpy(copy, conn, sizeof(tftpd_conn));
                *new_conn = sessions[i] = copy;
                return 0;
        }

        return -1;
}

int del_session(tftpd_conn *conn)
{
        int i;
        for (i = 0; i < MAX_SESSIONS; i++) {
                if (conn == sessions[i]) {
                        free(sessions[i]);
                        sessions[i] = NULL;
                        return 0;
                }
        }
        return -1;
}

int count_sessions()
{
        int i, ct;
        for (i = 0, ct = 0; i < MAX_SESSIONS; i++) {
                if (sessions[i]) {
                        ct++;
                }
        }
        return ct;
}

