// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"


enum FlowPath {NO_PATH, KETTLE, CHILLER, CARBOY};

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


void Test_Valves() {
   SetFlow(NO_PATH);
    HitButton(PUMP_BUTTON);
    SetFlow(KETTLE);
    sleep(10);
    SetFlow(CHILLER);
    sleep(18);
    SetFlow(CARBOY);
    sleep(10);
    SetFlow(NO_PATH);
    HitButton(PUMP_BUTTON);
}