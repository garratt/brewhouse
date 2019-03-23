// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fstream>
#include <vector>
#include <thread>
#include <functional>



class FakeScale : public RawScale {
  double current_weight_ = 12000;
  double dgrams_per_sec_ = 0;
  int64_t last_time_ = 0;

public:
  int InitLoop(std::function<void(double, int64_t)> callback,
                       std::function<void()> error_callback) {
  weight_callback_ = callback;
  error_callback_ = error_callback;
  reading_thread_enabled_ = true;
  reading_thread_ = std::thread(&FakeScale::ReadingThread, this);
  return 0;
  }

  bool ReadOne() {
    usleep(100000);
    int64_t tdiff, tnow = GetTimeMsec();
    if (last_time_ == 0) {
      last_time_ = tnow;
      return false;
    }
    tdiff = tnow - last_time_;
    current_weight_ += (tdiff * dgrams_per_sec_) / 1000;
    last_time_ = tnow;

    std::lock_guard<std::mutex> lock(status_lock_);
    current_status_.readings++;
    current_status_.consecutive_errors = 0;
    current_status_.last_read_time = tnow;
    current_status_.last_reading = current_weight_;
    return true;
  }

  void StopLoop() {
    reading_thread_enabled_ = false;
    if (reading_thread_.joinable())
      reading_thread_.join();
  }

  void InputData(double d, int64_t t) {
    if (weight_callback_) {
      weight_callback_(d, t);
    }
  }

  void DrainOut() { dgrams_per_sec_ = -50; }
  void Evaporate() { dgrams_per_sec_ = -.5; }
  void Stabalize() { dgrams_per_sec_ = 0; }
};



