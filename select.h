//
// Created by 13918962841 on 2022/10/28.
//

#ifndef RASPBERRY_CAR_SELECT_H
#define RASPBERRY_CAR_SELECT_H

#define SEL_READABLE  0x01
#define SEL_WRITEABLE 0x02

typedef int (*trigger_cb_t)(void *ctx, int fd);

void selecter_add(int fd, int fn, trigger_cb_t cb, void *ctx);
//void selecter_set(int fd, int fn, trigger_cb_t cb, void *ctx);
void selecter_remove(int fd, int fn);
int  selecter_poll(int timeout);

#endif //RASPBERRY_CAR_SELECT_H
