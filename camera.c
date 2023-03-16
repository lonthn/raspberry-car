//
// This code copy from https://github.com/lincolnhard/v4l2-framebuffer/blob/master/video_capture.c
// Author: Lincoln
//

#include "camera.h"
#include "select.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <unistd.h>

#include <jpeglib/jpeglib.h>

struct mbuffer {
    char    *mem;
    uint32_t len;
};

static int         fd_;
static int         capture_ = 0;
static const char *name_ = "/dev/video0";

static struct mbuffer *mbufs_;
static int             mbuf_count_;

static uint32_t fwidth_;
static uint32_t fheight_;
static uint8_t *tmpframe_ = 0;
static int      notifier_fd_ = 0;
unsigned char  *line_buffer_ = 0;

void *camera__thread_proc(void *args) {
    enum v4l2_buf_type  type;
    struct timeval timout, last, curr;
    int frame_time;
    int frame_len;
    int sel, wn;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* Start streaming I/O */
    assert(ioctl(fd_, VIDIOC_STREAMON, &type) == 0);

    fd_set rd_fds; // for capture
    fd_set ex_fds; // for capture

    gettimeofday(&last, NULL);

    while (1) {
        FD_ZERO(&rd_fds);
        FD_SET(fd_, &rd_fds);

        FD_ZERO(&ex_fds);
        FD_SET(fd_, &ex_fds);

        timout.tv_sec = 1;
        timout.tv_usec = 0;
        sel = select(fd_ + 1, &rd_fds, NULL, &ex_fds, &timout);

        if (!capture_)
            break;

        if (sel < 0) {
            if (errno == EINTR)
                continue;
            printf("select() error(%d)\n", errno);
            break;
        } else if (sel == 0) {
            continue;
        }

        if (FD_ISSET(fd_, &ex_fds)
         || !FD_ISSET(fd_, &rd_fds)) {
            printf("select() exception!\n");
            break;
        }

        uint8_t* frame;
        frame_len = camera_frame(&frame);
        *(int*)(frame - 4) = frame_len;

        gettimeofday(&curr, NULL);
        frame_time = (curr.tv_sec*1000000+curr.tv_usec)
                - (last.tv_sec*1000000+last.tv_usec);

        if (frame_time > 33000) {
            wn = send(notifier_fd_, &frame, sizeof(frame), 0);
            if (wn < 0) {
                printf("Frame channel error(%d).\n", errno);
                FREE_FRM(frame);
                continue;
            }
            if (wn == 0) {
                printf("Camera end capture.\n");
                break;
            }

            last = curr;
        }
    }

    assert(ioctl(fd_, VIDIOC_STREAMOFF, &type) == 0);
    return 0;
}

//int camera__read() {
//    if (tmpframe_ == 0) {
//        tmpframe_ = malloc(fwidth_ * fheight_ * 3-1);
//    }
//
//    struct package *package = (struct package *) tmpframe_;
//    int size = camera_frame((uint8_t *) package_body(package));
//
//    if (sctx->accepted_fd_ <= 0) {
//        return 0;
//    }
//
//    package_set_body_len(package, (uint32_t) size);
//    server_context_send(sctx, package);
//
//    return 0;
//}


int camera_open(uint32_t width, uint32_t height, int notifier_fd) {
    struct v4l2_capability cap;
    struct v4l2_format     fmt;
    unsigned int           min;

    unsigned int           i;
    enum v4l2_buf_type     type;

    notifier_fd_ = notifier_fd;

    // Open the camera device.
    fd_ = open(name_, O_RDWR);
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
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) == -1) {
        printf("Camera set format error: %d\n", errno);
        return -1;
    }
    /* YUYV sampling 4 2 2, so bytes per pixel is 2*/
    min = fmt.fmt.pix.width * 3;
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
    /* video output requires at least two buffers,
     * one displayed and one filled by the application */
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

        mbufs_[mbuf_count_].len = buf.length; //640 * 480 * 3 = 614400
        mbufs_[mbuf_count_].mem = mmap(NULL /* start anywhere */, buf.length,
                                        PROT_READ | PROT_WRITE /* required */,
                                        MAP_SHARED /* recommended */,
                                        fd_, (__off_t) buf.m.offset);

        if (MAP_FAILED == mbufs_[mbuf_count_].mem) {
            printf("%s\n", strerror(errno));
            return -1;
        }
    }

    for (i = 0; i < mbuf_count_; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        /* enqueue an empty (capturing) or filled (output)
         * buffer in the driver's incoming queue */
        assert(ioctl(fd_, VIDIOC_QBUF, &buf) == 0);
    }

    fwidth_= width;
    fheight_ = height;

    capture_ = 1;

    /// Open the capture thread.
    pthread_t tid;
    pthread_create(&tid, NULL, camera__thread_proc, NULL);
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

    if (line_buffer_)
        free(line_buffer_);

    close(fd_);
    fd_ = -1;
}

#define OUTPUT_BUF_SIZE  4096

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    JOCTET * buffer;    /* start of buffer */

    //unsigned char *outbuffer;
    //int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;

typedef mjpg_destination_mgr * mjpg_dest_ptr;

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
            JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
Description.: called whenever local jpeg buffer fills up
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
Description.: called by jpeg_finish_compress after all data has been written.
              Usually needs to flush buffer.
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

int camera__compress_to_jpeg(const uint8_t *bgr, unsigned char *jpeg, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int z;
    int row;
    static int written;

    mjpg_dest_ptr dest;

    row = (int) (fwidth_ * 3);
//    if (line_buffer_ == 0) {
//        line_buffer_ = calloc(row, 1);
//    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if(cinfo.dest == NULL) {
        cinfo.dest = (struct jpeg_destination_mgr *)(*cinfo.mem->alloc_small)
                ((j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr) cinfo.dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer_cursor = jpeg;
    dest->written = &written;

    cinfo.image_width = fwidth_;
    cinfo.image_height = fheight_;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    z = 0;
    while(cinfo.next_scanline < fheight_) {
        int x;
        const unsigned char *src = bgr;
//        unsigned char *ptr = line_buffer_;

//        for(x = 0; x < fwidth_; x++) {
//            ptr[0] = src[2];
//            ptr[1] = src[1];
//            ptr[2] = src[0];
//            ptr += 3;
//            src += 3;
//        }

        row_pointer[0] = (JSAMPROW) src;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
        bgr += row;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    //free(line_buffer);

    return (written);
}

int camera_frame(uint8_t** frame) {
    struct v4l2_buffer buf;
    unsigned char* im_from_cam;
    int wn;
    int size;

    size = (int) (fwidth_ * fheight_ * 3);

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

    im_from_cam = (unsigned char*)mbufs_[buf.index].mem;
    if (!tmpframe_) {
        tmpframe_ = malloc(size);
    }
    //memcpy(tmpframe_, im_from_cam, size);

    wn = camera__compress_to_jpeg(im_from_cam, tmpframe_, 90);

    /* queue-in buffer */
    assert(ioctl(fd_, VIDIOC_QBUF, &buf) == 0);

    *frame = (uint8_t *) ALLOC_FRM(wn);

    memcpy(*frame, tmpframe_, wn);
    return wn;
}