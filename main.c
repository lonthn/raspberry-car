#include "car.h"
#include "camera.h"
#include "iobyte.h"
#include "server.h"
#include "select.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/socket.h>
#include <errno.h>

#define PKGID_FORWARD      0x01
#define PKGID_BACKWARD     0x02
#define PKGID_STOP         0x03
#define PKGID_OPEN_CAMERA  0x04
#define PKGID_CLOSE_CAMERA 0x05

struct server_context *sctx_;

int      poll_timeout_;

///
static int      camera_flag_;
static int      fps_;
static uint64_t time_;
static uint64_t next_time_;
static uint32_t fw_, fh_;
static uint8_t *frame_ = 0;
static int      frame_channel_[2];

void package_proc(struct package *pkg);
int camera_proc(void *ctx, int fd);
void signal_handler(int sig);

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("============= Raspberry Car =============\n");

    car_init();

    sctx_ = server_context_create(5760, package_proc);
    if (sctx_ == NULL) {
        printf("Failed to start!\n");
        goto exit;
    }

    printf("Starting on port 5760...\n");

    int res = socketpair(AF_UNIX, SOCK_STREAM,
                         0, frame_channel_);
    if (res != 0) {
        printf("Create frame channel faield[%d:%s]\n",
               errno, strerror(errno));
        goto exit;
    }
    //fps_ = 24;
    fw_ = 320;
    fh_ = 240;
    camera_open(fw_, fh_, frame_channel_[0]);

    selecter_add(frame_channel_[1], SEL_READABLE, camera_proc, NULL);

    // Main loop
    poll_timeout_ = 1000;
    while (sctx_->active_) {
        selecter_poll(poll_timeout_);
    }

    exit:
    //camera_close();
    car_release();
    if (sctx_ != NULL) {
        server_context_release(sctx_);
    }

    printf("Exit app!\n");
    return 0;
}

void signal_handler(int sig) {
    sctx_->active_ = 0;
    car_release();
}

void package_proc(struct package* pkg) {
    struct ibstream *ibs = ibstream_from_buf(package_body(pkg), pkg->body_len_);
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

int camera_proc(void *ctx, int fd) {
    int rn, wn;
    uint8_t *frame;
    struct package *pkg;

    static struct timeval last = {};
    struct timeval curr;

    rn = recv(fd, &frame, sizeof(frame), 0);
    if (rn <= 0) {
        printf("Read frame address error[%d,%s]\n",
               errno, strerror(errno));
        return 1;
    }

    if (rn != sizeof(frame)) {
        printf("Frame address is spill.\n");
        return 0;
    }

    ///////////////
//    gettimeofday(&curr, NULL);
//    int diff = (curr.tv_sec*1000000+curr.tv_usec)
//            - (last.tv_sec*1000000+last.tv_usec);
//    printf("%d\n", diff);
//    last = curr;
    ///////////////

    if (!sctx_->active_ || sctx_->accepted_fd_ <= 0) {
        return 0;
    }

    pkg = (struct package *) (frame - sizeof(struct package));
    pkg->pkg_id_ = 127;
    wn = server_context_send(sctx_, pkg);
    if (wn < 0)
        printf("Send error(%d)\n", errno);

    FREE_FRM(frame);

    return 0;
}