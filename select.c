//
// Created by luo-zeqi on 2022/10/28.
//

#include "select.h"

#include <sys/select.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

typedef struct poller {
    int fd_;
    uint32_t sel_fn_;
    trigger_cb_t read_cb_;
    trigger_cb_t write_cb_;
    void *ctx_;

    struct poller *next_;
} poller_t;

poller_t *head_ = NULL;

void selecter_add(int fd, int fn, trigger_cb_t cb, void *ctx) {
    poller_t **sel;

    sel = &head_;
    while (*sel) {
        if ((*sel)->fd_ == fd)
            break;

        sel = &((*sel)->next_);
    }

    if (*sel == NULL) {
        *sel = (poller_t *) malloc(sizeof(poller_t));
        (*sel)->fd_ = fd;
        (*sel)->sel_fn_ = 0;
        (*sel)->ctx_ = ctx;
        (*sel)->next_ = NULL;
    }

    assert(sel);

    (*sel)->sel_fn_ |= fn;
    if (fn == SEL_READABLE)
        (*sel)->read_cb_ = cb;
    else
        (*sel)->write_cb_ = cb;
}

//void selecter_set(int fd, int fn, trigger_cb_t cb, void *ctx) {
//    poller_t *sel;
//
//    sel = head_;
//    while (sel) {
//        if (sel->fd_ == fd) {
//            sel->sel_fn_ = fn;
//            if (fn == SEL_READABLE)
//                sel->read_cb_ = cb;
//            else
//                sel->write_cb_ = cb;
//            sel->ctx_ = ctx;
//            return;
//        }
//        sel = sel->next_;
//    }
//}

void selecter_remove(int fd, int fn) {
    poller_t **curr;

    curr = &head_;
    while (*curr) {
        if ((*curr)->fd_ != fd) {
            curr = &(*curr)->next_;
            continue;
        }

        if (((*curr)->sel_fn_ & fn) == fn)
            (*curr)->sel_fn_ ^= fn;

//        if ((*curr)->sel_fn_ == 0) {
//            poller_t *tmp;
//
//            tmp = *curr;
//            *curr = (*curr)->next_;
//            free(tmp);
//        }
        break;
    }
}

int selecter_poll(int timeout) {
    struct timeval tv;
    fd_set rfs, wfs;
    int nfds;
    int res;
    poller_t **sel, *rm;

    nfds = 0;
    sel = &head_;
    while (*sel) {
        if ((*sel)->sel_fn_ & SEL_READABLE)
            FD_SET((*sel)->fd_, &rfs);

        if ((*sel)->sel_fn_ & SEL_WRITEABLE)
            FD_SET((*sel)->fd_, &wfs);

        if ((*sel)->fd_ > nfds) nfds = (*sel)->fd_;
        sel = &(*sel)->next_;
    }

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (timeout > 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
    }
    res = select(nfds+1, &rfs, &wfs, NULL, timeout < 0 ? NULL : &tv);
    if (res < 0) {
        printf("select error(%d:%s).\n",
               errno, strerror(errno));
        return res;
    }
    if (res == 0)
        return 0;

    sel = &head_;
    while (*sel) {
        if (((*sel)->sel_fn_ & SEL_READABLE) && FD_ISSET((*sel)->fd_, &rfs)) {
            if ((*sel)->read_cb_((*sel)->ctx_, (*sel)->fd_)) {
                (*sel)->sel_fn_ ^= SEL_READABLE;
            }
        }
        if (((*sel)->sel_fn_ & SEL_WRITEABLE) && FD_ISSET((*sel)->fd_, &wfs)) {
            if ((*sel)->write_cb_((*sel)->ctx_, (*sel)->fd_)) {
                (*sel)->sel_fn_ ^= SEL_WRITEABLE;
            }
        }

        if ((*sel)->sel_fn_ == 0) {
            rm = *sel;
            sel = &(*sel)->next_;
            free(rm);
            continue;
        }

        sel = &(*sel)->next_;
    }
    return 0;
}