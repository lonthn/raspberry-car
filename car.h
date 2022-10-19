//
// Created by luo on 2022/10/2.
//

#ifndef CAR_CAR_H
#define CAR_CAR_H

#include <stdint.h>

struct car {};

int  car_init();
void car_release();

void car_forward(uint32_t ls, uint32_t rs);
void car_backward(uint32_t ls, uint32_t rs);
void car_update_speed(uint32_t ls, uint32_t rs);
void car_stop();

//void car_open_light();
//void car_close_light();
//void car_open_camera();
//void car_close_camera();

#endif //CAR_CAR_H
