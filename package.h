//
// Created by 13918962841 on 2022/10/18.
//

#ifndef RASPBERRY_CAR_TERMINAL_PACKAGE_H
#define RASPBERRY_CAR_TERMINAL_PACKAGE_H

#include <stdint.h>

#define MINI_PACKAGE_HEAD_LEN 2
#define PACKAGE_HEAD_LEN 5

#pragma pack(1)
struct package {
    uint8_t  pkg_id_;
    uint8_t  body_len_;
};
#pragma pack()

typedef void (*package_cb)(struct package *);

static int mini_package_len(struct package *pkg) {
    return sizeof(struct package) + pkg->body_len_;
}
static int package_len(struct package *pkg) {
    return sizeof(struct package) + *(uint32_t *) &pkg->body_len_;
}

static char *mini_package_body(struct package *pkg) {
    return ((char *) pkg) + MINI_PACKAGE_HEAD_LEN;
}
static char *package_body(struct package *pkg) {
    return ((char *) pkg) + PACKAGE_HEAD_LEN;
}

#endif //RASPBERRY_CAR_TERMINAL_PACKAGE_H
