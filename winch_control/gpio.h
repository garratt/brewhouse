// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pinout describes how relays are connected to the up board
// The numbers represent the (linux) gpio pin

#pragma once

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LEFT_WINCH_ENABLE 8
#define LEFT_WINCH_DIRECTION 11
#define RIGHT_WINCH_ENABLE 10
#define RIGHT_WINCH_DIRECTION 9

#define SET_BUTTON 15
#define PUMP_BUTTON 24
#define SPEAKER_IN 14

#define RIGHT_SLIDE_SWITCH 22

#define CHILLER_PUMP 2
#define VALVE_ENABLE 3
#define CARBOY_VALVE 4
#define CHILLER_VALVE 27
#define KETTLE_VALVE 21


#define SCALE_DATA 12
#define SCALE_SCLK 20

char path_buffer[50];

char *gpio_val(uint8_t pin) {
   sprintf(path_buffer, "/sys/class/gpio/gpio%u/value", pin);
   return path_buffer;
}

char *gpio_dir(uint8_t pin) {
   sprintf(path_buffer, "/sys/class/gpio/gpio%u/direction", pin);
   return path_buffer;
}

int SetOutput(uint8_t pin, uint8_t value) {
   int fd = open(gpio_val(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   write(fd, value ? "1" : "0", 1);
   close(fd);
   return 0;
}


int SetOpenDrain(uint8_t pin, uint8_t value) {
   int fd = open(gpio_dir(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   write(fd, value ? "low" : "in", 2 + value);
   close(fd);
   return 0;
}

int SetDirection(uint8_t pin, uint8_t direction, uint8_t value = 0) {
   int fd = open(gpio_dir(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   if (direction > 0) { // output
     if (value > 0) {
       write(fd, "high", 4);
     } else {
       write(fd, "low", 3);
     }
   } else {
     write(fd, "in", 2);
   }
   close(fd);
   return 0;
}

int ReadInput(uint8_t pin) {
   int fd = open(gpio_val(pin), O_RDWR);
   if (!fd) {
     printf("ReadInput: Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   char buffer;
   int bytes = read(fd, &buffer, 1);
   if (bytes == 0) {
     printf("ReadInput: Failed to read %s\n", gpio_val(pin));
     close(fd);
     return -1;
   }
   close(fd);
   if (buffer == '0') {
     return 0;
   }
   return 1;
}


int InitIO() {
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(CHILLER_PUMP, 1, 1)) return -1;
  if (SetDirection(VALVE_ENABLE, 1, 1)) return -1;
  if (SetDirection(CARBOY_VALVE, 1, 1)) return -1;
  if (SetDirection(CHILLER_VALVE, 1, 1)) return -1;
  if (SetDirection(KETTLE_VALVE, 1, 1)) return -1;
  if (SetDirection(SET_BUTTON, 0)) return -1;
  if (SetDirection(PUMP_BUTTON, 0)) return -1;
  if (SetDirection(SPEAKER_IN, 0)) return -1;
  if (SetDirection(RIGHT_SLIDE_SWITCH, 0)) return -1;
  if (SetDirection(SCALE_DATA, 0)) return -1;
  if (SetDirection(SCALE_SCLK, 1, 0)) return -1;
  // SetFlow(NO_PATH);
  return 0;
}
