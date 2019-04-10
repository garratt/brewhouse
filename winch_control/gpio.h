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
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LEFT_WINCH_ENABLE 8
#define LEFT_WINCH_DIRECTION 11
#define RIGHT_WINCH_ENABLE 10
#define RIGHT_WINCH_DIRECTION 9

#define LEFT_SLIDE_SWITCH 24
#define RIGHT_SLIDE_SWITCH 22
#define TOP_SWITCH 25

#define CHILLER_PUMP 2
#define VALVE_ENABLE 3
#define CARBOY_VALVE 4
#define CHILLER_VALVE 27
#define KETTLE_VALVE 21


#define SCALE_DATA 12
#define SCALE_SCLK 20


std::string gpio_val_path(uint8_t pin);

// Set an output pin to high (1) or low (0)
// Returns -1 if the operation could not be accomplished.
// Usually this is because of permissions.
int SetOutput(uint8_t pin, uint8_t value);

// Set a pin as if it were an open drain output.
// value=1: the pin is output, value 0
// value=0: the pin is input, with weak pullup
// Returns -1 if the operation could not be accomplished.
// Usually this is because of permissions.
int SetOpenDrain(uint8_t pin, uint8_t value);

// Set the direction (in/out) of a pin, and optionally the value.
// setting direction and value is done as one operation, to avoid
// undesireable states at startup.
// Returns -1 if the operation could not be accomplished.
// Usually this is because of permissions.
int SetDirection(uint8_t pin, uint8_t direction, uint8_t value = 0);

// Return the value (1 or 0) of a pin.
// Returns -1 if the operation could not be accomplished.
// Usually this is because of permissions.
int ReadInput(uint8_t pin);

int InitIO();

int64_t GetTimeMsec();
