#include "iobyte.h"

#include <string.h>
#include <endian.h>
#include <malloc.h>
#include <assert.h>

struct ibstream* ibstream_from_buf(const char* buf, uint32_t len) {
    struct ibstream *ibs =
            (struct ibstream *) malloc(sizeof(struct ibstream));
    ibs->buffer_ = buf;
    ibs->len_ = len;
    ibs->pos_ = 0;
    return ibs;
}

void ibstream_release(struct ibstream *ibs) {
    free(ibs);
}

int32_t ibstream_read(struct ibstream* ibs, char* buf, uint32_t len) {
    if ((ibs->pos_ + len) > ibs->len_) {
        assert(0);
        return -1;
    }
    memcpy(buf, ibs->buffer_ + ibs->pos_, len);
    ibs->pos_ += len;
}

int8_t  ibstream_read_int8(struct ibstream* ibs) {
    int8_t n;
    ibstream_read(ibs, (char *) &n, sizeof(int8_t));
    return n;
}

int16_t ibstream_read_int16(struct ibstream* ibs) {
    int16_t n;
    ibstream_read(ibs, (char *) &n, sizeof(int16_t));
    return n;
}

int32_t ibstream_read_int32(struct ibstream* ibs) {
    int32_t n;
    ibstream_read(ibs, (char *) &n, sizeof(int32_t));
    return n;
}

int64_t ibstream_read_int64(struct ibstream* ibs) {
    int64_t n;
    ibstream_read(ibs, (char *) &n, sizeof(int64_t));
    return n;
}

float ibstream_read_float32(struct ibstream* ibs) {
    float n;
    ibstream_read(ibs, (char *) &n, sizeof(float));
    return n;
}

double ibstream_read_float64(struct ibstream* ibs) {
    double n;
    ibstream_read(ibs, (char *) &n, sizeof(double));
    return n;
}

int32_t ibstream_read_sstr(struct  ibstream* ibs, char* str) {
    int8_t len = ibstream_read_int8(ibs);
    if (len == -1)
        return -1;
    return ibstream_read(ibs, str, len);
}

int32_t ibstream_read_lstr(struct  ibstream* ibs, char* str) {
    int16_t len = ibstream_read_int16(ibs);
    if (len == -1)
        return -1;
    return ibstream_read(ibs, str, len);
}


///////////////////////////////////////////


struct obstream* obstream_create(uint32_t initial_capacity, int auto_extend) {
    struct obstream *obs =
            (struct obstream *) malloc(sizeof(struct obstream));
    if (obs == NULL)
        return NULL;

    char* buf = (char *) malloc(initial_capacity);
    if (buf == NULL) {
        free(obs);
        return NULL;
    }

    obs->buffer_ = buf;
    obs->capacity_ = initial_capacity;
    obs->auto_extend_ = auto_extend;
    obs->pos_ = 0;
    return obs;
}

void obstream_destory(struct obstream* obs) {
    if (obs->buffer_)
        free(obs->buffer_);
    free(obs);
}

int obstream_write(struct obstream* obs, char* buf, uint32_t len) {
    if (obs->pos_ + len > obs->capacity_) {
        // It's not enough space, we need realloc buffer.
        if (obs->auto_extend_) {
            uint32_t new_capacity = obs->capacity_ * 2;
            if (new_capacity == UINT32_MAX || new_capacity < obs->capacity_)
                return -1;

            void* new_buf = realloc(obs->buffer_, new_capacity);
            if (new_buf == NULL)
                return -1;

            obs->buffer_ = (char *) new_buf;
            obs->capacity_ = new_capacity;
        } else {
            return -1;
        }
    }

    memcpy(obs->buffer_+obs->pos_, (void *) buf, len);
    obs->pos_ += len;
    return (int) len;
}

int obstream_write_int8(struct obstream* obs, int8_t val) {
    obstream_write(obs, (char *) &val, sizeof(int8_t));
}

int obstream_write_int16(struct obstream* obs, int16_t val) {
    obstream_write(obs, (char *) &val, sizeof(int16_t));
}

int obstream_write_int32(struct obstream* obs, int32_t val) {
    obstream_write(obs, (char *) &val, sizeof(int32_t));
}

int obstream_write_int64(struct obstream* obs, int64_t val) {
    obstream_write(obs, (char *) &val, sizeof(int64_t));
}

int obstream_write_float32(struct obstream* obs, float val) {
    obstream_write(obs, (char *) &val, sizeof(float));
}

int obstream_write_float64(struct obstream* obs, double val) {
    obstream_write(obs, (char *) &val, sizeof(double));
}

int obstream_write_sstr(struct  obstream* obs, char* str, uint8_t len) {
    obstream_write(obs, (char *) &len, sizeof(uint8_t));
    if (len == 0)
        return 0;

    return obstream_write(obs, str, (uint32_t) len);
}

int obstream_write_lstr(struct  obstream* obs, char* str, uint16_t len) {
    obstream_write(obs, (char *) &len, sizeof(uint16_t));
    if (len == 0)
        return 0;

    return obstream_write(obs, str, (uint32_t) len);
}