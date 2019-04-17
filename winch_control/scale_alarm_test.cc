// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing plan
//
// Initialize the Grainfather, scale, vales
//
// start scale and log raw data to file
// turn on pump with valves set to Kettle
// turn off pump
// turn on pump with vales set to chiller
// turn off pump
//

#include "scale_filter.h"
#include "gpio.h"
#include "valves.h"
#include "grainfather2.h"
#include <iostream>

void PrintWeight(double grams, int64_t wtime) {
  printf("Weight: %4.5lf   time: %ld\n", grams, wtime);
}

bool global_error = false;
void ErrorFunc() {
  printf("Error function called, ending loop!\n");
  global_error=true;
}

int main(int argc, char **argv) {
  InitIO();
   ScaleFilter sf("calibration.txt");
   sf.InitLoop(&ErrorFunc);
   sf.SetPeriodicWeightCallback(1000, &PrintWeight);
   // when actually rnning test, set drain alarm, to make sure it does not trigger.

  GrainfatherSerial grainfather;
  if(grainfather.Init(nullptr)) {
    printf("failed to init\n");
    return -1;
  }
  usleep(1000000 * 10);
  SetFlow(KETTLE);
  if (grainfather.TurnPumpOn()) {
    printf("Failed to Turn pump on!\n");
    return -1;
  }

  usleep(1000000 * 10);
  if (grainfather.TurnPumpOff()) {
    printf("Failed to Turn pump on!\n");
    return -1;
  }

  usleep(1000000 * 10);
  if (grainfather.TurnPumpOn()) {
    printf("Failed to Turn pump on!\n");
    return -1;
  }

  usleep(1000000 * 10);
  SetFlow(NO_PATH);

  usleep(1000000 * 10);
  if (grainfather.TurnPumpOff()) {
    printf("Failed to Turn pump on!\n");
    return -1;
  }

  usleep(1000000 * 10);

  return 0;
}
