// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Pinout describes how relays are connected to the up board
// The numbers represent the (linux) gpio pin

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LEFT_WINCH_ENABLE 5
#define LEFT_WINCH_DIRECTION 6
#define RIGHT_WINCH_ENABLE 2
#define RIGHT_WINCH_DIRECTION 3

#define SET_BUTTON 25
#define PUMP_BUTTON 8
#define SPEAKER_IN 1

#define RIGHT_SLIDE_SWITCH 21

#define CHILLER_PUMP 14
#define VALVE_ENABLE 15
#define CARBOY_VALVE 18
#define CHILLER_VALVE 23
#define KETTLE_VALVE 24


#define SCALE_DATA 12
#define SCALE_SCLK 20

// #define str(s) #s
// #define gpio_dir(pin) "/sys/class/gpio/gpio" str(pin) "/direction"
// #define gpio_val(pin) "/sys/class/gpio/gpio" str(pin) "/value"

char path_buffer[50];

char *gpio_val(uint8_t pin) {
   sprintf(path_buffer, "/sys/class/gpio/gpio%u/value", pin);
   return path_buffer;
}

char *gpio_dir(uint8_t pin) {
   sprintf(path_buffer, "/sys/class/gpio/gpio%u/direction", pin);
   return path_buffer;
}

int SetOutput(uint8_t pin, uint8_t value) {
   int fd = open(gpio_val(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   write(fd, value ? "1" : "0", 1);
   close(fd);
   return 0;
}


int SetOpenDrain(uint8_t pin, uint8_t value) {
   int fd = open(gpio_dir(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   write(fd, value ? "low" : "in", 2 + value);
   close(fd);
   return 0;
}

int SetDirection(uint8_t pin, uint8_t direction, uint8_t value = 0) {
   int fd = open(gpio_dir(pin), O_RDWR);
   if (!fd) {
     printf("Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   if (direction > 0) { // output
     if (value > 0) {
       write(fd, "high", 4);
     } else {
       write(fd, "low", 3);
     }
   } else {
     write(fd, "in", 2);
   }
   close(fd);
   return 0;
}

int ReadInput(uint8_t pin) {
   int fd = open(gpio_val(pin), O_RDWR);
   if (!fd) {
     printf("ReadInput: Failed to open %s\n", gpio_val(pin));
     return -1;
   }
   char buffer;
   int bytes = read(fd, &buffer, 1);
   if (bytes == 0) {
     printf("ReadInput: Failed to read %s\n", gpio_val(pin));
     close(fd);
     return -1;
   }
   close(fd);
   if (buffer == '0') {
     return 0;
   }
   return 1;
}

enum FlowPath {NO_PATH, KETTLE, CHILLER, CARBOY};

int SetFlow(FlowPath path) {
  // Set up the valve config.  The output is active low
  SetOutput(KETTLE_VALVE, (path != KETTLE));
  SetOutput(CARBOY_VALVE, (path != CARBOY));
  SetOutput(CHILLER_VALVE, (path != CHILLER));
  SetOutput(VALVE_ENABLE, 0);
  // The valve is guarenteed to finish in 5 seconds
  sleep(5);
  // disallow movement, then reset the relays:
  SetOutput(VALVE_ENABLE, 1);
  SetOutput(KETTLE_VALVE, 1);
  SetOutput(CARBOY_VALVE, 1);
  SetOutput(CHILLER_VALVE, 1);
  return 0;
}

int InitIO() {
  if (SetDirection(LEFT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_ENABLE, 1, 0)) return -1;
  if (SetDirection(LEFT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(RIGHT_WINCH_DIRECTION, 1, 0)) return -1;
  if (SetDirection(CHILLER_PUMP, 1, 1)) return -1;
  if (SetDirection(VALVE_ENABLE, 1, 1)) return -1;
  if (SetDirection(CARBOY_VALVE, 1, 1)) return -1;
  if (SetDirection(CHILLER_VALVE, 1, 1)) return -1;
  if (SetDirection(KETTLE_VALVE, 1, 1)) return -1;
  if (SetDirection(SET_BUTTON, 0)) return -1;
  if (SetDirection(PUMP_BUTTON, 0)) return -1;
  if (SetDirection(SPEAKER_IN, 0)) return -1;
  if (SetDirection(RIGHT_SLIDE_SWITCH, 0)) return -1;
  if (SetDirection(SCALE_DATA, 0)) return -1;
  if (SetDirection(SCALE_SCLK, 1, 0)) return -1;
  // SetFlow(NO_PATH);
  return 0;
}


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
  // return double(66702942 - ret) * 0.000026652;
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

  int CollectRawData(int num_readings) {
    raw_data_.clear();
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
        }
      }
      return 0;
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

    double total = 0;
    for (double d : raw_data_) {
      total+=d;
    }
    double average = total / raw_data_.size();
    // Now calculate sigma:
    std::vector<double> sigmas;
    double total_sigmas = 0;
    for (double d : raw_data_) {
      sigmas.push_back((d-average) * (d - average));
      total_sigmas += sigmas.back();
    }
    double average_sigma = total_sigmas / raw_data_.size();
    // Kick out any point with sigma > 3* average sigma
    // and by kick out, I mean don't average them into the return value.
    double ret = 0;
    int averaging_count = 0;
    for (unsigned int i = 0; i < raw_data_.size(); ++i) {
      printf("raw: %i  sigma %8.4f    ave: %8.4f   ave sigma:  %8.4f  dev: %8.4f",
             raw_data_[i], sigmas[i], average, average_sigma, sigmas[i] / average_sigma);
      if (sigmas[i] < 1.2 * average_sigma) {
        ret += raw_data_[i];
        averaging_count++;
        printf("\n");
      } else {
        printf("  Excluded!\n");
      }
    }
    if (averaging_count == 0) {
      printf("No non outliers.  This shouldn't be possible...\n");
      return kErrorSentinelValue;
    }
    return ret / averaging_count;
  }


    // 1000  540  291600
    // 1000  540
    // 100   360  129600
    // 100   360
    // 100   360
    // total: 2300, average = 460
    // ave sigma: 194400

    // 1000  720 518400
    // 100  
    // 100   180  32400
    // 100   
    // 100   
    // total: 1400, average = 280
    // ave sigma: 194400

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

#define RIGHT 0
#define LEFT 1
// down is 0
// Reel the winch in, running for |ms| milliseconds.
// the serial number can be specified using serial.
int RunWinch(uint32_t ms, int side, int direction) {
   int enable = (side == RIGHT) ? RIGHT_WINCH_ENABLE : LEFT_WINCH_ENABLE;
   int dir = (side == RIGHT) ? RIGHT_WINCH_DIRECTION : LEFT_WINCH_DIRECTION;
   if(SetOutput(dir, direction)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(enable, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(1000 * ms); // run for given amount of time
   if (SetOutput(enable, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(dir, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   return 0;
}

// returns -1 if cannot communicate
// 0 if not stopped
// 1 if stopped (the limit switch is triggered)
int IsRightSlideStop() {
  // Active low (for now)
  int val = ReadInput(RIGHT_SLIDE_SWITCH);
  if (val < 0) return val;
  if (val == 0) return 1;
  return 0;
}


//Run both winches:
// Direction: Right: 0 Left: 1
int RunBothWinches(uint32_t ms, int direction) {
  //if direction == right, left down, right up
  int ldir = direction ? 1 : 0;
  int rdir = direction? 0 : 1;
   if(SetOutput(RIGHT_WINCH_DIRECTION, rdir)) {
     printf("Failed to set output\n");
     return -1;
   }
   if(SetOutput(LEFT_WINCH_DIRECTION, ldir)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   if (SetOutput(RIGHT_WINCH_ENABLE, 1) || SetOutput(LEFT_WINCH_ENABLE, 1)) {
     printf("Failed to set output\n");
     return -1;
   }
   // TODO: if moving right, stop at the right stop
   // Check every ms.
   uint32_t ms_counter = 0;
   do {
     usleep(1000);
     ms_counter++;
     if (direction == 0 && IsRightSlideStop()) {
       break;
     }
   } while (ms_counter <= ms);

   if (SetOutput(RIGHT_WINCH_ENABLE, 0) || SetOutput(LEFT_WINCH_ENABLE, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   if (SetOutput(LEFT_WINCH_DIRECTION, 0) || SetOutput(RIGHT_WINCH_DIRECTION, 0)) {
     printf("Failed to set output\n");
     return -1;
   }
   usleep(100000); // let the relay click around
   return 0;
}


int LeftGoUp(uint32_t ms) {    return RunWinch(ms, 1, 1); }
int LeftGoDown(uint32_t ms) {  return RunWinch(ms, 1, 0); }
int RightGoUp(uint32_t ms) {   return RunWinch(ms, 0, 1); }
int RightGoDown(uint32_t ms) { return RunWinch(ms, 0, 0); }
int GoLeft(uint32_t ms) { return RunBothWinches(ms, 1); }
int GoRight(uint32_t ms) { return RunBothWinches(ms, 0); }

int RaiseToDrain() {
  // Assumes we are in the mash state
  if (RightGoUp(2500)) { // go up until we hit the limit
    return -1;
  }
  if (RightGoDown(200)) {
    return -1;
  }
  return 0;
}

int MoveToSink() {
  // Raise to limit:
  RightGoUp(300);
  // Lower a little:
  RightGoDown(100);
  // Now scoot over using both winches:
  GoRight(3000); // This will quit early when it hits the limit switch


  // uint32_t step_size = 75;
  // int val;
  // do {
    // if (LeftGoDown(step_size)) { return -1; }
    // if (RightGoUp(step_size * 1.1)) { return -1; }
    // usleep(500000); // let it settle?  I'd like to get it to not swing so much...
    // read the Right slide switch to see if we are done:
    // val = ReadInput(RIGHT_SLIDE_SWITCH);
    // if (step_size < 200) {
      // step_size += 25;
    // }
  // } while (val > 0);
  // if (val < 0) {
    // return -1;
  // }

  // Now we are over the sink, lower away!
  return RightGoDown(3500);
}

int LowerHops() {
  return LeftGoDown(3000);
}

int RaiseHops() {
  return LeftGoUp(2500);  // go up a little less than we went down
}


int ActivateChillerPump() {
  return SetOutput(CHILLER_PUMP, 0);
}
int DeactivateChillerPump() {
  return SetOutput(CHILLER_PUMP, 1);
}

int HitButton(uint8_t button) {
  if (SetOpenDrain(button, 1)) {
    printf("Failed to set open drain output\n");
    return -1;
  }
  usleep(100000); // let the button click register
  if (SetOpenDrain(button, 0)) {
    printf("Failed to set output\n");
    return -1;
  }
  usleep(100000); // make sure we don't do anything else right after
  return 0;
}

class BeepTracker {
  uint64_t start_ = 0, stop_ = 0, prev_start_ = 0, prev_stop_ = 0;
  int prev_val_ = 1;
  uint64_t initial_seconds_;
  // get milliseconds since this object was initialized.
  // This time should only be used for diffs
  uint64_t GetTime() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t ret = t.tv_sec - initial_seconds_;
    ret *= 1000;
    ret += t.tv_nsec / 1000000;
    return ret;
  }

  bool IsClose(uint64_t val, uint64_t target) {
    uint64_t diff = val > target? val - target: target - val;
    if (diff < target / 10) return true;
    return false;
  }

  public:
    int CheckBeep() {
      int val = ReadInput(SPEAKER_IN);
      if (val < 0) { return -1; }
      // just turned on:
      int change = val - prev_val_;
      prev_val_ = val;
      if (change < 0) {
        prev_start_ = start_;
        start_ = GetTime();
      }
      // Just turned off:
      if (change > 0) {
        prev_stop_ = stop_;
        stop_ = GetTime();
        return stop_ - start_;
      }
      return 0;
    }

  BeepTracker() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    initial_seconds_ = t.tv_sec;
  }

    int CheckContinous() {
      int val = CheckBeep();
      if (val <= 0) return val;
      // val > 0.
      // Critera for Continous beep: 500ms on, 500ms off, 500ms on
      if (!IsClose(stop_ - start_, 500)) return 0;
      if (!IsClose(start_ - prev_stop_, 500)) return 0;
      if (!IsClose(prev_stop_ - prev_start_, 500)) return 0;
      return 1;
    }

    int CheckLongBeep() {
      int val = CheckBeep();
      if (val <= 0) return val;
      if (IsClose(stop_ - start_, 1500)) {
        return 1;
      }
      return 0;
    }
};


int WaitForMashStart() {
  // Wait for Long beep
  BeepTracker bt;
  int ret;
  do {
    usleep(1000);
    ret = bt.CheckLongBeep();
  } while (ret == 0);
  return ret;
  // TODO: Then wait for the set button to be pushed
}


int WaitForBeeping() {
  BeepTracker bt;
  int ret;
  do {
    usleep(1000);
    ret = bt.CheckContinous();
  } while (ret == 0);
  if (ret > 0) {
    HitButton(SET_BUTTON);
  }
  return ret;
}

void ListenForBeeps() {
  BeepTracker bt;
  int ret;
  while(1) {
    usleep(1000);
    ret = bt.CheckBeep();
    if (ret > 0) {
      printf("beep: %d\n", ret);
    }
  }
}

void WaitMinutes(uint32_t minutes) {
  time_t begin = time(NULL);
  do {
    sleep(10);
  } while (difftime(begin, time(NULL)) < minutes * 60);
}

void DryRun() {
  printf("Waiting for mash to start...\n");
  WaitForMashStart();
  printf("Mash at Temp. Waiting for Mash to Finish.\n");
  // TODO: time how long mash takes

  // Wait for Mash to be done
  WaitForBeeping();
  printf("Mash is Done! Lift and let drain\n");
  sleep(1);
  printf("Skip to Boil\n");
  HitButton(SET_BUTTON);
  // wait for boil temp to be reached
  
  WaitForBeeping();
  printf("Boil Reached\n");
  HitButton(SET_BUTTON);
  // Wait for Boil to be done
  WaitForBeeping();
  printf("Boil Done\n");
}


void RunTestCommand(int argc, char **argv) {
  if (argc < 2) return;

  if (argv[1][0] == 'S') {
    HitButton(SET_BUTTON);
    return;
  }

  if (argv[1][0] == 'P') {
    HitButton(PUMP_BUTTON);
    return;
  }

  if (argv[1][0] == 'd') {
    DryRun();
    return;
  }

  if (argv[1][0] == 'H') {
    WeightFilter wf("./calibration.txt");
    double val = wf.GetWeight(false);
    printf("Scale Reads %f\n", val);
    return;
  }
  
  if (argv[1][0] == 'C') {
    if (argc < 3) {
      printf("Calibration needs an input weight for the calibration mass\n");
      return;
    }
    int calibration_mass = atoi(argv[2]);
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
    return;
  }

  if (argv[1][0] == 'V') {
    HitButton(PUMP_BUTTON);
    SetFlow(KETTLE);
    sleep(10);
    SetFlow(CHILLER);
    sleep(18);
    SetFlow(CARBOY);
    sleep(10);
    SetFlow(NO_PATH);
    HitButton(PUMP_BUTTON);
    return;
  }


  if (argv[1][0] == 'L') {
    ListenForBeeps();
    return;
  }

  if (argv[1][0] == '1') {
    RaiseToDrain();
    return;
  }

  if (argv[1][0] == '2') {
    MoveToSink();
    return;
  }

  if (argv[1][0] == '3') {
    LowerHops();
    return;
  }

  if (argv[1][0] == '4') {
    RaiseHops();
    return;
  }


  if (argv[1][0] == 'i') {
    int slide = ReadInput(RIGHT_SLIDE_SWITCH);
    for(int i = 0; i < 5; ++i) {
      printf("Right Slide Switch: %d\n", slide);
      usleep(1001);
    }
    return;
  }
  // Default to 200ms, which is ~8cm.
  uint32_t ms = 200;  
  if (argc > 2) {
    int ms_in = atoi(argv[2]);
    ms = (ms_in < 0) ? 0 : ms_in;
  }


  if (argv[1][0] == 'l' && argv[1][1] == 'u') {
    LeftGoUp(ms);
  }

  if (argv[1][0] == 'l' && argv[1][1] == 'd') {
    printf("Left doing down\n");
    LeftGoDown(ms);
  }

  if (argv[1][0] == 'r' && argv[1][1] == 'u') {
    RightGoUp(ms);
  }

  if (argv[1][0] == 'r' && argv[1][1] == 'd') {
    RightGoDown(ms);
  }

  if (argv[1][0] == 'b' && argv[1][1] == 'l') {
    RunBothWinches(ms, 1);
  }

  if (argv[1][0] == 'b' && argv[1][1] == 'r') {
    RunBothWinches(ms, 0);
  }

}

int main(int argc, char **argv) {
  if(InitIO() < 0) {
    printf("Failed during initialization. Make sure you can write to all gpios!\n");
  }

  if (argc > 1) {
    RunTestCommand(argc, argv);
    return 0;
  }
  bool do_waits = false;

  printf("Waiting for mash to start...\n");
  if (WaitForMashStart() < 0) return -1;
  printf("Mash at Temp. Waiting for Mash to Finish.\n");
  // TODO: time how long mash takes

  // Wait for Mash to be done
  if (WaitForBeeping() < 0) return -1;
  
  printf("Mash is Done! Lift and let drain\n");
  if(RaiseToDrain() < 0) return -1;
  if (do_waits) WaitMinutes(30);
  // Draining done.
  
  printf("Skip to Boil\n");
  HitButton(SET_BUTTON);

  if(MoveToSink() < 0) return -1;

  // Wait for beeping, which indicates boil reached
  if (WaitForBeeping() < 0) return -1;
  printf("Boil Reached\n");

  if (LowerHops() < 0) return -1;

  HitButton(PUMP_BUTTON);
  if (do_waits) WaitMinutes(45);

  SetFlow(CHILLER);

  // TODO: wait for 10 minutes
  if (do_waits) WaitMinutes(10);

  SetFlow(CARBOY);
  //Wait for 2 minutes
  if (do_waits) WaitMinutes(2);

  SetFlow(KETTLE);

  // Wait for boil to complete:
  WaitForBeeping();
  printf("Boil Done\n");

  RaiseHops();

  SetFlow(CHILLER);
  ActivateChillerPump();
  HitButton(PUMP_BUTTON);

  printf("Cooling Wort\n");
  // Wait for 20 minutes
  if (do_waits) WaitMinutes(20);

  HitButton(PUMP_BUTTON);
  DeactivateChillerPump();
  // Wait for a minute
  if (do_waits) WaitMinutes(1);

  printf("Decanting Wort into Carboy\n");
  // Decant:
  SetFlow(CARBOY);
  HitButton(PUMP_BUTTON);
  if (do_waits) WaitMinutes(20);
  // Wait for 20 minutes
  // TODO: Yikes!  Need a way to know when we are done!
  HitButton(PUMP_BUTTON);
  return -1;
}



















