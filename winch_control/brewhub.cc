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


#include "brew_session.h"

// All waiting involves:
//  - communicating with the scale (check every ms)
//  - a full weigh takes about 3 seconds (30 readings at 10 hz)
//  - reading the scale can take up to 2.5ms, but is usually less than 1ms.
//  - check for beeps (every ms)
//

// Manages the physical properties of the brewhub setup.
class GrainfatherSetup {
 static constexpr double kGrainfatherKettle_grams = 2000; // TODO: determine this!
 static constexpr double kMashTun_grams = 1000; // TODO: determine this!
 public:
   double GetVolumeL(double weight_grams, bool has_tun, double specific_gravity = 1.00) {
     double tare = kGrainfatherKettle_grams + has_tun ? kMashTun_grams : 0;
     return (weight_grams - tare) / (1000.0 * specific_gravity);  // Kg to Liters is easy
   }
   // TODO: put dimentions of the setup for estimating winch times.
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

  if (argv[1][0] == 'p') {
    ActivateChillerPump();
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
    char valve_arg = 'F';
    if (strlen(argv[1]) > 1) {
      valve_arg = argv[1][1];
    }
    Test_Valves(valve_arg);
  }


  if (argv[1][0] == 'L') {
    BeepTracker bt;
    bt.Test_ListenForBeeps();
    return;
  }

  if (argv[1][0] == '1') {
    winch::RaiseToDrain_1();
    return;
  }

  if (argv[1][0] == '2') {
    winch::RaiseToDrain_2();
    return;
  }

  if (argv[1][0] == '3') {
    winch::MoveToSink();
    return;
  }

  if (argv[1][0] == '4') {
    winch::LowerHops();
    return;
  }

  if (argv[1][0] == '5') {
    winch::RaiseHops();
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
    winch::ManualWinchControl(argv[1][0], argv[1][1], ms);
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















