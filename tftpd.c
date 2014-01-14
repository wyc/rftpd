/*
 * tftpd.c - A Simple TFTP Server
 *
 */ 

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
#include "protocol.h"

/* @todo decide whether to separate handling of the base/session conns */

int setup_rrq(const tftpd_conn *conn, const char *buf)
{
        int rv;
        tftpd_conn *new_conn;

        rv = add_session(conn, &new_conn);
        if (rv == -1) {
                fprintf(stderr, "add_session() failed\n");
                return -1;
        }
        fprintf(stderr, "--session count now at %d\n", count_sessions());

        rv = handle_rrq(new_conn, buf);
        if (rv == -1) {
                fprintf(stderr, "handle_rrq() failed\n");
                rv = del_session(new_conn);
                if (rv == -1) {
                        fprintf(stderr, "del_session() failed\n");
                }
                return -1;
        }

        rv = send_block(new_conn);
        if (rv == -1) {
                fprintf(stderr, "send_block() failed\n");
                teardown_rq(new_conn);
                rv = del_session(new_conn);
                if (rv == -1) {
                        fprintf(stderr, "del_session() failed\n");
                }
                return -1;
        }

        return 0;
}

int handle_request(tftpd_conn *conn)
{
        char buf[BUF_LEN];
        size_t n;
        int rv, i;

        n = recvfrom(conn->fd, buf, BUF_LEN, 0,
                     (struct sockaddr *)&conn->client,
                     &conn->client_len);
        if (n == -1) {
                perror("recvfrom");
                return -1;
        }

        {
                puts("---RCV MSG---");
                for (i = 0; i < n; i++)
                        printf("%x ", buf[i]);
                puts("");
                printf("from %s:%d to %s:%d\n",
                       inet_ntoa(conn->client.sin_addr),
                       ntohs(conn->client.sin_port),
                       inet_ntoa(conn->serv.sin_addr),
                       ntohs(conn->serv.sin_port));
                puts("---END MSG---");
        }


        if (is_rrq(buf, n)) {
                rv = setup_rrq(conn, buf);
                if (rv == -1) {
                        fprintf(stderr, "setup_rrq() failed\n");
                        return -1;
                }
        } else if (is_ack(buf, n)) {
                rv = handle_ack(conn, buf);
                if (rv == -1) {
                        fprintf(stderr, "handle_ack() failed\n");
                        return -1;
                }

                if (conn->finished) {
                        fprintf(stderr,
                                "conn (fd = %d) finished! tearing down\n",
                                conn->fd);
                        teardown_rq(conn);
                        rv = del_session(conn);
                        if (rv == -1) {
                                fprintf(stderr, "del_session() failed \n");
                                return -1;
                        }
                        fprintf(stderr, "--session count now at %d\n", count_sessions());
                }
        } else {
                rv = send_error(conn, 4, "Illegal Operation");
                if (rv == -1) {
                        fprintf(stderr, "send_error() failed\n");
                        return -1;
                }
        }

        return 0;
}

int tftpd_listen(tftpd_conn *bconn)
{
        fd_set rfds;
        tftpd_conn *conn;
        int rv, maxfd, i;

        FD_ZERO(&rfds);

        /* prime base conn */
        FD_SET(bconn->fd, &rfds);
        maxfd = bconn->fd;

        /* prime active sessions */
        for (i = 0; i < MAX_SESSIONS; i++) {
                if (!sessions[i]) {
                        continue;
                }

                conn = sessions[i];
                FD_SET(conn->fd, &rfds);
                if (conn->fd > maxfd) {
                        maxfd = conn->fd;
                }
        }

        puts("LISTENING!");
        rv = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (rv == -1) {
                perror("select");
        }

        /* handle base conn */
        if (FD_ISSET(bconn->fd, &rfds)) {
                rv = handle_request(bconn);
                if (rv == -1) {
                        fprintf(stderr, "handle_request() for bconn failed\n");
                        return -1;
                }
        }

        /* handle active sessions */
        for (i = 0; i < MAX_SESSIONS; i++) {
                if (!sessions[i]) {
                        continue;
                }
                conn = sessions[i];
                if (!FD_ISSET(conn->fd, &rfds)) {
                        continue;
                }

                rv = handle_request(conn);
                if (rv == -1) {
                        fprintf(stderr, "handle_request() failed\n");
                        return -1;
                }
        }

        return 0;
}

int tftpd_serve(uint16_t port)
{
        int rv;
        tftpd_conn bconn;

        signal(SIGPIPE, SIG_IGN);

        bconn.fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (bconn.fd == -1) {
                perror("socket");
                return -1;
        }

        bconn.fh = NULL;
        bconn.client_len = sizeof(bconn.client);
        bconn.serv_len = sizeof(bconn.serv);
        memset(&bconn.serv, 0, bconn.serv_len);
        bconn.serv.sin_family = AF_INET;
        bconn.serv.sin_addr.s_addr = htonl(INADDR_ANY);
        bconn.serv.sin_port = htons(port);
        rv = bind(bconn.fd,
                  (struct sockaddr *)&bconn.serv,
                  bconn.serv_len);
        if (rv == -1) {
                perror("bind");
                return -1;
        }

        for (;;) {
                rv = tftpd_listen(&bconn);
                if (rv == -1) {
                        fprintf(stderr, "tftpd_listen() failed\n");
                }
        }

        rv = close(bconn.fd);
        if (rv == -1) {
                perror("close"); 
                return -1;
        }

        return 0;
}

int main(int argc, char **argv)
{
        int rv;
        unsigned short port;

        if (argc < 2) {
                fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        if ((port = atoi(argv[1])) <= 0) {
                fprintf(stderr, "%s: bad port\n", argv[0]); 
                exit(EXIT_FAILURE);
        }

        rv = tftpd_serve(port);
        if (rv == -1) {
                fprintf(stderr, "tftpd_serve() failed\n");
                exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
}

