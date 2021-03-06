// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fstream>
#include <vector>
#include <thread>
#include <functional>
#include "gpio.h"
#include "brew_types.h"

static constexpr int kHX711ResponseLength = 25;

// Perform blocking wait for scale to present data. Returns
int WaitForHX711(uint8_t data_pin = SCALE_DATA, uint8_t sclk = SCALE_SCLK);

double ReadHX711Data(uint8_t data_pin = SCALE_DATA, uint8_t sclk = SCALE_SCLK);

struct WeightLimiter {
  time_t last_log_ = 0;
  time_t first_log_ = 0;
  int count_ = 0;
  double sum_ = 0;
  // if the reading is this far from the average, return the average and
  // start a new set.
  static constexpr int kMaxDeviationGrams = 30;
  // If there is a jump in weight logging, return the average and start a new set
  static constexpr int kMaxTimeJumpSeconds = 60;
  // This is the rate at which we log, even if the weight is constant
  static constexpr int kLoggingPeriodSeconds = 300; // 5 minutes

  bool PublishWeight(double weight_in, double *weight_out, time_t *log_time);

  double GetWeight() { return count_ ? sum_ / count_ : 0; }
};

struct ScaleStatus {
   static constexpr unsigned NONE = 0;
   static constexpr unsigned ERROR = 0x01;
   static constexpr unsigned READY = 0x02;
   unsigned state = 0;
   double weight = 0;
};

class FakeScale {
  double current_weight_ = 12000;
  double dgrams_per_sec_ = 0;
  int64_t last_time_ = 0;

public:
  ScaleStatus GetWeight();
  ScaleStatus CheckWeight();

  void Update() {
    int64_t tdiff, tnow = GetTimeMsec();
    if (last_time_ == 0) {
      last_time_ = tnow;
      return;
    }
    tdiff = tnow - last_time_;
    current_weight_ += (tdiff * dgrams_per_sec_) / 1000;
    last_time_ = tnow;
  }

  void DrainOut() { dgrams_per_sec_ = -50; }
  void Evaporate() { dgrams_per_sec_ = -.5; }
  void Stabalize() { dgrams_per_sec_ = 0; }
};



class WeightFilter {
  double offset_ = 0, scale_ = 0;
  const char *calibration_file_;
  static constexpr double kErrorSentinelValue = -10000;
  static constexpr double kMinNormalReadingGrams = -10;    // -10 g
  static constexpr double kMaxNormalReadingGrams = 100000; // 100 kg
  // Default number of readings to take for good average
  static constexpr uint32_t kNumberOfReadings = 30;
  bool verbose_ = false;
  std::vector<int> raw_data_;
  double average_, average_sigma_;
  std::vector<double> sigma_;
  std::vector<bool> excluded_;
  time_t last_data_ = 0;
  bool disable_for_test_ = false;
  FakeScale fake_scale_;

  bool reading_thread_enabled_ = false;
  std::thread reading_thread_;
  std::function<void(double)> weight_callback_;

  int CollectRawData(int num_readings);
  void CalculateStats();

  // return the index of the non-excluded run with the largest sigma.
  // should be run after CalculateStats.
  int FindMaxSigma();

  void ReadingThread();

  // Removes all data points with the largest sigma until the
  // std deviation falls below a certain threshold.
  // returns the average of the data.
  // TODO: incorporate slope...
  double RemoveOutlierData();

  // Short blocking call. blocks for up to ~5ms, if a reading is availale.
  // Checks if new weight is available.  If it is, the weight is
  // read out (takes about 1 ms).  If there are enough readings, the
  // readings are filtered and a weight is produced.
  ScaleStatus CheckWeight();

  public:
  void DisableForTest() { disable_for_test_ = true; }

  int CheckScale();

  // Checks if everything is fine:
  //  - We have calibration loaded
  //  - We are reading the sensor
  //  - We are reading normal values
  // Then starts thread loop continously reading the scale
  // When a reading is available, (about every 3 seconds)
  // |callback| will be called with the weight.
  // This function blocks while it checks the scale readings.
  // It can take around 3 seconds to return
  void InitLoop(std::function<void(double)> callback);

  // Initialize with calibration.  Creates file otherwise, and writes to it on
  // calls to Calibrate()
  WeightFilter(const char *calibration_file);

  // Assume that the load cell value is linear with weight (which is the whole point right?)
  // Calibrate will need to be called with calibration_mass == 0, then again with
  // calibration_mass == something non-zero.
  int Calibrate(double calibration_mass);

  // Set the weight expectation, so the scale can give alarms if the weight change
  // is unexpected
  // Als, allows the simulated scale to react properly
  void ExpectStable() { fake_scale_.Stabalize(); }
  void ExpectDecant() { fake_scale_.DrainOut(); }
  void ExpectEvaporation() {fake_scale_.Evaporate(); }

  // Take in a raw data reading and return a filtered value
  // 1) Filter erroneous readings
  //    If the time between readings is short and the reading is wildly different,
  //    throw it out.
  // 2) Average out noise
  //    Until proven that the noise is not gaussian, assume it is and just average the
  //    last N readings (within a time period)
  // 3) Apply scale and offset from calibration

  // Take a number of readings, average.
  // Way to take independant, self contained reading.  This call blocks (for up to
  // 3 seconds) until reading is done.
  // This call will also interfere with an ongoing reading using CheckWeight.
  // Don't use both interfaces simultaneously.
  ScaleStatus GetWeight(bool verbose = false);

  ~WeightFilter();
};


// An interactive program which weighs the scale at 0 and at a weight
// given by |calibration_mass|.  Calibration is stored in a text file specified
// by |calibration_file|
void RunManualCalibration(int calibration_mass,
                          const char *calibration_file = "scale_calibration.txt");
