#include "car.h"
#include "iobyte.h"
#include "server.h"

#include <stdio.h>
#include <memory.h>
#include <signal.h>

#define PKGID_FORWARD      0x01
#define PKGID_BACKWARD     0x02
#define PKGID_STOP         0x03
#define PKGID_OPEN_CAMERA  0x04
#define PKGID_CLOSE_CAMERA 0x05

struct server_context *sctx_;

int      poll_timeout_;

///
int      camera_flag_;
int      fps_;
uint64_t time_;
uint64_t next_time_;

void package_proc(struct package *pkg);
void camera_proc();
void signal_handler(int sig);

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("============= Raspberry Car =============\n");

    car_init();

    sctx_ = server_context_create(5760, package_proc);
    if (sctx_ == NULL) {
        printf("Failed to start!\n");
        goto over;
    }

    /* Start accepting sockets */
    printf("Starting on port 9999...\n");

    camera_flag_ = 0;
    poll_timeout_ = 1000;
    while (sctx_->active_) {
        if (server_context_poll(sctx_, poll_timeout_) != 0)
            break;

        if (camera_flag_) {
            camera_proc();
        }
    }

    over:
    car_release();
    if (sctx_ != NULL) server_context_release(sctx_);

    printf("Over\n");
    return 0;
}

void signal_handler(int sig) {
    car_release();
}

void package_proc(struct package* pkg) {
    struct ibstream *ibs = ibstream_from_buf(mini_package_body(pkg), pkg->body_len_);

    switch (pkg->pkg_id_) {
        case PKGID_FORWARD: {
            uint32_t ls = (uint32_t)ibstream_read_int16(ibs);
            uint32_t rs = (uint32_t)ibstream_read_int16(ibs);
            car_forward(ls, rs);
            break;
        }
        case PKGID_BACKWARD: {
            uint32_t ls = (uint32_t)ibstream_read_int16(ibs);
            uint32_t rs = (uint32_t)ibstream_read_int16(ibs);
            car_backward(ls, rs);
            break;
        }
        case PKGID_STOP: {
            car_stop();
            break;
        }
        case PKGID_OPEN_CAMERA: {
            // TODO:
            camera_flag_ = 1;
            break;
        }
        case PKGID_CLOSE_CAMERA: {
            // TODO:
            camera_flag_ = 0;
            break;
        }
    }

    ibstream_release(ibs);
}