// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include <utility>

struct SessionInfo {
  unsigned boil_minutes = 0;
  std::vector<std::pair<double, uint32_t>> mash_steps;
  // std::vector<uint8_t> addition_times;
  std::string session_name;
  double initial_fill = 0, sparge_amount = 0;

  std::string GetSessionCommand() {
    std::string ret;
    char buffer[20];
    snprintf(buffer, 20, "R%u,%u,%2.1f,%2.1f,                  ",
             boil_minutes, mash_steps.size(), initial_fill, sparge_amount);
    ret += buffer;
    // convert name string to all caps
    snprintf(buffer, 20, "%s                           ", session_name.c_str());
    ret += buffer;
    ret += "0,1,1,0,0,         ";
    ret += "0,0,0,0,           "; // second number is number of additions
    // we would put in addition times here, but they don't change the heating
    for (auto step: mash_steps) {
       snprintf(buffer, 20, "%2.1f:%u,                      ", step.first, step.second);
       ret += buffer;
    }
    return ret;
  }
};

struct Weights {
  uint32_t initial_rig, initial_with_water, initial_with_grain;
  uint32_t after_mash, after_lift, after_drain, after_boil;
  uint32_t after_decant;
  void RecordInitWater(uint32_t in) { initial_with_water = in; latest = in; }
  void RecordInitRig(uint32_t in)     { initial_rig = in; latest = in; }
  void RecordInitGrain(uint32_t in)   { initial_with_grain = in; latest = in; }
  void RecordAfterMash(uint32_t in)   { after_mash = in; latest = in; }
  void RecordAfterDrain(uint32_t in)  { after_drain = in; latest = in; }
  void RecordAfterLift(uint32_t in)   { after_lift = in; latest = in; }
  void RecordAfterBoil(uint32_t in)   { after_boil = in; latest = in; }
  void RecordAfterDecant(uint32_t in) { after_decant = in; latest = in; }
  uint32_t latest;
};



class BrewSession {
// session info, shouldn't change:
  unsigned boil_minutes = 0;
  std::vector<std::pair<double, uint32_t>> mash_steps;
  int64_t drain_duration_s;  // loaded from spreadsheet
  // std::vector<uint8_t> addition_times;
  std::string session_name;
  std::string spreadsheet_id;
  double initial_fill = 0, sparge_amount = 0;
  // End info that we send to grainfather
  enum Stage { PREMASH, MASHING, DRAINING, BOILING, CHILLING, DECANTING};
  Stage current_stage;
  // All times are in seconds from epoch
  int64_t brew_start_time = 0, mash_start_time, estimated_mash_end_time;
  int64_t actual_mash_end_time, boil_start_time;
  // All weights are in grams:
  Weights weights;

  using StateTransition = bool (*)(const BrewState&, const BrewState&);
  using TriggerFunc = void (*)(const BrewState&);
  struct ConditionalFunc {
    bool repeat = false;
    StateTransition condition;
    TriggerFunc callback;
  };

  std::list<ConditionalFunc> triggers_;

  int RegisterCallback(void (*callback)(BrewState), StateTransition condition, bool repeat) {
   triggers_.push_back({repeat, condition, callback});
  }

  // signal on transitions
  // Register wait conditions

  int OnChangeState(BrewState new_state, BrewState old_state) {
    for (int i = triggers_.size() - 1; i >= 0; --i) {
      if (triggers_[i].condition(new_state, old_state)) {
        triggers_[i].callback(new_state);
        // if it was a one shot condition, erase after the call
        if (!triggers_[i].repeat) {
          triggers_.erase(triggers_.begin() + i);
        }
      }
    }
  }

  int ParseBrewSheet(std::string sheet_name);
  int LoadTriggers() {}

  int OnMashTemp(BrewState current_state) {
    weights_.RecordInitWater(current_state.weight);
   // tweet readiness
     return 0;
  }

  int OnMashStart(BrewState current_state) {
    weights_.RecordInitGrain(current_state.weight);

  }
   // When mash starts
   // weigh grain bill, tweet
   
  int OnMashComplete(BrewState current_state) {
    grainfather_serial_.TurnPumpOff();
    valves_.CloseAll();
    weights_.RecordAfterMash(current_state.weight);
    // Now we wait for a minute or so to for fluid to drain from hoses
  }

  // Raise a little bit to check that we are not caught
  int OnAfterMashStep2(BrewState current_state) {
    winch_.RaiseToDrain_1();
    // add stage 2 trigger
  }
  // Raise The rest of the way
  int OnAfterMashStep2(BrewState current_state) {
    winch_.RaiseToDrain_2();
    // Add mash step 3 trigger
  }

  // After weight has settled from lifting the mash out
  int OnAfterMashStep3(BrewState current_state) {
    weights_.RecordAfterLift(current_state.weight);
  }

  int OnDrainComplete(BrewState current_state) {
    weights_.RecordAfterDrain(current_state.weight);
    winch_.MoveToSink();
    grainfather_serial_.AdvanceStage();
  }


   // When Mash complete
   // Pump Off, valves closed
   // measure after_mash_weight, tweet loss
   // wait for minute, raise
   // wait 45 minutes
   // move to sink
   // advance to boil
   
  int OnBoilTemp(BrewState current_state) {
    winch_.LowerHops();
    valves_.OpenKettle();
    grainfather_serial_.AdvanceStage();
    grainfather_serial_.TurnPumpOn();
  }
   // When Boil Temp Reached
   // Lower Hops
   // Advance
   // Ensure Pump on
   
  int OnBoilDone(BrewState current_state) {
    grainfather_serial_.AdvanceStage();
    grainfather_serial_.TurnPumpOff();
    valves_.CloseAll();
  }
   // When Boil is done
   // Advance
   // Pump Off, valves closed
   
  int OnPostBoil1Min(BrewState current_state) {
    winch_.RaiseHops()
    weights_.RecordAfterBoil();
  }
   // 1 minute after boil off
   // Raise Hops
   
  int OnPostBoil5Min(BrewState current_state) {

  }
   // 5 minutes after boil
   // take weight,
   // Set flow through chiller
   // Pump on
   // Chiller pump on


  int OnDecantFinish(BrewState current_state) {

  }
   // When Empty:
   // Stop pump, valves
   // Chiller Pump off
   
   
  int OnRapidWeightLoss(BrewState current_state) {

  }
   // if weight decreases rapidly (when not decanting)
   // Stop pump, valves
   // pause session
   
   
  int OnGrainfatherPickup(BrewState current_state) {

  }
   // if weight goes below empty grainfather
   // Stop session
  }



};



