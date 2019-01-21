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

#include "gpio.h"
#include "grainfather.h"
#include "scale.h"
#include "valves.h"
#include "winch.h"
#include "logger.h"



// All waiting involves:
//  - communicating with the scale (check every ms)
//  - a full weigh takes about 3 seconds (30 readings at 10 hz)
//  - reading the scale can take up to 2.5ms, but is usually less than 1ms.
//  - check for beeps (every ms)
//




class BrewManager {
 BeepTracker beep_tracker_;
 BrewLogger brewlogger_;
 WeightFilter weight_filter_;
 enum InterruptTrigger { NONE, LONG_BEEP, CONTINUOUS_BEEP, TIMER, WEIGHT};
 InterruptTrigger interrupt_trigger_;
 double weight_trigger_level_;
 public:
  BrewManager(const char *brew_session) : brewlogger_(brew_session),
                                          weight_filter_("scale_calibration.txt") {}

  // waits for beeps and scale
  // returns 0 if the function stops for the correct reason (trigger event)
  int WaitForInput(uint32_t timeout_sec) {
    time_t begin = time(NULL);
    do {
      usleep(1000);
      // Checks if the speaker is currently signalled.  Records the length
      // of a signal, and returns the beep length when the beep stops.
      // Also flags if the long "mash ready" beep occurrs, or if continous
      // beeping is detected.
      auto beep_status = beep_tracker_.CheckBeep();
      // Checks if new weight is available.  If it is, the weight is
      // read out (takes about 1 ms).  If there are enough readings, the
      // readings are filtered and a weight is produced.
      auto weight_status = weight_filter_.CheckWeight();
      if (HandleStatusUpdate(beep_status, weight_status)) {
        return 0;
      }
    } while (difftime(time(NULL), begin) < timeout_sec);
    // if we were waiting for a fixed amount of time, return 0.
    if (interrupt_trigger_ == InterruptTrigger::TIMER) {
      return 0;
    }
    return 1;
  }
  
  // Returns true if we should transition
  bool HandleStatusUpdate(BeepStatus beep_status, ScaleStatus scale_status) {
    // Log any beep:
    char status_msg[300];
    if (beep_status.state & (BeepStatus::SHORT | BeepStatus::LONG | BeepStatus::CONTINUOUS)) {
      sprintf(status_msg, "Beep for %u ms.", beep_status.length);
      brewlogger_.Log(1, status_msg);
    }
    // Log any weight:
    if (scale_status.state & ScaleStatus::READY) {
      brewlogger_.LogWeight(scale_status.weight);
      if (interrupt_trigger_ == InterruptTrigger::WEIGHT && 
          scale_status.weight < weight_trigger_level_) {
         sprintf(status_msg, "Weight of %f < %f, transition triggered.",
                 scale_status.weight, weight_trigger_level_);
         brewlogger_.Log(1, status_msg);
         return true;
      }
    }
    if ((beep_status.state & BeepStatus::LONG) && interrupt_trigger_ == InterruptTrigger::LONG_BEEP) {
         brewlogger_.Log(1, "Transition triggerd by long beep");
         return true;
    }
    if ((beep_status.state & BeepStatus::CONTINUOUS) &&
        interrupt_trigger_ == InterruptTrigger::CONTINUOUS_BEEP) {
         brewlogger_.Log(1, "Transition triggerd by CONTINUOUS_BEEP");
         return true;
    }
    return false;
  }

  int WaitForMashTemp() {
    interrupt_trigger_ = InterruptTrigger::LONG_BEEP;
    int ret = WaitForInput(60 * 60 * 3);  // Wait for up to 3 hours
    // TODO: Log mash started on overview page
    // This is a good measure of all the water we are mashing with
    // TODO: calculate water volume
    return ret;
  }
  
  int WaitForBeeping(uint32_t minutes) {
    interrupt_trigger_ = InterruptTrigger::CONTINUOUS_BEEP;
    return WaitForInput(60 * minutes);  // 5 hours, including heating time
  }

  int WaitMinutes(uint32_t minutes) {
    interrupt_trigger_ = InterruptTrigger::TIMER;
    return WaitForInput(60 * minutes);
  }

  int WaitForEmpty(double weight, uint32_t minutes) {
    interrupt_trigger_ = InterruptTrigger::WEIGHT;
    weight_trigger_level_ = weight;
    return WaitForInput(60 * minutes);
  }



  int RunBrewSession() {
    if (WaitForMashTemp()) {
      return -1;
    }
    // Wait for mash to complete
    if (WaitForBeeping(5 * 60)) {
      return -1;
    }

    printf("Mash is Done! Lift and let drain\n");
    if(RaiseToDrain() < 0) return -1;

    if (WaitMinutes(30)) return -1;
    // Draining done.

    printf("Skip to Boil\n");
    HitButton(SET_BUTTON);

    if(MoveToSink() < 0) return -1;

    // Wait for beeping, which indicates boil reached
    if (WaitForBeeping(90)) return -1;
    printf("Boil Reached\n");

    if (LowerHops() < 0) return -1;

    HitButton(PUMP_BUTTON);
    if (WaitMinutes(45)) return -1;

    // Run boiling wort through chiller to sterilize it
    SetFlow(CHILLER);

    // TODO: wait for 10 minutes
    if (WaitMinutes(10)) return -1;
    SetFlow(KETTLE);

    // Wait for boil to complete:
    if (WaitForBeeping(60)) return -1;
    printf("Boil Done\n");

    RaiseHops();

    SetFlow(CHILLER);
    ActivateChillerPump();
    HitButton(PUMP_BUTTON);

    printf("Cooling Wort\n");
    // Wait for 20 minutes
    if (WaitMinutes(20)) return -1;

    HitButton(PUMP_BUTTON);
    DeactivateChillerPump();

    printf("Decanting Wort into Carboy\n");
    // Decant:
    SetFlow(CARBOY);
    HitButton(PUMP_BUTTON);
    if (WaitForEmpty(3000, 30)) return -1;
    HitButton(PUMP_BUTTON);
  }


};




#if 0

int WaitForBeeping() {
  BeepTracker bt;
  int ret;
  do {
    usleep(1000);
    auto status = bt.CheckBeep();
    ret = (status.state == BeepStatus::CONTINUOUS);
  } while (ret == 0);
  if (ret > 0) {
    HitButton(SET_BUTTON);
  }
  return ret;
}

void Test_GrainfatherInterface() {
  printf("-------------------------------------------------------------\n"
         "This tests the ability to know what state the Grainfather is in\n"
         "1) Make sure the grainfather is plugged in, including the usb\n"
         "   connector.\n"
         "2) Make sure the outputs of the pump lines are in the kettle or\n"
         "   a carboy, as the pumps will turn on during this test.\n"
         "3) Load the test_brew session on the grainfather app.\n"
         "4) Step through the 'Start Mash' stage.\n"
         "This program will start tracking when the Mash temperature is reached\n"
         "-------------------------------------------------------------\n");
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

#endif

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

  // if (argv[1][0] == 'd') {
    // Test_GrainfatherInterface();
    // return;
  // }

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
    RunManualCalibration(atoi(argv[2]));
    return;
  }

  if (argv[1][0] == 'V') {
    Test_Valves();
  }


  // if (argv[1][0] == 'L') {
    // Test_ListenForBeeps();
    // return;
  // }

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

  if (argv[1][0] == 'l' || argv[1][0] == 'r' || argv[1][0] == 'b') {
    ManualWinchControl(argv[1][0], argv[1][1], ms);
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

}















