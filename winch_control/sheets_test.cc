// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brewmanager.h"
#include "grainfather2.h"
int main(int argc, char **argv) {

  BrewLogger brew_logger("a test session");
  brew_logger.ReadNewSession();
  // brew_logger.GetValues("Overview!G5:G9");

#if 0
  if (argc < 2) {
    printf("Must specify brew session name!\n");
    return -1;
  }
  InitIO();
  BrewManager brewmanager(argv[1]);
  // SetFlow(CARBOY);
  // HitButton(PUMP_BUTTON);
  // brewmanager.WaitForEmpty(9000, 20);
  // HitButton(PUMP_BUTTON);

  brewmanager.RunBrewSession();
#endif
  return 0;
}
