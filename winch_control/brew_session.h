// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include "grainfather2.h"
#include "user_interface.h"
#include "winch.h"
#include "scale.h"
#include "valves.h"
#include "logger.h"
#include <utility>
#include <deque>
#include <mutex>
#include <assert.h>
#include <vector>
#include <list>
#include <functional>


#define BREW_WARNING -1
#define BREW_ERROR -2
#define BREW_FATAL -3


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

struct FullBrewState {
 uint32_t weight;
 BrewStage current_stage = PREMASH;
 BrewState state;
 Weights weights;
 Times times;
};

class BrewSession {
// session info, shouldn't change:
  BrewRecipe brew_recipe_;
  int64_t drain_duration_s;  // loaded from spreadsheet
  std::string spreadsheet_id;
  GrainfatherSerial grainfather_serial_;
  FullBrewState full_state_;
  WinchController winch_controller_;
  WeightLimiter weight_limiter_;
  BrewLogger brew_logger_;
  WeightFilter scale_;
  UserInterface user_interface_;
  static constexpr uint32_t kStopDecantingWeightGrams = 10000;
  static constexpr uint32_t kWeightLossThresholdGrams = 75;
  static constexpr uint32_t kGrainfatherPickupThresholdGrams = 75;

  bool logger_disabled_ = false;
  bool grainfather_disabled_ = false;
  bool winch_disabled_ = false;
  bool scale_disabled_ = false;
  bool zippy_time_ = false;

  void SetOfflineTest() { brew_logger_.DisableForTest();  logger_disabled_ = true; }
  void SetFakeGrainFather() {grainfather_serial_.DisableForTest();  grainfather_disabled_ = true; }
  void SetFakeWinch() { winch_controller_.Disable(); winch_disabled_ = true; }
  void SetFakeScale() { scale_.DisableForTest(); scale_disabled_ = true; }
  void SetZippyTime() { zippy_time_ = true; }

  void RunForReal() {
    assert(logger_disabled_ == false);
    assert(winch_disabled_ == false);
    assert(scale_disabled_ == false);
    assert(grainfather_disabled_ == false);
    assert(zippy_time_ == false);
  }

  struct StateTransition {
    FullBrewState new_state, prev_state;
  };

  std::deque<StateTransition> transition_queue_;
  std::mutex tq_mutex_;

  using IsTriggered = std::function<bool(const FullBrewState&, const FullBrewState&)>;
    // bool (*)(const FullBrewState&, const FullBrewState&);
  using TriggerFunc = std::function<void(const FullBrewState&)>;
  // void (*)(const FullBrewState&);
  struct ConditionalFunc {
    bool repeat = false;
    std::function<bool(const FullBrewState&, const FullBrewState&)> condition;
    std::function<void(const FullBrewState&)> callback;
  };
  std::vector<ConditionalFunc> triggers_;
  std::mutex trigger_mutex_;

  void RegisterCallback(IsTriggered condition, TriggerFunc callback, bool repeat);

  // Called by other thread, when new weight is recorded
  void OnNewWeight(double new_weight);

  // Called by other thread, when new state is recorded
  void OnNewBrewState(BrewState new_state);
  void RecordNewState(FullBrewState new_state, FullBrewState prev_state);


  int LoadTriggers();
  void AddTimeTrigger(int64_t trigger_time, TriggerFunc trigger_func);
  bool enable_trigger_thread_ = false;
  bool global_pause_ = false;
  static constexpr int64_t kMaxTimeBetweenEvents = 1000; // 1 second

  void CheckTriggerThread();
  std::thread check_trigger_thread_;

  public:

  // Starts entire brewing session
  void StartSession(const char *spreadsheet_id);

  void GlobalPause();

  // Shut everything down because of error.
  void QuitSession();

  int OnChangeState(const FullBrewState &new_state, const FullBrewState &old_state);


  // Mashing
  // bool IsMashTemp(const FullBrewState &new_state, const FullBrewState &prev_state);
  // int OnMashTemp(const FullBrewState &current_state);
  // bool IsMashStart(const FullBrewState &new_state, const FullBrewState &prev_state);
  // int OnMashStart(FullBrewState current_state);
  bool IsMashComplete(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnMashComplete(FullBrewState current_state);

  // Draining
  // Raise a little bit to check that we are not caught
  int RaiseStep1(FullBrewState current_state);
  // Raise the rest of the way
  int RaiseStep2(FullBrewState current_state);
  // After weight has settled from lifting the mash out
  int RaiseStep3(FullBrewState current_state);
  int OnDrainComplete(FullBrewState current_state);

  // Boil
  bool IsBoilTemp(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnBoilTemp(FullBrewState current_state);
  bool IsBoilDone(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnBoilDone(FullBrewState current_state);
  int OnPostBoil1Min(FullBrewState current_state);

  // Decanting
  int StartDecanting(FullBrewState current_state);
  bool IsDecantFinished(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnDecantFinished(FullBrewState current_state);

  // Error Triggers
  // TODO: this should really be time based
  bool IsRapidWeightLoss(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnRapidWeightLoss(FullBrewState current_state);
  bool IsGrainfatherPickup(const FullBrewState &new_state, const FullBrewState &prev_state);
  int OnGrainfatherPickup(FullBrewState current_state);
  int CheckSystemFunctionality();

};
// TODO: 
// Make init function, check all functionality
// make source fle, make main file for this.
