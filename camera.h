//
// Created by luo-zeqi on 2022/10/27.
//

#ifndef RASPBERRY_CAR__CAMERA_H_
#define RASPBERRY_CAR__CAMERA_H_

#include <stdint.h>

#define FRM_HEAD_SPACE  16
#define ALLOC_FRM(size) \
  (malloc(FRM_HEAD_SPACE + (size_t)(size)) + FRM_HEAD_SPACE)
#define FREE_FRM(frame) \
  free((frame) - FRM_HEAD_SPACE)

int  camera_open(uint32_t width, uint32_t height, int notifier_fd);
void camera_close();
int  camera_frame(uint8_t** frame);

#endif //RASPBERRY_CAR__CAMERA_H_
