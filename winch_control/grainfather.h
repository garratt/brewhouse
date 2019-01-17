// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"



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

void Test_ListenForBeeps() {
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


