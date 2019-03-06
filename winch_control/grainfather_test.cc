// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "grainfather2.h"
#include <iostream>
int main(int argc, char **argv) {
  
  // const char *tstring = "X63.5,0.0,";
  // double f1, f2;
  // int obj_read = sscanf(tstring, "X%lf,%lf,", &f1, &f2);
  // std::cout << obj_read << "  "  << f1 << "  "<< f2  << std::endl;
  // return 0;

  if (Test_Types()) {
    printf("failed to verify type conversions.\n");
    return -1;
  }
  GrainfatherSerial grainfather;
  grainfather.DisableForTest();
  if(grainfather.Init(nullptr)) {
    printf("failed to init\n");
    return 1;
  }
  grainfather.TestCommands();
  return 0;
}
