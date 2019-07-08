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
#include "raw_scale.h"
#include "fake_scale.h"



struct SlopeInfo {
  size_t num_points = 0;
  double mean = 0, slope = 0, ave_diff = 0;
  double biggest_change = 0;
};

  // Maximum drainage rate we believe.  Anything more is too big.
  static constexpr double kMaxDrainSlope = 500; // ml per second
SlopeInfo FitSlope(std::vector<double> weights, std::vector<int64_t> times);

// We have 3 uses of the scale:
// to log accurate weights at specific times
// To measure changes in weight over time, to characterize processes
// And to alert when either:
//   - we are losing fluid unintentionally (usually because of a bad hose position)
//   - We have picked up the grainfather
class ScaleFilter {
  public:
  void DisableForTest() { disable_for_test_ = true; }

  // Gets the weight reading of the scale.
  // This will pull on some amount of historical data to get a filtered
  // reading.  If you want to limit how far back the data is taken from,
  // you can pass in a time, the averaging will be limited to after that
  // point.
  double GetWeight(int64_t since_time = 0);

  // Get an averaged weight using readings after this call was made.
  // This call will block until enough readings are available, then return the
  // weight.
  double GetWeightStartingNow(unsigned max_points = kPointsForFiltering,
                             int64_t timeout = 100000 * kPointsForFiltering);

  // Sets a callback to be called at a constant reporting_interval (in milliseconds)
  // with the time of the latest measurement and a filtered weight reading.
  void SetPeriodicWeightCallback(int64_t reporting_interval,
                                 std::function<void(double, int64_t)> callback);

  // This can be checked at any time
  // Checks if the weight is below the Kettle lifted threshold.
  // doesn't need to get as accurate reading so can return faster.
  bool HasKettleLifted();

  // Checks if the Grainfather is finished draining
  bool CheckEmpty();


  // Enabes a check if the kettle is losing weight at a rate
  // indicating it is draining somewhere.
  void EnableDrainingAlarm(std::function<void()> callback);
  void DisableDrainingAlarm();


  // Calls the given callback when draining is complete.
  // This is detected by looking for a set weight threshold, and
  // also checking if the draining rate decreases.
  // This also disables the draining alarm (for obvious reasons)
  void NotifyWhenDrainComplete(std::function<void()> callback);

  // Then starts thread loop continously reading the scale
  // After this call, callbacks from:
  //  EnableDrainingAlarm
  //  NotifyWhenDrainComplete
  //  SetPeriodicWeightCallback
  // can be called. 
  // This function will return < 0 if there is a problem reading
  // from the scale.
  // If a problem occurs later, error_callback will be called.
  int InitLoop(std::function<void()> error_callback);

  // Initialize with calibration.  Creates file otherwise, and writes to it on
  // calls to Calibrate()
  ScaleFilter(const char *calibration_file);

  // Assume that the load cell value is linear with weight (which is the whole point right?)
  // Calibrate will need to be called with calibration_mass == 0, then again with
  // calibration_mass == something non-zero.
  int Calibrate(double calibration_mass);

  ~ScaleFilter();

  // For Testing:
  FakeScale *GetFakeScale() { return &fake_scale_; }

 private:
  double offset_ = 0, scale_ = 1.0;
  const char *calibration_file_;
  std::deque<double> weight_data_;
  std::deque<int64_t> time_data_;
  RawScale raw_scale_;
  FakeScale fake_scale_;
  std::mutex data_lock_;
  bool looping_ = false;

  static constexpr double kMinNormalReadingGrams = -10;    // -10 g
  static constexpr double kMaxNormalReadingGrams = 100000; // 100 kg
  // The Kettle weighs about 8kg, so 2kg is pretty conservative.
  // This is the threshold below which we declare the kettle lifted off
  // of the scale.
  static constexpr double kKettleLiftedThresholdGrams = 2000;
  static constexpr size_t kPointsForFiltering = 30;
  // Max number of points to store in our data queue
  static constexpr size_t kMaxDataPoints = 1000;
  static constexpr int64_t kCheckEmptyIntervalMs = 1000;
  static constexpr int64_t kCheckDrainingIntervalMs = 500;
  // Represents slope / ave deviation from slope
  static constexpr double kDrainingThreshGramsPerSecond = -50.0;  //TODO: check value
  static constexpr double kDrainingConfidenceThresh = 10.0;  //TODO: check value
  static constexpr double kTotalLossThreshold = 200.0;  //TODO: check value
  // Data points to use when checking for draining.  Note that each point delays
  // the warning by ~100ms.
  static constexpr int kPointsToCheckForDrain = 30;
  // Below this weight, we declare the grainfather empty
  static constexpr double kEmptyThresholdGrams = 9000;

  bool disable_for_test_ = false;

  static constexpr const char *kRawLogFile = "./weight_data.txt";
  bool raw_logger_enabled_ = false;
  std::thread raw_logger_thread_;
  void RawLoggerThread();

  std::function<void(double)> weight_callback_;
  std::function<void()> error_callback_;

  // For periodic update:
  int64_t periodic_update_period_, last_periodic_update_ = 0;
  std::function<void(double, int64_t)> periodic_callback_;
  // For draining update:
  int64_t draining_update_period_, last_draining_update_ = 0;
  std::function<void()> draining_callback_;
  // For empty update:
  int64_t empty_update_period_, last_empty_update_ = 0;
  std::function<void()> empty_callback_;

  // callbacks for raw scale:
  void OnNewMeasurement(uint32_t weight, int64_t tmeas);
  void OnScaleError();

  // Filter all a set of data since |min_time_bound|
  // Right now just returns the mean.
  double FilterData(int64_t min_time_bound);

  // Fits a slope to the recent data, to see if we are losing weight
  // at a constant rate.
  bool CheckDraining();

  inline double ToGrams(uint32_t raw) { return ((double)raw - offset_) * scale_;}

};


