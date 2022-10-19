//
// Created by luo-zeqi on 2022/10/18.
//

#include "server.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#define SCTX_SENDBUF_CAPACITY 1024*1024
#define SCTX_RECVBUF_CAPACITY 1024

#define SCTX_MAX_PKG_BODY_LEN 127

struct server_context *server_context_create(int port, package_cb pkg_cb) {
    int res;
    int listen_fd;
    struct sockaddr_in addr;
    struct server_context *ctx;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd <= 0) {
        printf("ServerContext: create socket error: %d.\n", errno);
        return NULL;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    res = bind(listen_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    if (res != 0) {
        printf("ServerContext: bind error: %d.\n", errno);
        return NULL;
    }
    res = listen(listen_fd, 1);
    if (res != 0) {
        printf("ServerContext: listen error: %d.\n", errno);
        return NULL;
    }

    ctx = (struct server_context *) malloc(sizeof(struct server_context));
    assert(ctx);
    memset(ctx, 0, sizeof(struct server_context));
    ctx->listen_fd_ = listen_fd;
    ctx->package_cb_ = pkg_cb;
    ctx->sendbuf_ = malloc(SCTX_SENDBUF_CAPACITY);
    ctx->recvbuf_ = malloc(SCTX_RECVBUF_CAPACITY);
    ctx->sendbuf_capacity_ = SCTX_SENDBUF_CAPACITY;
    assert(ctx->sendbuf_);
    assert(ctx->recvbuf_);

    ctx->active_ = 1;

    return ctx;
}

int server_context_send(struct server_context *ctx, struct package *pkg) {
    if (ctx->accepted_fd_ == 0)
        return -1;

    char *buf = (char *) pkg;
    int len = package_len(pkg);
    if (ctx->send_len_ == 0) {
        int nwrite = send(ctx->accepted_fd_, buf, len, 0);
        if (nwrite < 0) return -1;
        if (nwrite == len) return len;

        buf += nwrite;
        len -= nwrite;
    }

    if ((ctx->sendbuf_capacity_ - ctx->send_len_) < len) {
        ctx->sendbuf_ = realloc(ctx->sendbuf_,
                                ctx->sendbuf_capacity_ + len);
        assert(ctx->sendbuf_);
        ctx->sendbuf_capacity_ += len;
    }

    memcpy(ctx->sendbuf_ + ctx->send_len_, buf, len);
    ctx->send_len_ += len;
    return 0;
}

int __sctx_on_read(struct server_context *ctx) {
    int nread;
    struct package *pkg;
    int pkg_len;
    char *buf;

    read:
    buf = ctx->recvbuf_;
    nread = recv(ctx->accepted_fd_, buf + ctx->recv_len_,
                 SCTX_RECVBUF_CAPACITY - ctx->recv_len_, 0);
    if (nread < 0) {
        if (nread == EAGAIN) {
            return 0;
        }
        printf("ServerContext: recv error: %d\n", errno);
        return nread;
    }
    if (nread == 0) {
        close(ctx->accepted_fd_);
        ctx->accepted_fd_ = 0;
        printf("ServerContext: connection(%s) is close\n.", "");
        return -1;
    }

    while (1) {
        if (nread < MINI_PACKAGE_HEAD_LEN) {
            memcpy(ctx->recvbuf_, buf, nread);
            ctx->recv_len_ += nread;
            return 0;
        }

        pkg = (struct package *) buf;
        if (pkg->body_len_ > SCTX_MAX_PKG_BODY_LEN) {
            close(ctx->accepted_fd_);
            ctx->accepted_fd_ = 0;
            printf("ServerContext: connection(%s) is close\n.", "");
            return -1;
        }
        pkg_len = mini_package_len(pkg);
        if (pkg_len > nread) {
            memcpy(ctx->recvbuf_, buf, nread);
            ctx->recv_len_ += nread;
            goto read;
        }

        ctx->package_cb_(pkg);
        buf += pkg_len;
        nread -= pkg_len;
    }

    return 0;
}

int __sctx_on_write(struct server_context *ctx) {
    int nw;
    int remain;

    nw = 0;
    if (ctx->send_len_ != 0) {
        nw = send(ctx->accepted_fd_, ctx->sendbuf_, ctx->send_len_, 0);
        if (nw <= 0) return -1;
        if (nw == ctx->send_len_) {
            ctx->send_len_ = 0;
            return nw;
        }
    }

    remain = ctx->send_len_ - nw;
    memcpy(ctx->sendbuf_ + ctx->send_len_, ctx->sendbuf_ + nw, remain);
    ctx->send_len_ = remain;
    return 0;
}

int server_context_poll(struct server_context *ctx, int timeout) {
    struct timeval tv;
    fd_set rfs, wfs;
    int nfds;
    int res;
    struct sockaddr_in cli_addr;
    socklen_t addr_len;

    if (ctx->active_ == 0) {
        return -1;
    }

    if (ctx->accepted_fd_ != 0) {
        FD_SET(ctx->accepted_fd_, &rfs);
        FD_SET(ctx->accepted_fd_, &wfs);
        nfds = ctx->accepted_fd_;
    } else {
        FD_SET(ctx->listen_fd_, &rfs);
        nfds = ctx->listen_fd_;
    }
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    res = select(nfds+1, &rfs, &wfs, NULL, timeout < 0 ? NULL : &tv);
    if (res < 0) {
        printf("ServerContext: select error: %d.\n", errno);
        return res;
    }
    if (res == 0)
        return 0;

    if (ctx->accepted_fd_ == 0) {
        res = accept(ctx->listen_fd_, (struct sockaddr *) &cli_addr, &addr_len);
        if (res <= 0) {
            printf("ServerContext: accept error: %d.\n", errno);
            return res;
        }
        ctx->accepted_fd_ = res;
        printf("ServerContext: accepted new connection: %s.\n", "");
        return 1;
    }

    if (FD_ISSET(ctx->accepted_fd_, &rfs)) {
        return __sctx_on_read(ctx);
    }

    if (FD_ISSET(ctx->accepted_fd_, &wfs)) {
        return __sctx_on_write(ctx);
    }

    return 0;
}

void server_context_release(struct server_context *ctx) {
    if (ctx->accepted_fd_) {
        close(ctx->accepted_fd_);
    }
    close(ctx->listen_fd_);

    free(ctx->sendbuf_);
    free(ctx->recvbuf_);
    free(ctx);
}