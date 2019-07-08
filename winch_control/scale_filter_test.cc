// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "scale_filter.h"

void PrintWeight(double grams, int64_t wtime) {
  printf("Weight: %4.5lf   time: %ld\n", grams, wtime);
}

bool global_error = false;
void ErrorFunc() {
  printf("Error function called, ending loop!\n");
  global_error=true;
}


// Stuff from ScaleFilter:
  static constexpr size_t kMaxDataPoints = 1000;
  static constexpr size_t kPointsForFiltering = 30;
  static constexpr double kDrainingThreshGramsPerSecond = -50.0;  //TODO: check value
  static constexpr double kDrainingConfidenceThresh = 10.0;  //TODO: check value
  // Data points to use when checking for draining.  Note that each point delays
  // the warning by ~100ms.
  static constexpr int kPointsToCheckForDrain = 30;
  std::deque<double> weight_data_;
  std::deque<int64_t> time_data_;


double ToGrams(uint32_t raw) { return ((double)raw);}

bool OnNewMeasurement(double weight, int64_t tmeas, SlopeInfo *info) {
  // add data to deque
  {
    // std::lock_guard<std::mutex> lock(data_lock_);
    weight_data_.push_back(weight);
    time_data_.push_back(tmeas);
    if (weight_data_.size() < kPointsForFiltering)
      return false;
  }
  // -----------------------------------------------------------
  // Check Drain Code
  // ----------------------------------------------------------
  if (weight_data_.size() < kPointsToCheckForDrain)
      return false;
  // Fit line to data:
  // mx = average(weight_data_)
  // my = average(time_data_)
  // sum((weight - mx) * (time - my))
  // divide by sum((weight-mx)*(weight-mx))
  // if slope < threshold
  std::vector<double> weights;
  std::vector<int64_t> times;
  for (int i = weight_data_.size() - 1; i > 1 && i > (int)weight_data_.size() - kPointsToCheckForDrain; i--) {
    weights.push_back(ToGrams(weight_data_[i]));
    times.push_back(time_data_[i]);
  }
  *info = FitSlope(weights, times);

  // if (info.slope < kDrainingThreshGramsPerSecond &&
      // info.ave_diff < kDrainingConfidenceThresh &&
      // info.biggest_change < kTotalLossThreshold) {
    // std::cout << "Slope was: " << info.slope << " > " << kDrainingThreshGramsPerSecond 
              // << " grams/sec" << std::endl;
    // return true;
  // }
  // TODO: also check for longer trends with lower thresholds

  // If over max storage size, drop earliest
  if (weight_data_.size() > kMaxDataPoints) {
    // std::lock_guard<std::mutex> lock(data_lock_);
    weight_data_.pop_front();
    time_data_.pop_front();
  }
  return true;
}



int main(int argc, char **argv) {
  std::fstream data_file;
  data_file.open(argv[1], std::fstream::in);
  if(!data_file.is_open()) {
      printf("No data file at %s\n", argv[1]);
      return -1;
  }
  std::fstream log_file;
  const char* output_log_file = "slope_data.txt";
  log_file.open(output_log_file, std::fstream::out | std::fstream::app);
  if(!log_file.is_open()) {
      printf("Failed to open raw log file at %s\n", output_log_file);
      return -1;
  }
  while (data_file) {
      int64_t tmeas = 0;
      double uncal_data, cal_data;
      data_file >> tmeas >> uncal_data >> cal_data;
      SlopeInfo info;
      if (OnNewMeasurement(cal_data, tmeas, &info)) {
          log_file << tmeas << " " << cal_data << " " << info.mean << " "
              << info.slope << " " << info.ave_diff << " " << info.biggest_change <<std::endl;
      }
  }
  log_file.close();
  data_file.close();
  return 0;
}
