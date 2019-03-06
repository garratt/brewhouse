// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "valves.h"


int SetFlow(FlowPath path) {
  // Set up the valve config.  The output is active low
  SetOutput(KETTLE_VALVE, (path != KETTLE));
  SetOutput(CARBOY_VALVE, (path != CARBOY));
  SetOutput(CHILLER_VALVE, (path != CHILLER));
  SetOutput(VALVE_ENABLE, 0);
  // The valve is guarenteed to finish in 5 seconds
  sleep(5);
  // disallow movement, then reset the relays:
  SetOutput(VALVE_ENABLE, 1);
  SetOutput(KETTLE_VALVE, 1);
  SetOutput(CARBOY_VALVE, 1);
  SetOutput(CHILLER_VALVE, 1);
  return 0;
}


int ActivateChillerPump() {
  return SetOutput(CHILLER_PUMP, 0);
}
int DeactivateChillerPump() {
  return SetOutput(CHILLER_PUMP, 1);
}


void Test_Valves(char valve_arg) {
  switch (valve_arg) {
    case 'F':
      SetFlow(NO_PATH);
      SetFlow(KETTLE);
      sleep(10);
      SetFlow(CHILLER);
      sleep(18);
      SetFlow(CARBOY);
      sleep(10);
      SetFlow(NO_PATH);
      break;
    case 'K':
      SetFlow(KETTLE);
      break;
    case 'C':
      SetFlow(CHILLER);
      break;
    case 'B':
      SetFlow(CARBOY);
      break;
    case 'N':
      SetFlow(NO_PATH);
      break;
  }
}
