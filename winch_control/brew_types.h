// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>
#include <deque>
#include <vector>
#include <mutex>

#pragma once

#define BREW_WARNING -1
#define BREW_ERROR -2
#define BREW_FATAL -3

struct BrewRecipe {
  std::string session_name;
  std::vector<double> mash_temps;
  std::vector<uint32_t> mash_times;
  unsigned boil_minutes = 0;
  double grain_weight_grams;
  double hops_grams;
  std::string hops_type;
  double initial_volume_liters = 0, sparge_liters = 0;

  void Print();

  // This creates the string which is passed to the Grainfather to
  // Load a session
  std::string GetSessionCommand();

  // Load from a serialized session command
  int Load(const std::string &in);

  // just check the stuff that gets loaded
  bool operator==(const BrewRecipe &other);

};


struct BrewState {
  int64_t read_time;
  bool timer_on, timer_paused;
  uint32_t timer_seconds_left;
  uint32_t timer_total_seconds;
  bool waiting_for_input, waiting_for_temp;
  bool brew_session_loaded;
  bool heater_on, pump_on;
  double current_temp, target_temp, percent_heating;
  uint8_t stage, substage;
  bool valid = false;

  bool operator!=(const BrewState& other);

  // Serialize to string what would get sent from the grainfather
  // For testing and faking purposes
  std::string ToString();

  // de-serialize the state from what would be read from the grainfather.
  int Load(std::string in);

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

struct Times {
  int64_t brew_start_time = 0, mash_start_time = 0;
  int64_t mash_end_time = 0, boil_start_time = 0;
  void RecordBrewStart(int64_t t) { brew_start_time = t; }
  void RecordMashStart(int64_t t) { mash_start_time = t; }
  void RecordMashEnd(int64_t t)   { mash_end_time = t; }
  void RecordBoilStart(int64_t t) { boil_start_time = t; }
};

enum BrewStage { PREMASH, MASHING, DRAINING, BOILING, CHILLING, DECANTING, DONE, CANCELLED};


// The entire state vector of what is happening in the brew.
struct FullBrewState {
 uint32_t weight;
 BrewStage current_stage = PREMASH;
 BrewState state;
 Weights weights;
 Times times;
};

int64_t GetTimeMsec();
