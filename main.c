#include "car.h"
#include "iobyte.h"
#include "server.h"
#include "itime.h"

#include <stdio.h>
#include <memory.h>
#include <signal.h>

#define OPT_FORWARD   0x1
#define OPT_BACKWARD  0x2
#define OPT_STOP      0x3

struct us_loop_t *loop_;
struct us_socket_context_t *socket_context_;
struct us_listen_socket_t *listen_socket_;
struct us_timer_t *timer_;

void on_weakup(struct us_loop_t* loop) { }
void on_pre(struct us_loop_t* loop) { }
void on_post(struct us_loop_t* loop) { }

void on_timer(struct us_timer_t* timer);

void signal_handler(int sig) {
    car_release();
    us_listen_socket_close(SSL, listen_socket_);
}

int main() {
    printf("============= RaspberryCar =============\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    car_init();

    loop_ = us_create_loop(NULL,
            on_weakup, on_pre, on_post, 0);

//    timer_ = us_create_timer(loop_, 1, 0);
//    us_timer_set(timer_, on_timer, 1000, 100);

    socket_context_ = server_init(loop_);

    /* Start accepting sockets */
    listen_socket_ = server_listen(socket_context_, 9999);

    if (listen_socket_) {
        printf("Starting on port 9999...\n");
        us_loop_run(loop_);
    } else {
        printf("Failed to start!\n");
    }

    us_loop_free(loop_);
    printf("application exit!\n");
    return 0;
}

void on_option(struct option_head* head, char* body) {
    //printf("opt: %d\n", head->option_id_);
    switch (head->option_id_) {
        case OPT_FORWARD: {
            struct ibstream *ibs = ibstream_from_buf(body, head->body_len_);
            uint32_t ls = (uint32_t)ibstream_read_int16(ibs);
            uint32_t rs = (uint32_t)ibstream_read_int16(ibs);
            car_forward(ls, rs);
            break;
        }
        case OPT_BACKWARD: {
            struct ibstream *ibs = ibstream_from_buf(body, head->body_len_);
            uint32_t ls = (uint32_t)ibstream_read_int16(ibs);
            uint32_t rs = (uint32_t)ibstream_read_int16(ibs);
            car_backward(ls, rs);
            break;
        }
        case OPT_STOP: {
            car_stop();
            break;
        }
    }
}

//void on_timer(struct us_timer_t *timer) {
//    if (!capture_)
//        return;
//
//    IplImage* frame = cvQueryFrame(capture_);
//    if (frame) {
//        printf("w:%d, h:%d, size: %d\n", frame->width, frame->height, frame->width*frame->height);
//    }
//}