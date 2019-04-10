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
#include "scale_filter.h"
#include "valves.h"
#include "winch.h"
#include "logger.h"
#include "grainfather2.h"


int main(int argc, char **argv) {
  if(InitIO() < 0) {
    printf("Failed during initialization. Make sure you can write to all gpios!\n");
  }
  if (argc < 2) return 0;

  GrainfatherSerial gs;
  if (argv[1][0] == 'S') {
    gs.TurnHeatOn();
    usleep(1000000);
    gs.TurnHeatOff();
    return 0;
  }

  if (argv[1][0] == 'P') {
    gs.TurnPumpOn();
    usleep(1000000);
    gs.TurnPumpOff();
    return 0;
  }

  if (argv[1][0] == 'p') {
    ActivateChillerPump();
    usleep(1000000);
    DeactivateChillerPump();
    return 0;
  }

  if (argv[1][0] == 'H') {
    ScaleFilter sf("./calibration.txt");
    printf("Scale Reads %f\n", sf.GetWeight());
    return 0;
  }

  // if (argv[1][0] == 'C') {
    // if (argc < 3) {
      // printf("Calibration needs an input weight for the calibration mass\n");
      // return 0;
    // }
    // RunManualCalibration(atoi(argv[2]));
    // return 0;
  // }

  if (argv[1][0] == 'V') {
    char valve_arg = 'F';
    if (strlen(argv[1]) > 1) {
      valve_arg = argv[1][1];
    }
    Test_Valves(valve_arg);
  }

  WinchController winch_controller_;

  if (argv[1][0] == '1') {
    winch_controller_.RaiseToDrain_1();
    return 0;
  }

  if (argv[1][0] == '2') {
    winch_controller_.RaiseToDrain_2();
    return 0;
  }

  if (argv[1][0] == '3') {
    winch_controller_.MoveToSink();
    return 0;
  }

  if (argv[1][0] == '4') {
    winch_controller_.LowerHops();
    return 0;
  }

  if (argv[1][0] == '5') {
    winch_controller_.RaiseHops();
    return 0;
  }


  if (argv[1][0] == 'i') {
    bool rslide = WinchController::IsRightSlideAtLimit();
    bool lslide = WinchController::IsLeftSlideAtLimit();
    bool top = WinchController::IsTopAtLimit();
    printf("Right Slide %s  | Left Slide %s  | Top switch %s\n",
           rslide ? "ON" : "OFF", lslide ? "ON" : "OFF", top ? "ON" : "OFF");
    return 0;
  }
  // Default to 200ms, which is ~8cm.
  uint32_t ms = 200;  
  if (argc > 2) {
    int ms_in = atoi(argv[2]);
    ms = (ms_in < 0) ? 0 : ms_in;
  }

  if (argv[1][0] == 'l' || argv[1][0] == 'r' || argv[1][0] == 'b') {
    winch_controller_.ManualWinchControl(argv[1][0], argv[1][1], ms);
  }

  return 0;
}


