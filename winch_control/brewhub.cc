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

#define SET_BUTTON 25
#define PUMP_BUTTON 8
#define SPEAKER_IN 1

#define RIGHT_SLIDE_SWITCH 21

// #define str(s) #s
// #define gpio_dir(pin) "/sys/class/gpio/gpio" str(pin) "/direction"
// #define gpio_val(pin) "/sys/class/gpio/gpio" str(pin) "/value"

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
// down is 0
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

// returns -1 if cannot communicate
// 0 if not stopped
// 1 if stopped (the limit switch is triggered)
int IsRightSlideStop() {
  // Active low (for now)
  int val = ReadInput(RIGHT_SLIDE_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}


//Run both winches:
// Direction: Right: 0 Left: 1
int RunBothWinches(uint32_t ms, int direction) {
  //if direction == right, left down, right up
  int ldir = direction ? 1 : 0;
  int rdir = direction? 0 : 1;
   if(SetOutput(RIGHT_WINCH_DIRECTION, rdir)) {
     printf("Failed to set output\n");
     return -1;
   }
   if(SetOutput(LEFT_WINCH_DIRECTION, ldir)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(RIGHT_WINCH_ENABLE, 1) || SetOutput(LEFT_WINCH_ENABLE, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   // TODO: if moving right, stop at the right stop
   // Check every ms.
   uint32_t ms_counter = 0;
   do {
     usleep(1000);
     ms_counter++;
     if (direction == 0 && IsRightSlideStop()) {
       break;
     }
   } while (ms_counter <= ms);

   if (SetOutput(RIGHT_WINCH_ENABLE, 0) || SetOutput(LEFT_WINCH_ENABLE, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(LEFT_WINCH_DIRECTION, 0) || SetOutput(RIGHT_WINCH_DIRECTION, 0)) {
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
int GoLeft(uint32_t ms) { return RunBothWinches(ms, 1); }
int GoRight(uint32_t ms) { return RunBothWinches(ms, 0); }

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
  // Raise to limit:
  RightGoUp(300);
  // Lower a little:
  RightGoDown(100);
  // Now scoot over using both winches:
  GoRight(3000); // This will quit early when it hits the limit switch


  // uint32_t step_size = 75;
  // int val;
  // do {
    // if (LeftGoDown(step_size)) { return -1; }
    // if (RightGoUp(step_size * 1.1)) { return -1; }
    // usleep(500000); // let it settle?  I'd like to get it to not swing so much...
    // read the Right slide switch to see if we are done:
    // val = ReadInput(RIGHT_SLIDE_SWITCH);
    // if (step_size < 200) {
      // step_size += 25;
    // }
  // } while (val > 0);
  // if (val < 0) {
    // return -1;
  // }

  // Now we are over the sink, lower away!
  return RightGoDown(3000);
}

int LowerHops() {
  return LeftGoDown(3000);
}

int RaiseHops() {
  return LeftGoUp(2500);  // go up a little less than we went down
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

class BeepTracker {
  uint64_t start_ = 0, stop_ = 0, prev_start_ = 0, prev_stop_ = 0;
  int prev_val_ = 1;
  uint64_t initial_seconds_;
  // get milliseconds since this object was initialized.
  // This time should only be used for diffs
  uint64_t GetTime() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t ret = t.tv_sec - initial_seconds_;
    ret *= 1000;
    ret += t.tv_nsec / 1000000;
    return ret;
  }

  bool IsClose(uint64_t val, uint64_t target) {
    uint64_t diff = val > target? val - target: target - val;
    if (diff < target / 10) return true;
    return false;
  }

  public:
    int CheckBeep() {
      int val = ReadInput(SPEAKER_IN);
      if (val < 0) { return -1; }
      // just turned on:
      int change = val - prev_val_;
      prev_val_ = val;
      if (change < 0) {
        prev_start_ = start_;
        start_ = GetTime();
      }
      // Just turned off:
      if (change > 0) {
        prev_stop_ = stop_;
        stop_ = GetTime();
        return stop_ - start_;
      }
      return 0;
    }

  BeepTracker() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    initial_seconds_ = t.tv_sec;
  }

    int CheckContinous() {
      int val = CheckBeep();
      if (val <= 0) return val;
      // val > 0.
      // Critera for Continous beep: 500ms on, 500ms off, 500ms on
      if (!IsClose(stop_ - start_, 500)) return 0;
      if (!IsClose(start_ - prev_stop_, 500)) return 0;
      if (!IsClose(prev_stop_ - prev_start_, 500)) return 0;
      return 1;
    }

    int CheckLongBeep() {
      int val = CheckBeep();
      if (val <= 0) return val;
      if (IsClose(stop_ - start_, 1500)) {
        return 1;
      }
      return 0;
    }
};


int WaitForMashStart() {
  // Wait for Long beep
  BeepTracker bt;
  int ret;
  do {
    usleep(1000);
    ret = bt.CheckLongBeep();
  } while (ret == 0);
  return ret;
  // TODO: Then wait for the set button to be pushed
}


int WaitForBeeping() {
  BeepTracker bt;
  int ret;
  do {
    usleep(1000);
    ret = bt.CheckContinous();
  } while (ret == 0);
  if (ret > 0) {
    HitButton(SET_BUTTON);
  }
  return ret;
}

void ListenForBeeps() {
  BeepTracker bt;
  int ret;
  while(1) {
    usleep(1000);
    ret = bt.CheckBeep();
    if (ret > 0) {
      printf("beep: %d\n", ret);
    }
  }
}

void WaitMinutes(uint32_t minutes) {
  time_t begin = time(NULL);
  do {
    sleep(10);
  } while (difftime(begin, time(NULL)) < minutes * 60);
}

void DryRun() {
  printf("Waiting for mash to start...\n");
  WaitForMashStart();
  printf("Mash at Temp. Waiting for Mash to Finish.\n");
  // TODO: time how long mash takes

  // Wait for Mash to be done
  WaitForBeeping();
  printf("Mash is Done! Lift and let drain\n");
  sleep(1);
  printf("Skip to Boil\n");
  HitButton(SET_BUTTON);
  // wait for boil temp to be reached
  
  WaitForBeeping();
  printf("Boil Reached\n");
  HitButton(SET_BUTTON);
  // Wait for Boil to be done
  WaitForBeeping();
  printf("Boil Done\n");
}


void RunTestCommand(int argc, char **argv) {
  if (argc < 2) return;

  if (argv[1][0] == 'S') {
    HitButton(SET_BUTTON);
    return;
  }

  if (argv[1][0] == 'P') {
    HitButton(PUMP_BUTTON);
    return;
  }

  if (argv[1][0] == 'd') {
    DryRun();
    return;
  }

  if (argv[1][0] == 'L') {
    ListenForBeeps();
    return;
  }

  if (argv[1][0] == '1') {
    RaiseToDrain();
    return;
  }

  if (argv[1][0] == '2') {
    MoveToSink();
    return;
  }

  if (argv[1][0] == '3') {
    LowerHops();
    return;
  }

  if (argv[1][0] == '4') {
    RaiseHops();
    return;
  }


  if (argv[1][0] == 'i') {
    int slide = ReadInput(RIGHT_SLIDE_SWITCH);
    for(int i = 0; i < 5; ++i) {
      printf("Right Slide Switch: %d\n", slide);
      usleep(1001);
    }
    return;
  }
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
    printf("Left doing down\n");
    LeftGoDown(ms);
  }

  if (argv[1][0] == 'r' && argv[1][1] == 'u') {
    RightGoUp(ms);
  }

  if (argv[1][0] == 'r' && argv[1][1] == 'd') {
    RightGoDown(ms);
  }

  if (argv[1][0] == 'b' && argv[1][1] == 'l') {
    RunBothWinches(ms, 1);
  }

  if (argv[1][0] == 'b' && argv[1][1] == 'r') {
    RunBothWinches(ms, 0);
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

  SetFlow(CHILLER);

  // TODO: wait for 10 minutes
  WaitMinutes(10);

  SetFlow(CARBOY);
  //Wait for 2 minutes
  WaitMinutes(2);

  SetFlow(KETTLE);

  // Wait for boil to complete:
  WaitForBeeping();

  RaiseHops();

  SetFlow(CHILLER);
  ActivateChillerPump();
  HitButton(PUMP_BUTTON);

  // Wait for 20 minutes
  WaitMinutes(20);

  HitButton(PUMP_BUTTON);
  DeactivateChillerPump();
  // Wait for a minute
  WaitMinutes(1);

  // Decant:
  SetFlow(CARBOY);
  HitButton(PUMP_BUTTON);
  WaitMinutes(20);
  // Wait for 20 minutes
  // TODO: Yikes!  Need a way to know when we are done!
  HitButton(PUMP_BUTTON);
  return -1;
}


















