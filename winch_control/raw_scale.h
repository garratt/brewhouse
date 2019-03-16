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
  int InitLoop(std::function<void(double, int64_t)> weight_callback,
                       std::function<void()> error_callback);

  ~RawScale();
 private:
  Status current_status_;
  int data_fd_, sclk_fd_;
  bool reading_thread_enabled_ = false;
  std::thread reading_thread_;
  std::function<void(double, int64_t)> weight_callback_;
  std::function<void()> error_callback_;
  static constexpr int kReqNumLowReadings = 3;
  static constexpr int kMaxReadsBeforeGiveUp = 3000; // at least 3 seconds
  static constexpr int kHX711DataLength = 25;
  std::mutex status_lock_;
  static constexpr int kMaxConsecutiveErrors = 10;
  bool had_fatal_error_ = false;

  bool ReadOne();
  void RecordError(int64_t tnow);

  void ReadingThread();
};

