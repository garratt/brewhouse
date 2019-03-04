// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio.h"
#include "scale.h"


int WaitForHX711(uint8_t data_pin, uint8_t sclk) {
  uint32_t ret = 0;
  // The HX711 communicates by pulling the data line low every N Hz.
  // Wait for the Data line to be pulled low:
  int valid_count = 0;
  do {
    // The time between conversions is 100ms (at 10 hz), and data won't come
    // until we strobe the clock, so we can poll pretty infrequently...
    usleep(1000);
    int rval = ReadInput(data_pin);
    if (rval < 0) {
      printf("Failed to read data input\n");
      return rval;
    }
    if (rval) {  // the signal is active low, so we count the # of 0 readings
      valid_count = 0;
    } else {
      valid_count++;
    }
  } while (valid_count < 10 );
  return 0;
}

double ReadHX711Data(uint8_t data_pin, uint8_t sclk) {
  // We need to toggle the sclk fairly fast. If the Sclk stays high longer than
  // 60 us, the chip powers down.  Since we don't have great low level hardware
  // control, we just have to take few instructions, and accept that we may get preempted.
  // TODO: use the last bit to verify that we have not reset.
  // int data_pin_fd = open(gpio_val(data_pin), O_RDWR);
  // if (!data_pin_fd) {
    // printf("Failed to open %s\n", gpio_val(data_pin));
    // return -1;
  // }
  std::string sclk_path = gpio_val_path(sclk);
  int sclk_fd = open(sclk_path.c_str(), O_RDWR);
  if (!sclk_fd) {
    printf("Failed to open %s\n", sclk_path.c_str());
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
      printf("ReadHX711Data: Failed to read %s\n", gpio_val_path(data_pin).c_str());
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




bool WeightLimiter::PublishWeight(double weight_in, double *weight_out, time_t *log_time) {
  // If it is the first weight, publish it and save it.
  time_t now = time(NULL);
  if (count_ == 0) {
    last_log_ = now;
    count_ = 1;
    sum_ = weight_in;
  }
  double average = sum_ / count_;
  double dev = average > weight_in ? average - weight_in : weight_in - average;
  if (dev > kMaxDeviationGrams || difftime(now, last_log_) > kMaxTimeJumpSeconds
      || difftime(now, first_log_) > kLoggingPeriodSeconds) {
    // We always report the previous readings, and leave our current reading
    *weight_out = average;
    *log_time = last_log_;
    last_log_ = now;
    first_log_ = now;
    count_ = 1;
    sum_ = weight_in;
    return true;
  }
  // Otherwise, just record the reading
  count_++;
  sum_ += weight_in;
  return false;
}



int WeightFilter::CollectRawData(int num_readings) {
  raw_data_.clear();
  // excluded_.clear();
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
      // excluded_.push_back(false);
    }
  }
  // sigma_.resize(raw_data_.size());
  return 0;
}

void WeightFilter::CalculateStats() {
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

int WeightFilter::FindMaxSigma() {
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


// Initialize with calibration.  Creates file otherwise, and writes to it on
// calls to Calibrate()
WeightFilter::WeightFilter(const char *calibration_file) : calibration_file_(calibration_file) {
  std::fstream calfile;
  calfile.open(calibration_file, std::fstream::in);
  if(!calfile.is_open()) {
    printf("No Calfile at %s\n", calibration_file_);
    return;
  }
  calfile >> offset_ >> scale_;
  calfile.close();
}

// Checks if everything is fine:
//  - We have calibration loaded
//  - We are reading the sensor
//  - We are reading normal values
int WeightFilter::CheckScale() {
   // Check calibration:
   // Even if offset and scale were small, there is a very low chance
   // that they would actually both be zero.
   if (offset_ == 0 && scale_ == 0) {
     printf("Calibration not loaded!\n");
     return -1;
   }
   // Gather weight data:
   ScaleStatus status = GetWeight(false);
   if (status.state != ScaleStatus::READY) {
     printf("Error reading Scale!\n");
     return -1;
   }
   if (status.weight < kMinNormalReadingGrams ||
       status.weight > kMaxNormalReadingGrams) {
     printf("Scale is reading %f, which is out of normal range!\n");
     return -1;
   }
   // Okay, scale passes all the checks. Start the loop!
   return 0;
}


void WeightFilter::InitLoop(std::function<void(double)> callback) {
  weight_callback_ = callback;
  reading_thread_enabled_ = true;
  reading_thread_ = std::thread(&WeightFilter::ReadingThread, this);
}

WeightFilter::~WeightFilter() {
  reading_thread_enabled_ = false;
  reading_thread_.join();
}


double WeightFilter::RemoveOutlierData() {
  excluded_.clear();
  excluded_.resize(raw_data_.size(), false);
  sigma_.resize(raw_data_.size());
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
      if(!excluded_[i] && verbose_)
        printf("%02d raw: %i  sigma %8.4f    ave: %8.4f   ave sigma:  %8.4f  dev: %8.4f %s\n", i,
            raw_data_[i], sigma_[i], average_, average_sigma_, sigma_[i] / average_sigma_,
            excluded_[i] ? "    (Excluded)" : "");
      if (!excluded_[i]) unexcluded_count++;
    }
    if (verbose_)
      printf("  Excluding %d\n", index);
    CalculateStats();

  } while (average_sigma_ > 100000 || dev > 100000);
  // Kick out any point with sigma > 3* average sigma
  // and by kick out, I mean don't average them into the return value.

  if (unexcluded_count == 0) {
    printf("No non outliers.  This shouldn't be possible...\n");
    raw_data_.clear();
    return kErrorSentinelValue;
  }

  raw_data_.clear();
  return average_;
}


// Assume that the load cell value is linear with weight (which is the whole point right?)
// Calibrate will need to be called with calibration_mass == 0, then again with
// calibration_mass == something non-zero.
int WeightFilter::Calibrate(double calibration_mass) {
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

ScaleStatus FakeScale::GetWeight() {
  usleep(2000000);
  Update();
  ScaleStatus status;
  status.state = ScaleStatus::READY;
  status.weight = current_weight_;
  return status;
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
ScaleStatus WeightFilter::GetWeight(bool verbose) {
  if (disable_for_test_) {
    return fake_scale_.GetWeight();
  }
  ScaleStatus status;
  verbose_ = verbose;
  if(CollectRawData(kNumberOfReadings)) {
    printf("Failed to collect raw data\n");
    status.state = ScaleStatus::ERROR;
    return status;
  }
  double average = RemoveOutlierData();
  printf("average: %f   ", average);
  status.state = ScaleStatus::READY;
  status.weight = scale_ * (average - offset_);
  return status;
}

ScaleStatus FakeScale::CheckWeight() {
  ScaleStatus status;
  int64_t tnow = GetTimeMsec();
  if (tnow - last_time_ < 2000) {
    status.state = ScaleStatus::NONE;
    return status;
  }
  Update();
  status.state = ScaleStatus::READY;
  status.weight = current_weight_;
  return status;
}


// Checks if new weight is available.  If it is, the weight is
// read out (takes about 1 ms).  If there are enough readings, the
// readings are filtered and a weight is produced.
ScaleStatus WeightFilter::CheckWeight() {
  if (disable_for_test_) {
    return fake_scale_.CheckWeight();
  }
  ScaleStatus status;
  int val = ReadInput(SCALE_DATA);
  if (val < 0) {
    status.state = ScaleStatus::ERROR;
    return status;
  }
  if (val > 0){
    status.state = ScaleStatus::NONE;
    return status;
  }
  // else, val == 0, so speaker is on (active low)
  // Read the scale data
  val = ReadHX711Data();
  if (val < 0) {
    printf("Error Reading Scale!\n");
    status.state = ScaleStatus::ERROR;
    return status;
  }
  // Now, process the data.
  // Wait for kNumberOfReadings.
  // clears if it has been more than 2 seconds since we got the last reading.
  // at two seconds, this makes the whole sampling time up to 1 minute.
  // But since the sensor gives readings at 10Hz, normal sampling period is
  // 3 seconds.
  if (difftime(time(NULL), last_data_) > 2) {
    raw_data_.clear();
  }
  last_data_ = time(NULL);
  raw_data_.push_back(val);
  if (raw_data_.size() < kNumberOfReadings) {
    status.state = ScaleStatus::NONE;
    return status;
  }

  // We have enough recent data, filter and return the reading
  double average = RemoveOutlierData();
  status.weight =  scale_ * (average - offset_);
  status.state = ScaleStatus::READY;
  return status;
}


void WeightFilter::ReadingThread() {
  while (reading_thread_enabled_) {
    ScaleStatus status = CheckWeight();
    if (status.state == ScaleStatus::READY && weight_callback_) {
      weight_callback_(status.weight);
    } else {
      usleep(10000); // Check every 10ms
    }
  }
}

void RunManualCalibration(int calibration_mass, const char *calibration_file) {
    WeightFilter wf(calibration_file);
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
       ScaleStatus status = wf.GetWeight(false);
       if (status.state == ScaleStatus::READY){
         printf("Scale Reads %f\n", status.weight);
       } else {
         printf("Error reading scale.\n");
       }
    }
}
