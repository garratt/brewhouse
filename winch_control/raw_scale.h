// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fstream>
#include <thread>
#include <functional>
#include <iostream>
#include "gpio.h"
#include "brew_types.h"


// Just performs reading in a thread, publishing dat when available:
class RawScale {
 public:
  struct Status {
    int64_t readings = 0;
    int64_t errors = 0;
    int64_t consecutive_errors = 0;
    int64_t start_time = 0;
    int64_t last_read_time = 0;
    int64_t last_error = 0;
    uint32_t last_reading = 0;
  };

  // dumps the state of the scale, to check latest weight, or
  // to determine if there are issues.
  Status GetStatus();

  // Then starts thread loop continously reading the scale
  // When a reading is available, (about every 100 miliseconds)
  // |callback| will be called with the weight.
  virtual int InitLoop(std::function<void(double, int64_t)> weight_callback,
                       std::function<void()> error_callback);

  ~RawScale();
 protected:
  Status current_status_;
  int data_fd_, sclk_fd_;
  bool reading_thread_enabled_ = false;
  std::thread reading_thread_;
  std::function<void(double, int64_t)> weight_callback_;
  std::function<void()> error_callback_;
  static constexpr int kReqNumLowReadings = 3;
  static constexpr int kMaxReadsBeforeGiveUp = 3000; // at least 3 seconds
  static constexpr int kHX711DataLength = 25;
  // The one filter we perform:
  // If we screw up the timing, we will just read ones
  // for the rest of the data.  So we want to throw out data
  // that ends in all ones.  The noise floor is around 1000,
  // so throwing out 1FF (511) and above shouldn't affect the
  // data much.
  static constexpr int kMaxConsecutiveOnesValue = 0x1FF; // 511

  std::mutex status_lock_;
  static constexpr int kMaxConsecutiveErrors = 10;
  bool had_fatal_error_ = false;

  virtual bool ReadOne();
  // Pass in the time and the errno value.
  // a negative errno means don't print anything,
  // a 0 errno means print 'read 0 bytes' error
  void RecordError(int64_t tnow, int my_errorno);

  void ReadingThread();
};

