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

struct BeepStatus {
   static constexpr unsigned NONE = 0;
   static constexpr unsigned ERROR = 0x01;
   static constexpr unsigned CONTINUOUS = 0x02;
   static constexpr unsigned LONG = 0x04;
   static constexpr unsigned SHORT = 0x08;
   unsigned state = 0;
   unsigned length = 0;
};

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

  bool IsContinous() {
    if (!IsClose(stop_ - start_, 500)) return false;
    if (!IsClose(start_ - prev_stop_, 500)) return false;
    if (!IsClose(prev_stop_ - prev_start_, 500)) return false;
    return true;
  }

  public:
    BeepStatus CheckBeep() {
      BeepStatus bs;
      int val = ReadInput(SPEAKER_IN);
      if (val < 0) {
        bs.state = BeepStatus::ERROR;
        return bs;
      }
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
        bs.length = stop_ - start_;
        bs.state = BeepStatus::SHORT;
        if (IsContinous()) {
          bs.state = BeepStatus::CONTINUOUS;
          return bs;
        }
        if (IsClose(stop_ - start_, 1500)) {
          bs.state = BeepStatus::LONG;
        }
        return bs;
      }
        bs.state = BeepStatus::NONE;
        return bs;
    }

  BeepTracker() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    initial_seconds_ = t.tv_sec;
  }

    // int CheckContinous() {
      // int val = CheckBeep();
      // if (val <= 0) return val;
      // val > 0.
      // Critera for Continous beep: 500ms on, 500ms off, 500ms on
      // if (!IsClose(stop_ - start_, 500)) return 0;
      // if (!IsClose(start_ - prev_stop_, 500)) return 0;
      // if (!IsClose(prev_stop_ - prev_start_, 500)) return 0;
      // return 1;
    // }

    // int CheckLongBeep() {
      // int val = CheckBeep();
      // if (val <= 0) return val;
      // if (IsClose(stop_ - start_, 1500)) {
        // return 1;
      // }
      // return 0;
    // }

  
void Test_ListenForBeeps() {
  BeepStatus ret;
  while(1) {
    usleep(1000);
    ret = CheckBeep();
    if (ret.state == BeepStatus::SHORT) {
      printf("short beep: length: %d off: %lu  prev: %lu\n",
          ret.length, start_ - prev_stop_, prev_stop_ - prev_start_);
    }
    if (ret.state == BeepStatus::LONG) {
      printf("long beep: length: %d off: %lu  prev: %lu\n",
          ret.length, start_ - prev_stop_, prev_stop_ - prev_start_);
    }
    if (ret.state == BeepStatus::CONTINUOUS) {
      printf("CONTINUOUS beep: length: %d off: %lu  prev: %lu\n",
          ret.length, start_ - prev_stop_, prev_stop_ - prev_start_);
    }
  }
}

};





