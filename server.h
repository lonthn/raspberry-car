//
// Created by luo-zeqi on 2022/10/18.
//

#ifndef RASPBERRY_CAR_TERMINAL_SERVER_H
#define RASPBERRY_CAR_TERMINAL_SERVER_H

#include "package.h"

struct server_context {
    int listen_fd_;
    int accepted_fd_;

    int      active_;

    char    *sendbuf_;
    uint32_t sendbuf_capacity_;
    uint32_t sendlen_;
    char    *recvbuf_;
    uint32_t recvlen_;

    package_cb package_cb_;
};

struct server_context *server_context_create(int port, package_cb pkg_cb);
int server_context_send(struct server_context *ctx, struct package *pkg);
int server_context_poll(struct server_context *ctx, int timeout);
void server_context_release(struct server_context *ctx);

#endif //RASPBERRY_CAR_TERMINAL_SERVER_H
