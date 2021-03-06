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

int main(int argc, char **argv) {
   ScaleFilter sf("calibration.txt");
   sf.InitLoop(&ErrorFunc);
   sf.SetPeriodicWeightCallback(1000, &PrintWeight);
 while (!global_error) {
    sleep(10);
 }
  return 0;
}
