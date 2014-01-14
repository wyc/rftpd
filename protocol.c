#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "tftpd.h"
#include "session.h"
#include "protocol.h"

int is_rrq(const char *buf, size_t len)
{
        uint16_t opcode;
        size_t i;
        int zc; /* zero count */

        if (len < 6) {
                return 0;
        } else if (buf[len - 1] != '\0') {
                return 0;
        }
        
        opcode = ntohs(*((uint16_t *)&buf[0]));
        if (opcode != OP_RRQ) {
                return 0;
        }

        for (i = 4, zc = 0; i < len; i++) {
                if (buf[i] == '\0') {
                        zc++;
                }
        }
        if (zc != 2) {
                return 0;
        }

        return 1;
}

int is_ack(const char *buf, size_t len) {
        uint16_t opcode;
        if (len != 4) {
                return 0;
        }

        opcode = ntohs(*((uint16_t *)&buf[0]));
        if (opcode != OP_ACK) {
                return 0;
        }

        return 1;
}

int setup_reply(tftpd_conn *conn)
{
        int rv, ec;
        uint16_t port;
        conn->finished = 0;
        conn->fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (conn->fd == -1) {
                perror("socket");
                return -1;
        }

        /* we assume we are passed a populated
         * conn->client_len and conn->client */
        conn->serv_len = sizeof(conn->serv);
        memset(&conn->serv, 0, conn->serv_len);
        conn->serv.sin_family = AF_INET;
        conn->serv.sin_addr.s_addr = htonl(INADDR_ANY);

        /* dynamic ports 49,152 to 65,535 = 0xC000 to 0xFFFF */
        ec = 0;
        do {
                port = 0xC000 + rand() % 0x3FFF;
                conn->serv.sin_port = htons(port);
                fprintf(stderr, "binding to port %d\n", port);
                rv = bind(conn->fd,
                          (struct sockaddr *)&conn->serv,
                          conn->serv_len);
                if (rv == -1) {
                        ec++;
                        continue;
                }
                break;
        } while (ec < 3);
        if (ec >= 3) {
                perror("bind (3x)");
                return -1;
        }

        return 0;
}

void teardown_reply(tftpd_conn *conn)
{
        int rv;

        rv = close(conn->fd);
        if (rv == -1) {
                perror("close");
        }
}

void teardown_rq(tftpd_conn *conn)
{
        int rv;

        rv = fclose(conn->fh);
        if (rv == -1) {
                perror("fclose");
        }

        teardown_reply(conn);
}

int send_block(tftpd_conn *conn)
{
        char buf[BLOCK_LEN + 4];
        ssize_t n;
        size_t data_len, i;
        uint16_t opcode, block;

        n = fread(&buf[4], 1, BLOCK_LEN, conn->fh);
        if (n == 0 && ferror(conn->fh) != 0) {
                fprintf(stderr, "ferror() failed\n");
                return -1;
        } else if (n != BLOCK_LEN) {
                conn->finished = 1;
        }
        opcode = htons(OP_DATA);
        block = htons(conn->block);
        memcpy(&buf[0], &opcode, 2);
        memcpy(&buf[2], &block, 2);
        data_len = n + 4;

        n = sendto(conn->fd, buf, data_len, 0,
                   (struct sockaddr *)&conn->client,
                   conn->client_len);
        if (n == -1) {
                perror("sendto");
                return -1;
        }

        puts("---SRT REPLY---");
        printf("data len: %d\n", (int) data_len);
        for (i = 0; i < data_len; i++) {
                printf("%x ", buf[i]);
        }
        puts("\n---END REPLY---");

        return 0;
}

int handle_rrq(tftpd_conn *conn, const char *buf)
{
        int rv;
        const char *pathname, *mode;
        struct stat st;
        conn->opcode = ntohs(*((uint16_t *)&buf[0]));
        conn->block = 1;

        pathname = &buf[2];
        mode = strchr(pathname, '\0') + 1;
        mode = mode; /* we don't use mode yet */

        rv = stat(pathname, &st);
        if (rv == -1) {
                perror("stat");
                rv = send_error(conn, 1, "file not found");
                if (rv == -1) {
                        fprintf(stderr, "send_error() failed\n");
                }
                return -1;
        } else if (!(st.st_mode & S_IFREG)) {
                rv = send_error(conn, 1, "requested file is a directory");
                if (rv == -1) {
                        fprintf(stderr, "send_error() failed\n");
                }
                return -1;
        }

        conn->fh = fopen(pathname, "r");
        if (!conn->fh) {
                perror("fopen");
                return -1;
        }
        
        rv = setup_reply(conn);
        if (rv == -1) {
                fprintf(stderr, "setup_reply() failed\n");
                return -1;
        }

        return 0;
}


int handle_ack(tftpd_conn *conn, const char *buf) {
        int rv;
        uint16_t block;
        if (!conn->fh) {
                fprintf(stderr, "handle_ack(): no file handle\n");
                return -1;
        }

        block = ntohs(*((uint16_t *)&buf[2]));
        if (conn->block == block) {
                conn->block++;
        }

        rv = send_block(conn);
        if (rv == -1) {
                fprintf(stderr, "send_block() failed\n");
                return -1;
        }

        return 0;
}

int send_error(const tftpd_conn *conn, uint16_t code, const char *msg)
{
        char buf[ERROR_LEN + 4];
        ssize_t n;
        size_t len;
        uint16_t opcode;

        opcode = htons(OP_ERROR);
        code = htons(code);

        memcpy(&buf[0], &opcode, 2);
        memcpy(&buf[2], &code, 2);

        strncpy(&buf[4], msg, ERROR_LEN);
        len = 4 + strlen(msg) + 1;

        n = sendto(conn->fd, buf, len, 0,
                   (struct sockaddr *)&conn->client,
                   conn->client_len);
        if (n != len) {
                perror("sendto");
                return -1;
        }
        return 0;
}

