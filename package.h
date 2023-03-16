//
// Created by 13918962841 on 2022/10/18.
//

#ifndef RASPBERRY_CAR_TERMINAL_PACKAGE_H
#define RASPBERRY_CAR_TERMINAL_PACKAGE_H

#include <stdint.h>

#define PACKAGE_HEAD_LEN 5

#pragma pack(1)
struct package {
    uint8_t   pkg_id_;
    uint32_t  body_len_;
};
#pragma pack()

typedef void (*package_cb)(struct package *);

static int package_len(struct package *pkg) {
    return PACKAGE_HEAD_LEN + pkg->body_len_;
}

static char* package_body(struct package *pkg) {
    return (char *) (pkg + 1);
}

#endif //RASPBERRY_CAR_TERMINAL_PACKAGE_H
