// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"

static constexpr int kHX711ResponseLength = 25;

double WaitForHX711(uint8_t data_pin = SCALE_DATA, uint8_t sclk = SCALE_SCLK) {
  uint32_t ret = 0;
  // The HX711 communicates by pulling the data line low every N Hz.
  // Wait for the Data line to be pulled low:
  int rval = 0;
  do {
    // The time between conversions is 100ms (at 10 hz), and data won't come
    // until we strobe the clock, so we can poll pretty infrequently...
    usleep(1000);  
    int trval = ReadInput(data_pin);
    // printf(" %d ", trval);
    if (trval < 0) { rval = trval; break;}
    if (trval) { 
      rval = 0;
    } else {
      rval++;
    }
  } while (rval < 10 );
  if (rval < 0) {
    printf("Failed to read data input\n");
    return ret;
  }
    usleep(1000);  
  return 0;
}

double ReadHX711Data(uint8_t data_pin = SCALE_DATA, uint8_t sclk = SCALE_SCLK) {
  // We need to toggle the sclk fairly fast. If the Sclk stays high longer than
  // 60 us, the chip powers down.  Since we don't have great low level hardware
  // control, we just have to take few instructions, and accept that we may get preempted.
  // TODO: use the last bit to verify that we have not reset.
  // int data_pin_fd = open(gpio_val(data_pin), O_RDWR);
  // if (!data_pin_fd) {
    // printf("Failed to open %s\n", gpio_val(data_pin));
    // return -1;
  // }
  int sclk_fd = open(gpio_val(sclk), O_RDWR);
  if (!sclk_fd) {
    printf("Failed to open %s\n", gpio_val(sclk));
    // close(data_pin_fd);
    return -1;
  }
  // Pulse the sclk line at period of 2 us.  The rising edge triggers
  // the next bit to be available on data line, so read on the falling edge.
  // timespec sleep_time = { .tv_sec = 0, . tv_nsec = 100}, rem;
  // char buffer;
  uint32_t ret = 0, rval;
  for (int i = 0; i < kHX711ResponseLength; ++i) {
    // Pull higha
    write(sclk_fd, "1", 1);
    // TODO: Does this busy wait need to exist?
    for (int j = 0; j < 10; ++j) {
      asm("nop");
    }
    write(sclk_fd, "0", 1);
    rval = ReadInput(data_pin);
    if (rval < 0) {
      printf("ReadHX711Data: Failed to read %s\n", gpio_val(data_pin));
      close(sclk_fd);
      return -1;
    }
    // int bytes = read(data_pin_fd, &buffer, 1);
    // if (bytes == 0) {
      // printf("ReadHX711Data: Failed to read %s\n", gpio_val(data_pin));
      // close(data_pin_fd);
      // close(sclk_fd);
      // return -1;
    // }
    ret = ret << 1;
    ret += rval; 
    // ret += (buffer == '1');
  }
  close(sclk_fd);
  // close(data_pin_fd);
  return ret;
}

#include <fstream>
#include <vector>

class WeightFilter {
  double offset_ = 0, scale_ = 0;
  const char *calibration_file_;
  static constexpr double kErrorSentinelValue = -10000;
  // Default number of readings to take for good average
  static constexpr uint32_t kNumberOfReadings = 30;
  bool verbose_ = false;
  std::vector<int> raw_data_;
  double average_, average_sigma_;
  std::vector<double> sigma_;
  std::vector<bool> excluded_;

  int CollectRawData(int num_readings) {
    raw_data_.clear();
    excluded_.clear();
      for (int i  = 0; i < num_readings; ++i) {
        WaitForHX711();
        int val = ReadHX711Data();
        if (val < 0) {
          printf("Error Reading Scale!\n");
          return -1;
        }
        if (verbose_) {
          printf("raw data %i\n", val);
        }
        if (val == 0x1ffffff) { // False reading
          --i;
        } else {
           raw_data_.push_back(val);
           excluded_.push_back(false);
        }
      }
      sigma_.resize(raw_data_.size());
      return 0;
  }

  void CalculateStats() {
    double total = 0;
    unsigned int unexcluded_count = 0;
    for (unsigned int i = 0; i < raw_data_.size(); ++i) {
      if (!excluded_[i]) {
        total += raw_data_[i];
        unexcluded_count++;
      }
    }
    average_ = total / unexcluded_count;
    // Now calculate sigma:
    double total_sigmas = 0;
    for (unsigned int i = 0; i < raw_data_.size(); ++i) {
      sigma_[i] = (raw_data_[i]-average_) * (raw_data_[i] - average_);
      if (!excluded_[i]) {
        total_sigmas += sigma_[i];
      }
    }
    average_sigma_ = total_sigmas / unexcluded_count;
  }

  int FindMaxSigma() {
    double max_sigma = 0;
    unsigned int max_index = 0;
    for (unsigned int i = 0; i < raw_data_.size(); ++i) {
        if (!excluded_[i] && max_sigma < sigma_[i]) {
          max_sigma = sigma_[i];
          max_index = i;
        }
    }
    return max_index;
  }


  public:

  // Initialize with calibration.  Creates file otherwise, and writes to it on
  // calls to Calibrate()
  WeightFilter(const char *calibration_file) : calibration_file_(calibration_file) {
    std::fstream calfile;
    calfile.open(calibration_file, std::fstream::in);
    if(!calfile.is_open()) {
      printf("No Calfile at %s\n", calibration_file_);
      return;
    }
    calfile >> offset_ >> scale_;
    calfile.close();
  }


  double RemoveOutlierData() {
    // Need three points for an outlier to exist.
    if (raw_data_.size() == 0) return 0;
    if (raw_data_.size() == 1) return raw_data_[0];
    if (raw_data_.size() == 2) return (raw_data_[0] + raw_data_[1]) / 2;

    int unexcluded_count;
    double dev;
    CalculateStats();
    do {
      int index = FindMaxSigma();
      dev = sigma_[index];
      excluded_[index] = true;
      unexcluded_count = 0;
      for (unsigned int i = 0; i < raw_data_.size(); ++i) {
        if(!excluded_[i])
      printf("%02d raw: %i  sigma %8.4f    ave: %8.4f   ave sigma:  %8.4f  dev: %8.4f %s\n", i,
             raw_data_[i], sigma_[i], average_, average_sigma_, sigma_[i] / average_sigma_,
             excluded_[i] ? "    (Excluded)" : "");
      if (!excluded_[i]) unexcluded_count++;
      }
      printf("  Excluding %d\n", index);
      CalculateStats();

    } while (average_sigma_ > 100000 || dev > 100000);
    // Kick out any point with sigma > 3* average sigma
    // and by kick out, I mean don't average them into the return value.
    
    if (unexcluded_count == 0) {
      printf("No non outliers.  This shouldn't be possible...\n");
      return kErrorSentinelValue;
    }
    return average_;
  }


  // Assume that the load cell value is linear with weight (which is the whole point right?)
  // Calibrate will need to be called with calibration_mass == 0, then again with
  // calibration_mass == something non-zero.
  int Calibrate(double calibration_mass) {
    if(CollectRawData(kNumberOfReadings)) {
      printf("Failed to collect raw data\n");
      return -1;
    }
    double average = RemoveOutlierData();
    if (calibration_mass == 0) {
      offset_ = average;
      return 0;
    }
    // if non-zero, we are to be finding the scaling factor:
    if ((average - offset_) == 0) {
      printf("Error, average == offset. calibration mass may not be sufficient\n");
      return -1;
    }
    scale_ =  calibration_mass / (average - offset_);
    // Write new calibration values out to file:
    std::fstream calfile;
    calfile.open(calibration_file_, std::fstream::out);
    if(!calfile.is_open()) {
      printf("Failed to open cal file at %s\n", calibration_file_);
      return -1;
    }
    printf("Calibration Result: Scale %f, Offset: %f\n", scale_, offset_);
    calfile << offset_ << " " << scale_ << std::endl;
    calfile.close();
    
    return 0;
  }

    // Take in a raw data reading and return a filtered value
    // 1) Filter erroneous readings
    //    If the time between readings is short and the reading is wildly different,
    //    throw it out.
    // 2) Average out noise
    //    Until proven that the noise is not gaussian, assume it is and just average the 
    //    last N readings (within a time period)
    // 3) Apply scale and offset from calibration

    // Take a number of readings, average.
    double GetWeight(bool verbose = false) {
      verbose_ = verbose;
      if(CollectRawData(kNumberOfReadings)) {
        printf("Failed to collect raw data\n");
        return kErrorSentinelValue;
      }
      double average = RemoveOutlierData();
      printf("average: %f   ", average);
      return scale_ * (average - offset_);
    }
};



void RunManualCalibration(int calibration_mass) {
    WeightFilter wf("./calibration.txt");
    printf("Calibrating Scale.\nRemove all weight. Taking data in\n");
    for (int i=5; i >=0; --i) {
      printf("%d...\n", i);
      sleep(1);
    }
    if(wf.Calibrate(0) < 0) {
      printf("failed to calibrate with zero mass.\n");
      return;
    }
    printf("Step 2: Put on %d weight. Taking data in\n", calibration_mass);
    for (int i=7; i >=0; --i) {
      wf.GetWeight(false);
      printf("%d...\n", i);
      sleep(1);
    }
    if(wf.Calibrate(calibration_mass) < 0) {
      printf("failed to calibrate with zero mass.\n");
      return;
    }
    printf("Calibration complete. Here are sample weights:\n");
    for (int i = 0; i < 100; ++i) {
       double val = wf.GetWeight(false);
       printf("Scale Reads %f\n", val);
    }
}
