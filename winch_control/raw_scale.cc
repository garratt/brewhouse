// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raw_scale.h"

#include <fstream>
#include <thread>
#include <functional>
#include <iostream>
#include <string>
#include <string.h>
#include "gpio.h"
#include "brew_types.h"

RawScale::Status RawScale::GetStatus() {
   std::lock_guard<std::mutex> lock(status_lock_);
   return current_status_;
}

void RawScale::RecordError(int64_t tnow, int error) {
   if (error > 0) {
     std::cout << "RawScale Read Error: " << strerror(error) << std::endl;
   }
   if (error == 0) {
     std::cout << "Error: read 0 bytes" <<std::endl;
   }
   std::lock_guard<std::mutex> lock(status_lock_);
   current_status_.errors++;
   current_status_.consecutive_errors++;
   current_status_.last_error = GetTimeMsec();
   if (current_status_.consecutive_errors > kMaxConsecutiveErrors) {
      std::cout << "Fatal: Too many consecutive errors (";
      std::cout << current_status_.consecutive_errors << ")" <<std::endl;
     had_fatal_error_ = true;
   }
}

void PrintRawValue(uint32_t val) {
  std::string out;
  uint32_t mask = 0x80000000;
  for (int i = 0; i <32; ++i) {
    out += (val & (mask >> i)) ? "1": "0";
  }
  std::cout << "Raw value: " << out << "   " << val <<  std::endl;
}

bool RawScale::ReadOne() {
  // The HX711 communicates by pulling the data line low every N Hz.
  // Wait for the Data line to be pulled low:
  int valid_count = 0;
  int num_reads = 0;
  do {
    // The time between conversions is 100ms (at 10 hz), and data won't come
    // until we strobe the clock, so we can poll pretty infrequently...
    usleep(1000);
    char buffer;
    num_reads++;
    lseek(data_fd_, 0, SEEK_SET);
    int bytes = read(data_fd_, &buffer, 1);
    if (bytes <= 0) {
        int myerr = errno;
        RecordError(GetTimeMsec(), myerr);
        return false;
    }
    if (buffer == '0') {  // the signal is active low, so we count the # of 0 readings
      valid_count++;
      num_reads = 0;
    } else {
      valid_count = 0;
    }
    if (num_reads > kMaxReadsBeforeGiveUp) {
      std::cout << "Fatal: num_reads > kMaxReadsBeforeGiveUp" <<std::endl;
      had_fatal_error_ = true;
      return false;
      // shut it down, we're not functioning.
    }
  } while (valid_count < kReqNumLowReadings);

  // Data is now available, so take the time:
  int64_t tnow = GetTimeMsec();
  // Pulse the sclk line at period of 2 us.  The rising edge triggers
  // the next bit to be available on data line, so read on the falling edge.
  // timespec sleep_time = { .tv_sec = 0, . tv_nsec = 100}, rem;
  // char buffer;
  uint32_t ret = 0;
  for (int i = 0; i < kHX711DataLength; ++i) {
    // Pull high
    write(sclk_fd_, "1", 1);
    // TODO: Does this busy wait need to exist?
    for (int j = 0; j < 10; ++j) {
      asm("nop");
    }
    write(sclk_fd_, "0", 1);
    // Read the value at the data pin:

    char buffer;
    lseek(data_fd_, 0, SEEK_SET);
    int bytes = read(data_fd_, &buffer, 1);
    if (bytes <= 0) {
        int myerr = errno;
        RecordError(GetTimeMsec(), myerr);
        return false;
    }
    // Otherwise, the data was good. just convert from ascii:
    ret = ret << 1;
    ret += buffer - '0';
  }
  // successful read!
  // One check here, since it is a binary thing:
  // If we screw up the timing, we will just read ones
  // for the rest of the data.  So we want to throw out data
  // that ends in all ones.  Of course, sometimes this will happen
  // naturally, so we strike a balance between throwing out to much
  // data and too little.  The noise floor is around 1000, so throwing
  // out 1FF (511) and above shouldn't affect the data much.
  if ((ret ^ (ret + 1)) > kMaxConsecutiveOnesValue) {
    RecordError(tnow, -1);
    return false;
  }
  // Now we will keep the data
  {
    std::lock_guard<std::mutex> lock(status_lock_);
    current_status_.readings++;
    current_status_.consecutive_errors = 0;
    current_status_.last_read_time = tnow;
    current_status_.last_reading = ret;
  }
  // If debugging scale values, this can be helpful:
  // PrintRawValue(ret);
  return true;
}

void RawScale::ReadingThread() {
  while (reading_thread_enabled_) {
    if (ReadOne()) {
      std::lock_guard<std::mutex> lock(status_lock_);
      weight_callback_(current_status_.last_reading,
                       current_status_.last_read_time);
    } else {
      if (had_fatal_error_) {
        std::cout << "RawScale has fatal error, quitting." <<std::endl;
        error_callback_();
        reading_thread_enabled_ = false;
      }
    }
  }
}

int RawScale::InitLoop(std::function<void(double, int64_t)> callback,
                       std::function<void()> error_callback) {
  if (SetDirection(SCALE_DATA, 0)) {
    printf("Failed to set GPIO direction RawScale::Init\n");
    return -1;
  }
  if (SetDirection(SCALE_SCLK, 1, 0)) {
    printf("Failed to set GPIO direction RawScale::Init\n");
    return -1;
  }
  std::string sclk_path = gpio_val_path(SCALE_SCLK);
  sclk_fd_ = open(sclk_path.c_str(), O_RDWR);
  if (!sclk_fd_) {
    printf("Failed to open %s\n", sclk_path.c_str());
    return -1;
  }
  std::string data_path = gpio_val_path(SCALE_DATA);
  data_fd_ = open(data_path.c_str(), O_RDWR);
  if (!data_fd_) {
    printf("Failed to open %s\n", data_path.c_str());
    return -1;
  }
  // Try to read and write from the buffers:
  char buffer;
  int bytes = read(data_fd_, &buffer, 1);
  if (bytes <= 0) {
    int myerr = errno;
    RecordError(GetTimeMsec(), myerr);
    return -1;
  }
  if (bytes == 0) {
    printf("Failed to read during RawScale::Init\n");
    return -1;
  }
  bytes = write(sclk_fd_, "0", 1);
  if (bytes < 1) {
    printf("Failed to write during RawScale::Init\n");
    return -1;
  }

  weight_callback_ = callback;
  error_callback_ = error_callback;
  reading_thread_enabled_ = true;
  reading_thread_ = std::thread(&RawScale::ReadingThread, this);
  return 0;
}

RawScale::~RawScale() {
  reading_thread_enabled_ = false;
  if (reading_thread_.joinable())
    reading_thread_.join();
  close(sclk_fd_);
  close(data_fd_);
}
