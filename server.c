//
// Created by luo-zeqi on 2022/10/18.
//

#include "server.h"
#include "select.h"

#include <sys/socket.h>
#include <sys/types.h>
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

int sctx__on_read(void *ctx, int fd, int sel_fn) {
    int nread;
    struct package *pkg;
    int pkg_len;

    char *buf;
    int   len;
    struct server_context *sctx = (struct server_context *) ctx;

    read:
    buf = sctx->recvbuf_;
    nread = recv(sctx->accepted_fd_, buf + sctx->recvlen_,
                 SCTX_RECVBUF_CAPACITY - sctx->recvlen_, 0);
    if (nread < 0) {
        if (nread == EAGAIN) {
            return 0;
        }
        printf("ServerContext: recv error: %d\n", errno);
        return 0;
    }
    if (nread == 0) {
        goto close;
    }

    len = (int) (sctx->recvlen_ + nread);
    while (1) {
        if (len < MINI_PACKAGE_HEAD_LEN) {
            if (len != 0) {
                memcpy(sctx->recvbuf_, buf, len);
                sctx->recvlen_ = len;
                return 0;
            }
            sctx->recvlen_ = 0;
            return 0;
        }

        pkg = (struct package *) buf;
        if (pkg->body_len_ > SCTX_MAX_PKG_BODY_LEN) {
            goto close;
        }
        pkg_len = mini_package_len(pkg);
        if (pkg_len > len) {
            memcpy(sctx->recvbuf_, buf, len);
            sctx->recvlen_ = len;
            goto read;
        }

        sctx->package_cb_(pkg);
        buf += pkg_len;
        len -= pkg_len;
    }

    close:
    close(sctx->accepted_fd_);
    sctx->accepted_fd_ = 0;
    printf("ServerContext: connection(%s) is close.\n", "");
    return 1; // remove sel_readable
}

int sctx__on_write(void *ctx, int fd, int sel_fn) {
    int nw;
    int remain;
    struct server_context *sctx = (struct server_context *) ctx;

    nw = 0;
    if (sctx->sendlen_ != 0) {
        nw = send(sctx->accepted_fd_, sctx->sendbuf_, sctx->sendlen_, 0);
        if (nw <= 0) {
            printf("SocketContext: write error: %d\n", errno);
            return 0;
        }
        if (nw == sctx->sendlen_) {
            sctx->sendlen_ = 0;
            return 1;
        }
    }

    remain = (int) (sctx->sendlen_ - nw);
    memcpy(sctx->sendbuf_ + sctx->sendlen_, sctx->sendbuf_ + nw, remain);
    sctx->sendlen_ = remain;
    return 0;
}

int sctx__on_accpect(void *ctx, int fd, int sel_fn) {
    int res;
    struct sockaddr_in cli_addr;
    socklen_t          addr_len;
    struct server_context *sctx = (struct server_context *) ctx;

    if (sctx->accepted_fd_ == 0) {
        res = accept(sctx->listen_fd_, (struct sockaddr *) &cli_addr, &addr_len);
        if (res <= 0) {
            printf("ServerContext: accept error: %d.\n", errno);
            return 0;
        }
        sctx->accepted_fd_ = res;

        selecter_add(sctx->accepted_fd_, SEL_READABLE, sctx__on_read, ctx);
        printf("ServerContext: accepted new connection: %s.\n", "");
    }
    return 1;
}

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
    selecter_add(ctx->listen_fd_, SEL_READABLE, sctx__on_accpect, ctx);
    return ctx;
}

int server_context_send(struct server_context *ctx, struct package *pkg) {
    if (ctx->accepted_fd_ == 0)
        return -1;

    char *buf = (char *) pkg;
    int len = package_len(pkg);
    if (ctx->sendlen_ == 0) {
        int nwrite = send(ctx->accepted_fd_, buf, len, 0);
        if (nwrite < 0) return -1;
        if (nwrite == len) return len;

        buf += nwrite;
        len -= nwrite;
    }

    if ((ctx->sendbuf_capacity_ - ctx->sendlen_) < len) {
        ctx->sendbuf_ = realloc(ctx->sendbuf_,
                                ctx->sendbuf_capacity_ + len);
        assert(ctx->sendbuf_);
        ctx->sendbuf_capacity_ += len;
    }

    memcpy(ctx->sendbuf_ + ctx->sendlen_, buf, len);
    ctx->sendlen_ += len;
    selecter_add(ctx->accepted_fd_, SEL_WRITEABLE, sctx__on_write, ctx);
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