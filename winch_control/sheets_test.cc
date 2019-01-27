// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brewmanager.h"
int main(int argc, char **argv)
{
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
  return 0;
}
