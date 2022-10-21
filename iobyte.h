//
// Created by zeqi.luo on 2020/8/5.
//

#ifndef IO_BYTE_STREAM
#define IO_BYTE_STREAM

#include <stdint.h>

enum seek_origin {
  beg, end, cur
};

struct ibstream {
    const char* buffer_;
    uint32_t    len_;
    uint32_t    pos_;
};

/// Create a byte input stream, the buf is data source
struct ibstream* ibstream_from_buf(const char* buf, uint32_t len);
void             ibstream_release(struct ibstream *ibs);

/// Read data of basic type from the input stream.
int32_t          ibstream_read(struct ibstream* ibs, char* buf, uint32_t len);
int8_t           ibstream_read_int8(struct ibstream* ibs);
int16_t          ibstream_read_int16(struct ibstream* ibs);
int32_t          ibstream_read_int32(struct ibstream* ibs);
int64_t          ibstream_read_int64(struct ibstream* ibs);
float            ibstream_read_float32(struct ibstream* ibs);
double           ibstream_read_float64(struct ibstream* ibs);
/// sstr is short string,
/// for example: [len(1 byte), str(len byte)].
int32_t          ibstream_read_sstr(struct  ibstream* ibs, char* str);
/// lstr is long string,
/// for example: [len(2 byte), str(len byte)].
int32_t          ibstream_read_lstr(struct  ibstream* ibs, char* str);


struct obstream {
    char*    buffer_;
    uint32_t capacity_;
    uint32_t pos_;

    int      auto_extend_;
};

/// Create a output stream.
/// @param initial_capacity: Provides the initial capacity of the buffer.
/// @param auto_size: If 1 When the stream buffer is full, the capacity is automatically expanded
struct obstream* obstream_create(uint32_t initial_capacity, int auto_extend);
void             obstream_destory(struct obstream* obs);

/// Write data of basic type to the output stream.
int              obstream_write(struct obstream* obs, char* buf, uint32_t len);
int              obstream_write_int8(struct obstream* obs, int8_t val);
int              obstream_write_int16(struct obstream* obs, int16_t val);
int              obstream_write_int32(struct obstream* obs, int32_t val);
int              obstream_write_int64(struct obstream* obs, int64_t val);
int              obstream_write_float32(struct obstream* obs, float val);
int              obstream_write_float64(struct obstream* obs, double val);
/// sstr is short string,
/// for example: [len(1 byte), str(len byte)].
int              obstream_write_sstr(struct  obstream* obs, char* str, uint8_t len);
/// lstr is long string,
/// for example: [len(2 byte), str(len byte)].
int              obstream_write_lstr(struct  obstream* obs, char* str, uint16_t len);

#endif // #ifndef IO_BYTE_STREAM