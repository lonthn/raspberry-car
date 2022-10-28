//
// Created by luo-zeqi on 2022/10/28.
//

#include "select.h"

#include <sys/select.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>

struct selecter {
    int fd_;
    int sel_fn_;
    trigger_cb_t read_cb_;
    trigger_cb_t write_cb_;
    void *ctx_;

    struct selecter *next_;
};

struct selecter *head_ = 0;
struct selecter *tail_ = 0;

void selecter_add(int fd, int fn, trigger_cb_t cb, void *ctx) {
    struct selecter *sel;

    sel = head_;
    while (sel) {
        if (sel->fd_ == fd) {
            sel->sel_fn_ |= fn;
            return;
        }
        sel = sel->next_;
    }

    sel = (struct selecter *) malloc(sizeof(struct selecter));
    assert(sel);

    sel->fd_ = fd;
    sel->sel_fn_ = fn;
    if (fn == SEL_READABLE)
        sel->read_cb_ = cb;
    else
        sel->write_cb_ = cb;
    sel->ctx_ = ctx;
    sel->next_ = 0;
    if (tail_) {
        tail_->next_ = sel;
        tail_ = sel;
    } else {
        head_ = sel;
        tail_ = sel;
    }
}

void selecter_set(int fd, int fn, trigger_cb_t cb, void *ctx) {
    struct selecter *sel;

    sel = head_;
    while (sel) {
        if (sel->fd_ == fd) {
            sel->sel_fn_ = fn;
            if (fn == SEL_READABLE)
                sel->read_cb_ = cb;
            else
                sel->write_cb_ = cb;
            sel->ctx_ = ctx;
            return;
        }
        sel = sel->next_;
    }
}

void selecter_remove(int fd, int fn) {
    struct selecter *prev, *curr;

    prev = 0;
    curr = head_;
    while (curr) {
        if (curr->fd_ == fd) {
            curr->sel_fn_ |= ~fn;
            if (curr->sel_fn_ == 0) {
                if (prev == 0) {
                    head_ = 0;
                    tail_ = 0;
                } else {
                    prev->next_ = curr->next_;
                    if (tail_ == curr) {
                        tail_ = prev;
                    }
                }
                free(curr);
                return;
            }
        }
        prev = curr;
        curr = curr->next_;
    }
}

int selecter_poll(int timeout) {
    struct timeval tv;
    fd_set rfs, wfs;
    int nfds;
    int res;
    struct selecter *prev, *curr;

    nfds = 0;
    curr = head_;
    while (curr) {
        if (head_->sel_fn_ & SEL_READABLE) {
            FD_SET(curr->fd_, &rfs);
            if (curr->fd_ > nfds) nfds = curr->fd_;
        }
        if (head_->sel_fn_ & SEL_WRITEABLE) {
            FD_SET(curr->fd_, &wfs);
            if (curr->fd_ > nfds) nfds = curr->fd_;
        }
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

    prev = 0;
    curr = head_;
    while (curr) {
        if (curr->sel_fn_ & SEL_READABLE && FD_ISSET(curr->fd_, &rfs)) {
            if (curr->read_cb_(curr->ctx_, curr->fd_)) {
                curr->sel_fn_ |= ~SEL_READABLE;
            }
        }
        if (curr->sel_fn_ & SEL_WRITEABLE && FD_ISSET(curr->fd_, &wfs)) {
            if (curr->write_cb_(curr->ctx_, curr->fd_)) {
                curr->sel_fn_ |= ~SEL_WRITEABLE;
            }
        }

        if (curr->sel_fn_ == 0) {
            if (prev == 0) {
                head_ = 0;
                tail_ = 0;
            } else {
                prev->next_ = curr->next_;
                if (tail_ == curr) {
                    tail_ = prev;
                }
            }
            free(curr);
        }
        prev = curr;
        curr = curr->next_;
    }
    return 0;
}