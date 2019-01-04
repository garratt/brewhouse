// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Pinout describes how relays are connected to the up board
// The numbers represent the (linux) gpio pin

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LEFT_WINCH_ENABLE 5
#define LEFT_WINCH_DIRECTION 6
#define RIGHT_WINCH_ENABLE 2
#define RIGHT_WINCH_DIRECTION 3

#define SET_BUTTON 7
#define PUMP_BUTTON 8
#define SPEAKER_IN 1

#define RIGHT_SLIDE_SWITCH 21

#define gpio_dir(pin) "/sys/class/gpio/gpio" str(s) "/direction"
#define gpio_val(pin) "/sys/class/gpio/gpio" str(s) "/value"
#define str(s) #s

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
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   char buffer;
   int bytes = read(fd, &buffer, 1);
   if (bytes == 0) {
     printf("Failed to read %s\n", gpio_val(pin));
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
  if (SetDirection(SET_BUTTON, 0)) return -1;
  if (SetDirection(PUMP_BUTTON, 0)) return -1;
  if (SetDirection(SPEAKER_IN, 0)) return -1;
  if (SetDirection(RIGHT_SLIDE_SWITCH, 0)) return -1;
  return 0;
}


#define RIGHT 0
#define LEFT 1

// Reel the winch in, running for |ms| milliseconds.
// the serial number can be specified using serial.
int RunWinch(uint32_t ms, int side, int direction) {
   int enable = (side == RIGHT) ? RIGHT_WINCH_ENABLE : LEFT_WINCH_ENABLE;
   int dir = (side == RIGHT) ? RIGHT_WINCH_DIRECTION : LEFT_WINCH_DIRECTION;
   if(SetOutput(dir, direction)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(enable, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(1000 * ms); // run for given amount of time
   if (SetOutput(enable, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(dir, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   return 0;
}

int LeftGoUp(uint32_t ms) {    return RunWinch(ms, 1, 1); }
int LeftGoDown(uint32_t ms) {  return RunWinch(ms, 1, 0); }
int RightGoUp(uint32_t ms) {   return RunWinch(ms, 0, 1); }
int RightGoDown(uint32_t ms) { return RunWinch(ms, 0, 0); }

int RaiseToDrain() {
  // Assumes we are in the mash state
  if (RightGoUp(2500)) { // go up until we hit the limit
    return -1;
  }
  if (RightGoDown(200)) {
    return -1;
  }
  return 0;
}

int MoveToSink() {
  uint32_t step_size = 75;
  int val;
  do {
    if (LeftGoDown(step_size)) { return -1; }
    if (RightGoUp(step_size * 1.1)) { return -1; }
    usleep(500000); // let it settle?  I'd like to get it to not swing so much...
    // read the Right slide switch to see if we are done:
    val = ReadInput(RIGHT_SLIDE_SWITCH);
    if (step_size < 200) {
      step_size += 25;
    }
  } while (val > 0);
  if (val < 0) {
    return -1;
  }

  // Now we are over the sink, lower away!
  return RightGoDown(2000);
}

int LowerHops() {
  return LeftGoDown(2000);
}

int RaiseHops() {
  return LeftGoUp(1800);  // go up a little less than we went down
}

enum FlowPath { KETTLE, CHILLER, CARBOY};

int SetFlow(FlowPath path) {
  // TODO
  return 0;
}

int ActivateChillerPump() {
  // TODO
  return 0;
}
int DeactivateChillerPump() {
  // TODO
  return 0;
}

int HitButton(uint8_t button) {
  if (SetOpenDrain(button, 1)) {
    printf("Failed to set open drain output\n");
    return -1;
  }
  usleep(100000); // let the button click register
  if (SetOpenDrain(button, 0)) {
    printf("Failed to set output\n");
    return -1;
  }
  usleep(100000); // make sure we don't do anything else right after
  return 0;
}


int WaitForMashStart() {
  // Wait for Long beep
  // Then wait for the set button to be pushed
  return 0;
}

int WaitForBeeping() {
  // TODO
  return 0;
}

void WaitMinutes(uint32_t minutes) {
  time_t begin = time(NULL);
  do {
    sleep(10);
  } while (difftime(begin, time(NULL)) < minutes * 60);
}

void RunTestCommand(int argc, char **argv) {
  if (argc < 2) return;

  if (argv[1][0] == 'S') {
    HitButton(SET_BUTTON);
    return;
  }

  if (argv[1][0] == 'P') {
    HitButton(SET_BUTTON);
    return;
  }


  if (argv[1][0] == 'i') {
    int slide = ReadInput(RIGHT_SLIDE_SWITCH);
    for(int i = 0; i < 5; ++i) {
      printf("Right Slide Switch: %d\n", slide);
      sleep(1);
    }
    return;
  }

  if (argv[1][0] == 'l' || argv[1][0] == 'r' ) {
    // Default to 200ms, which is ~8cm.
    uint32_t ms = 200;  
    if (argc > 2) {
      int ms_in = atoi(argv[2]);
      ms = (ms_in < 0) ? 0 : ms_in;
    }
  
    if (argv[1][0] == 'l' && argv[1][1] == 'u') {
      LeftGoUp(ms);
    }
  
    if (argv[1][0] == 'l' && argv[1][1] == 'd') {
      LeftGoDown(ms);
    }

    if (argv[1][0] == 'r' && argv[1][1] == 'u') {
      RightGoUp(ms);
    }

    if (argv[1][0] == 'r' && argv[1][1] == 'd') {
      RightGoDown(ms);
    }
  }
}

int main(int argc, char **argv) {
  InitIO();

  if (argc > 1) {
    RunTestCommand(argc, argv);
    return 0;
  }

  if (WaitForMashStart()) return -1;
  // TODO: time how long mash takes

  // Wait for Mash to be done
  if (WaitForBeeping()) return -1;
  // Mash is Done! Lift and let drain
  if(RaiseToDrain()) return -1;
  //TODO: Wait for 30 minutes

  // Draining done.
  // SkipToBoil();
  HitButton(SET_BUTTON);

  if(MoveToSink()) return -1;

  // Wait for beeping, which indicates boil reached
  if (WaitForBeeping()) return -1;

  if (LowerHops()) return -1;

  HitButton(PUMP_BUTTON);
  WaitMinutes(45);

  SetFlow(FlowPath::CHILLER);

  // TODO: wait for 10 minutes
  WaitMinutes(10);

  SetFlow(FlowPath::CARBOY);
  //Wait for 2 minutes
  WaitMinutes(2);

  SetFlow(FlowPath::KETTLE);

  // Wait for boil to complete:
  WaitForBeeping();

  RaiseHops();

  SetFlow(FlowPath::CHILLER);
  ActivateChillerPump();
  HitButton(PUMP_BUTTON);

  // Wait for 20 minutes
  WaitMinutes(20);

  HitButton(PUMP_BUTTON);
  DeactivateChillerPump();
  // Wait for a minute
  WaitMinutes(1);

  // Decant:
  SetFlow(FlowPath::CARBOY);
  HitButton(PUMP_BUTTON);
  WaitMinutes(20);
  // Wait for 20 minutes
  // TODO: Yikes!  Need a way to know when we are done!
  HitButton(PUMP_BUTTON);
  return -1;
}



















