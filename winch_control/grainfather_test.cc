// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "grainfather2.h"
#include <iostream>
int main(int argc, char **argv) {
  GrainfatherSerial grainfather;
  // grainfather.DisableForTest();
  if(grainfather.Init(nullptr)) {
    printf("failed to init\n");
    return 1;
  }
  grainfather.TestCommands();
  return 0;
}
