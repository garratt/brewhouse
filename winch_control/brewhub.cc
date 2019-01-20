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


void WaitMinutes(uint32_t minutes) {
  time_t begin = time(NULL);
  do {
    sleep(10);
  } while (difftime(begin, time(NULL)) < minutes * 60);
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
    Test_GrainfatherInterface();
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
    RunManualCalibration(atoi(argv[2]));
    return;
  }

  if (argv[1][0] == 'V') {
    Test_Valves();
  }


  if (argv[1][0] == 'L') {
    Test_ListenForBeeps();
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

  if (argv[1][0] == 'l' || argv[1][0] == 'r' || argv[1][0] == 'b') {
    ManualWinchControl(argv[1][0], argv[1][1], ms);
  }

}



class BrewManager {
 public:
  void WaitForMashStart() {}
  void WaitForMashComplete() {}
   




};







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



















