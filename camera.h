//
// Created by luo-zeqi on 2022/10/27.
//

#ifndef RASPBERRY_CAR__CAMERA_H_
#define RASPBERRY_CAR__CAMERA_H_

#include <stdint.h>

int  camera_open(uint32_t width, uint32_t height);
void camera_close();
int  camera_frame(char* frame);

#endif //RASPBERRY_CAR__CAMERA_H_
