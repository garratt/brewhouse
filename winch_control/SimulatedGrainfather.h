// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include "brew_types.h"
#include <vector>

class SimulatedGrainfather {
  BrewState current_state_;
  BrewRecipe recipe_;
  bool waiting_for_mash_start = false;
  bool waiting_for_start_heating = false;
  bool waiting_for_start_sparge = false;
  bool waiting_for_sparge_done = false;
  bool waiting_for_start_boil = false;
  bool waiting_for_boil_done = false;
  double boil_temp_ = 100;
  double sparge_temp_ = 95;

  void TogglePause();
  void Reset();
  void Advance();
  void OnDoneHeating();
  void OnTimerDone();
  bool Update();
  void LoadSession(BrewRecipe recipe);
  void WaitForInput(BrewState::InputReason reason);

  public:
  SimulatedGrainfather() { Reset();}
  void ReceiveSerial(const char *serial_in);

  BrewState ReadState();
};
