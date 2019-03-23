// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scale_filter.h"

#include <fstream>
#include <vector>
#include <thread>
#include <functional>
#include "gpio.h"
#include "brew_types.h"
#include "raw_scale.h"

// Gets the weight reading of the scale.
// This will pull on some amount of historical data to get a filtered
// reading.  If you want to limit how far back the data is taken from,
// you can pass in a time, the averaging will be limited to after that
// point.
double ScaleFilter::GetWeight(int64_t since_time) {
  return FilterData(since_time);
}

// This function does a silly amount of blocking, but we've got lots of time...
double ScaleFilter::GetWeightStartingNow(unsigned max_points, int64_t timeout) {
  // Wait until we see max_points points of data, or until timeout
  int64_t tnow = GetTimeMsec();
  unsigned num_points = 0;
  do {
    num_points = 0;
    if (!disable_for_test_) {
      usleep(100000); //at least sleep for the time in between readings
    }
    // find out how many points we have accumulated:
    for (unsigned i = 0; i < weight_data_.size() && i <= max_points; ++i) {
      if (time_data_[time_data_.size() - 1 - i] < tnow) {
        break;
      } else {
        num_points++;
      }
    }
    // printf("%s: wait loop points: %u\n", __func__, num_points);
  } while ((num_points < max_points) && (GetTimeMsec() - tnow < timeout));
  if (num_points == 0) {
    printf("We have timed out with no points!\n");
    return 0.0;
  }
  // How much should we wait?
  // we won't use more than kPointsForFiltering
  // measuring period * kPointsForFiltering
  return FilterData(tnow);
}



void ScaleFilter::SetPeriodicWeightCallback(int64_t reporting_interval,
    std::function<void(double, int64_t)> callback) {
  periodic_update_period_ = reporting_interval;
  periodic_callback_ = callback;
}

// Checks if the weight is below the Kettle lifted threshold.
// doesn't need to get as accurate reading so can return faster.
bool ScaleFilter::HasKettleLifted() {
  if (weight_data_.size() == 0) return false;
  return ToGrams(weight_data_.back()) < kKettleLiftedThresholdGrams;
}

// Enabes a check if the kettle is losing weight at a rate
// indicating it is draining somewhere.
void ScaleFilter::EnableDrainingAlarm(std::function<void()> callback) {
  draining_update_period_ = kCheckDrainingIntervalMs;
  draining_callback_ = callback;
}

void ScaleFilter::DisableDrainingAlarm() {
  draining_callback_ = nullptr;
}

// Calls the given callback when draining is complete.
// This is detected by looking for a set weight threshold, and
// also checking if the draining rate decreases.
// This also disables the draining alarm (for obvious reasons)
void ScaleFilter::NotifyWhenDrainComplete(std::function<void()> callback) {
  empty_callback_ = callback;
  draining_callback_ = nullptr;
  empty_update_period_ = kCheckEmptyIntervalMs;
}

// Then starts thread loop continously reading the scale
// When a reading is available, (about every 3 seconds)
// |callback| will be called with the weight.
// This function blocks while it checks the scale readings.
// It can take around 3 seconds to return
int ScaleFilter::InitLoop(std::function<void()> error_callback) {
  using std::placeholders::_1;
  using std::placeholders::_2;
  error_callback_ = error_callback;
  int ret;
  if (disable_for_test_) {
   ret = fake_scale_.InitLoop(std::bind(&ScaleFilter::OnNewMeasurement, this, _1, _2),
      std::bind(&ScaleFilter::OnScaleError, this));

  } else {
   ret = raw_scale_.InitLoop(std::bind(&ScaleFilter::OnNewMeasurement, this, _1, _2),
      std::bind(&ScaleFilter::OnScaleError, this));
  }
  if (ret == 0) {
    looping_ = true;
  } else {
    printf("Raw scale failed to initialize.\n");
  }

  return ret;
}

// Initialize with calibration.  Creates file otherwise, and writes to it on
// calls to Calibrate()
ScaleFilter::ScaleFilter(const char *calibration_file) : calibration_file_(calibration_file) {
  std::fstream calfile;
  calfile.open(calibration_file, std::fstream::in);
  if(!calfile.is_open()) {
    printf("No Calfile at %s\n", calibration_file_);
    return;
  }
  calfile >> offset_ >> scale_;
  calfile.close();
}

void ScaleFilter::OnScaleError() {
  looping_ = false;
  if (error_callback_) {
    error_callback_();
  }
}


// Assume that the load cell value is linear with weight (which is the whole point right?)
// Calibrate will need to be called with calibration_mass == 0, then again with
// calibration_mass == something non-zero.
int ScaleFilter::Calibrate(double calibration_mass) {
  if (!looping_) {
    printf("Not measuring data, so cannot collect raw data\n");
    return -1;
  }
  int64_t tnow = GetTimeMsec();
  usleep(3000000);
  double average = FilterData(tnow);
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

void ScaleFilter::OnNewMeasurement(uint32_t weight, int64_t tmeas) {
  int64_t tnow = GetTimeMsec();
  // add data to deque
  {
    std::lock_guard<std::mutex> lock(data_lock_);
    weight_data_.push_back(weight);
    time_data_.push_back(tmeas);
    if (weight_data_.size() < kPointsForFiltering)
      return;
  }
  // TODO: maybe this should just be its own thread...
  // If we need to call periodic callback, filter for that reading
  if (periodic_callback_ && tnow - last_periodic_update_  > periodic_update_period_) {
    periodic_callback_(FilterData(0), tmeas); // TODO: should be in the middle of sequence
    last_periodic_update_ = tnow;
  }
  // If we are monitoring for draining, and it has been long enough since we last checked
  if (draining_callback_ && tnow - last_draining_update_  > draining_update_period_) {
    if (CheckDraining()) {
      draining_callback_();
      draining_callback_ = nullptr; // one shot call
    }
    last_draining_update_ = tnow;
  }
  // If we are monitoring for drain complete, and it has been long enough since we last checked
  if (empty_callback_ && tnow - last_empty_update_  > empty_update_period_) {
    if(CheckEmpty()) {
      empty_callback_();
      empty_callback_ = nullptr;  // one shot call
    }
    last_empty_update_ = tnow;
  }
  // If over max storage size, drop earliest
  if (weight_data_.size() > kMaxDataPoints) {
    std::lock_guard<std::mutex> lock(data_lock_);
    weight_data_.pop_front();
    time_data_.pop_front();
  }
}

double ScaleFilter::FilterData(int64_t min_time_bound) {
    std::lock_guard<std::mutex> lock(data_lock_);
  // TODO: explore other filtering methods...
  double wsum = 0;
  int data_points = 0;
  if (weight_data_.size() == 0) {
    return 0.0;
  }
  for (unsigned i = 0; i < weight_data_.size() && i < kPointsForFiltering; ++i) {
    if (time_data_[time_data_.size() - 1 - i] < min_time_bound) {
      break;
    }
    wsum += ToGrams(weight_data_[weight_data_.size() - 1 - i]);
    data_points++;
  }
  return wsum / data_points;
}

// This function has no locking because it is private and only called
// on the same thread that alters the data.
bool ScaleFilter::CheckDraining() {
  if (weight_data_.size() == 0) return false;
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
  SlopeInfo info = FitSlope(weights, times);
  if (info.slope < kDrainingThreshGramsPerSecond &&
      info.ave_diff < kDrainingConfidenceThresh) {
    return true;
  }
  // TODO: also check for longer trends with lower thresholds

  return false;
}

bool ScaleFilter::CheckEmpty() {
  //This is a bit of a hack, but it is for testing...
  if (disable_for_test_) {
    fake_scale_.DrainOut();
  }
  std::lock_guard<std::mutex> lock(data_lock_);
  // TODO: regardless of the weight, check if we are slowing down
  // For now, just check if we are below kettle+trub threshold
  // A +/- 40 gram weight difference doesn't matter - just get last reading
  if (weight_data_.size() == 0) return false;
  return ToGrams(weight_data_.back()) < kEmptyThresholdGrams;
}

ScaleFilter::SlopeInfo ScaleFilter::FitSlope(std::vector<double> weights, std::vector<int64_t> times) {
  // run = (numpy.array(range(window))-numpy.mean(range(window))) / 100.0
  // ...:     s0 = run * (r1 - m1)
  // ...:     s1 = (r1-m1)*(r1-m1)
  // ...:     slope = min(max(sum(s1)/sum(s0),-3000),3000)
  // ...:     # how accurate was the slope:
  // ...:     est=m1 + run*slope
  // ...:     diff = sum(abs(r1-est))
  double wsum = 0 ;
  for (double w : weights) { wsum+=w; }
  double wmean = wsum/weights.size();
  double tsum = 0 ;
  for (auto t : times) { tsum+=t; }
  double tmean = tsum/weights.size();
  double slope_num = 0, slope_denom = 0;
  for (unsigned i = 0; i < weights.size(); ++i) {
    slope_denom += (times[i] - tmean) * (weights[i] - wmean);
    slope_num += (weights[i] - wmean)  * (weights[i] - wmean);
  }
  double slope;
  if (slope_num < 1e-6 && slope_num > -1e-6) {
    slope = 0;
  } else {
    slope = slope_num / slope_denom;
  }
  // some losses are to large to be believed.  If we are truly losing at this rate,
  // it won't matter anyway...
  // slope is in grams(approx ml)/millisecond, so 1L/sec is crazy high
  slope = slope > kMaxDrainSlope ? kMaxDrainSlope : slope;
  slope = slope < -1*kMaxDrainSlope ? -1*kMaxDrainSlope : slope;
  double diff = 0;
  for (unsigned i = 0; i < weights.size(); ++i) {
    // error = reading  - estimate from slope
    double err = weights[i] - (wmean + (times[i] - tmean) * slope);
    diff += err > 0 ? err : -1.0 * err; // abs(err)
  }
  diff /= weights.size();
  SlopeInfo info = {
    .num_points = weights.size(),
    .mean = wmean,
    .slope = slope * 1000, // convert from ms to seconds
    .ave_diff = diff
  };
  return info;
}

