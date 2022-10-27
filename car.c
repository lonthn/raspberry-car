//
// Created by luo-zeqi on 2022/10/2.
//

#include "car.h"
#include "bcm2835/bcm2835.h"

#include <stdio.h>

#define LPIN0 RPI_GPIO_P1_15
#define LPIN1 RPI_GPIO_P1_16
#define RPIN0 RPI_GPIO_P1_08
#define RPIN1 RPI_GPIO_P1_10
#define LEN   RPI_V2_GPIO_P1_35
#define REN   RPI_GPIO_P1_12

#define max(a, b) ((a)>(b)?(a):(b))

enum car_state {
    state_forward,
    state_backward,
    state_stop,
};

enum car_state state_;

int car_init() {
    if (!bcm2835_init()) {
        printf("Failed to initial BCM2835!\n");
        return 0;
    }

    // select function
    bcm2835_gpio_fsel(LPIN0, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(LPIN1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RPIN0, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RPIN1, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(LEN,   BCM2835_GPIO_FSEL_ALT5);
    bcm2835_gpio_fsel(REN,   BCM2835_GPIO_FSEL_ALT5);

    // initial pwm params
    bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_512);
    bcm2835_pwm_set_mode(0, 1, 1);
    bcm2835_pwm_set_mode(1, 1, 1);
    bcm2835_pwm_set_range(0, 1024);
    bcm2835_pwm_set_range(1, 1024);

    state_= state_stop;
    return 1;
}

void car_release() {
    bcm2835_close();
}

void car_forward(uint32_t ls, uint32_t rs) {
    if (state_ == state_backward) {
        return;
    }

    ls = max(ls, 0);
    rs = max(rs, 0);

    bcm2835_gpio_write(LPIN0, HIGH);
    bcm2835_gpio_write(LPIN1, LOW);
    bcm2835_gpio_write(RPIN0, LOW);
    bcm2835_gpio_write(RPIN1, HIGH);
    bcm2835_pwm_set_data(0, ls);
    bcm2835_pwm_set_data(1, rs);
}

void car_backward(uint32_t ls, uint32_t rs) {
    if (state_ == state_forward) {
        return;
    }

    ls = max(ls, 0);
    rs = max(rs, 0);

    bcm2835_gpio_write(LPIN0, LOW);
    bcm2835_gpio_write(LPIN1, HIGH);
    bcm2835_gpio_write(RPIN0, HIGH);
    bcm2835_gpio_write(RPIN1, LOW);
    bcm2835_pwm_set_data(0, ls);
    bcm2835_pwm_set_data(1, rs);
}

void car_stop() {
    bcm2835_gpio_write(LPIN0, LOW);
    bcm2835_gpio_write(LPIN1, LOW);
    bcm2835_gpio_write(RPIN0, LOW);
    bcm2835_gpio_write(RPIN1, LOW);
}