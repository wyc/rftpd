#ifndef _REQUESTS_H
#define _REQUESTS_H

#include "tftpd.h"
#include "session.h"

#define ERROR_LEN 128
#define BLOCK_LEN 512

int is_rrq(const char *buf, size_t len);
int is_ack(const char *buf, size_t len);

int handle_rrq(tftpd_conn *conn, const char *buf);
int handle_ack(tftpd_conn *conn, const char *buf);

int send_block(tftpd_conn *conn);
int send_error(const tftpd_conn *conn, uint16_t code, const char *msg);

int setup_reply(tftpd_conn *conn);
void teardown_reply(tftpd_conn *conn);
void teardown_rq(tftpd_conn *conn);

#endif
