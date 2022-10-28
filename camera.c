//
// This code copy from https://github.com/lincolnhard/v4l2-framebuffer/blob/master/video_capture.c
// Author: Lincoln
//

#include "camera.h"
#include "select.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <unistd.h>

struct mbuffer {
    char    *mem;
    uint32_t len;
};

static int         fd_;
static const char *name_ = "/dev/video0";

static struct mbuffer *mbufs_;
static int             mbuf_count_;

static uint32_t fwidth_;
static uint32_t fheight_;

int camera__read(void *ctx, int fd) {
    struct server_context *sctx = (struct server_context *) ctx;
    if (!sctx->active_)
        return 1;

    // TODO: Read frame and upload to server
    return 0;
}

int camera_open(struct server_context *sctx, uint32_t width, uint32_t height) {
    struct v4l2_capability cap;
    struct v4l2_format     fmt;
    unsigned int           min;

    unsigned int           i;
    enum v4l2_buf_type     type;

    // Open the camera device.
    fd_ = open(name_, O_RDONLY);
    if (fd_ <= 0) {
        printf("Camera open error: %d\n", errno);
        return fd_;
    }

    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) != 0) {
        if (EINVAL == errno) {
            printf("Camera %s is no V4L2 device\n", name_);
        } else {
            printf("Camera open error: %d\n", errno);
        }
        return -1;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("Camera %s is no video capture device\n", name_);
        return -1;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("Camera %s does not support streaming i/o\n", name_);
        return -1;
    }

    memset(&fmt, 0, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) == -1) {
        printf("Camera set format error: %d\n", errno);
        return -1;
    }
    /* YUYV sampling 4 2 2, so bytes per pixel is 2*/
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min){
        fmt.fmt.pix.bytesperline = min;
    }
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min){
        fmt.fmt.pix.sizeimage = min;
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(struct v4l2_requestbuffers));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    /* Initiate Memory Mapping */
    assert(ioctl(fd_, VIDIOC_REQBUFS, &req) == 0);
    /* video output requires at least two buffers, one displayed and one filled by the application */
    if (req.count < 2) {
        printf("Insufficient buffer memory on %s\n", name_);
        return -1;
    }

    mbufs_ = (struct mbuffer *) malloc(sizeof(struct mbuffer) * req.count);
    assert(mbufs_);

    for (mbuf_count_ = 0; mbuf_count_ < req.count; mbuf_count_++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = mbuf_count_;
        /* Query the status of a buffer */
        assert(ioctl(fd_, VIDIOC_QUERYBUF, &buf) == 0);

        mbufs_[mbuf_count_].len = buf.length; //640 * 480 * 2 = 614400
        mbufs_[mbuf_count_].mem = mmap(NULL /* start anywhere */, buf.length,
                                        PROT_READ | PROT_WRITE /* required */,
                                        MAP_SHARED /* recommended */, fd_, (__off_t) buf.m.offset);

        if (MAP_FAILED == mbufs_[mbuf_count_].mem) {
            return -1;
        }
    }

    for (i = 0; i < mbuf_count_; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        /* enqueue an empty (capturing) or filled (output) buffer in the driver's incoming queue */
        assert(ioctl(fd_, VIDIOC_QBUF, &buf) == 0);
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* Start streaming I/O */
    assert(ioctl(fd_, VIDIOC_STREAMON, &type) == 0);

    fwidth_= width;
    fheight_ = height;

    selecter_add(fd_, SEL_READABLE, camera__read, sctx);
    return 0;
}

void camera_close() {
    unsigned int i;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    assert(ioctl(fd_, VIDIOC_STREAMOFF, &type) == 0);

    for (i = 0; i < mbuf_count_; ++i) {
        munmap(mbufs_[i].mem, mbufs_[i].len);
    }
    free(mbufs_);

    close(fd_);
}

static void camera__parse_im(const unsigned char *im_yuv, unsigned char *dst,
                             uint32_t width, uint32_t height) {
    const uint32_t IM_SIZE = width * height;
    uint32_t i;
    unsigned char Y = 0, U = 0, V = 0;
    int B = 0, G = 0, R = 0;

    for (i = 0; i < IM_SIZE; ++i) {
        if (!(i & 1)) {
            U = im_yuv[2 * i + 1];
            V = im_yuv[2 * i + 3];
        }
        Y = im_yuv[2 * i];
        B = (int) (Y + 1.773 * (U - 128));
        G = (int) (Y - 0.344 * (U - 128) - (0.714 * (V - 128)));
        R = (int) (Y + 1.403 * (V - 128));
        dst[3*i] = B > UINT8_MAX ? UINT8_MAX : B;
        dst[3*i+1] = G > UINT8_MAX ? UINT8_MAX : G;
        dst[3*i+2] = R > UINT8_MAX ? UINT8_MAX : R;
    }
}

int camera_frame(uint8_t* frame) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    /* dequeue from buffer */
    if(ioctl(fd_, VIDIOC_DQBUF, &buf) == -1) {
        if (errno == EAGAIN) {
            return 0;
        }
        printf("Camera get frame error: %d\n", errno);
        return -1;
    }

    unsigned char* im_from_cam = (unsigned char*)mbufs_[buf.index].mem;
    camera__parse_im(im_from_cam, frame, fwidth_, fheight_);

    /* queue-in buffer */
    assert(ioctl(fd_, VIDIOC_QBUF, &buf) == 0);

    return 0;
}