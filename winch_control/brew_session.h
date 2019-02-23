// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include "grainfather2.h"
#include <utility>
#include <deque>
#include <mutex>
#include <list>
#include <functional>


#define BREW_WARNING -1
#define BREW_ERROR -2
#define BREW_FATAL -3


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

struct Times {
  int64_t brew_start_time = 0, mash_start_time = 0;
  int64_t mash_end_time = 0, boil_start_time = 0;
  void RecordBrewStart(int64_t t) { brew_start_time = t; }
  void RecordMashStart(int64_t t) { mash_start_time = t; }
  void RecordMashEnd(int64_t t)   { mash_end_time = t; }
  void RecordBoilStart(int64_t t) { boil_start_time = t; }
};

enum BrewStage { PREMASH, MASHING, DRAINING, BOILING, CHILLING, DECANTING, DONE};

struct FullBrewState {
 uint32_t weight;
 BrewStage current_stage;
 BrewState state;
 Weights weights;
 Times times;
};

  using std::placeholders::_1;
  using std::placeholders::_2;
class BrewSession {
// session info, shouldn't change:
  BrewRecipe recipe_;
  // unsigned boil_minutes = 0;
  // std::vector<std::pair<double, uint32_t>> mash_steps;
  int64_t drain_duration_s;  // loaded from spreadsheet
  // std::vector<uint8_t> addition_times;
  // std::string session_name;
  std::string spreadsheet_id;
  GrainfatherSerial grainfather_serial_;
  // double initial_fill = 0, sparge_amount = 0;
  // End info that we send to grainfather
  // Stage current_stage;
  // All times are in miliseconds from epoch
  // int64_t brew_start_time = 0, mash_start_time, estimated_mash_end_time;
  // int64_t actual_mash_end_time, boil_start_time;
  // All weights are in grams:
  // Weights weights;
  FullBrewState full_state_;
  WinchController winch_controller_;

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

  void RegisterCallback(IsTriggered condition, TriggerFunc callback, bool repeat) {
   std::lock_guard<std::mutex> lock(trigger_mutex_);
   ConditionalFunc cf;
   cf.repeat = repeat;
                      cf.condition = condition;
                      cf.callback = callback;
   triggers_.emplace_back(cf);
  }


  // Called by other thread, when new weight is recorded
  void OnNewWeight(uint32_t new_weight) {
    // TODO: better detections by maintaining more weight history here?
    StateTransition new_transition;
    new_transition.prev_state = full_state_;
    new_transition.new_state = full_state_;
    new_transition.new_state.weight = new_weight;
    {
      std::lock_guard<std::mutex> lock(tq_mutex_);
      transition_queue_.push_back(new_transition);
    }
  }

  // Called by other thread, when new state is recorded
  void OnNewBrewState(BrewState new_state) {
    // TODO: update current_stage based on state
    StateTransition new_transition;
    new_transition.prev_state = full_state_;
    new_transition.new_state = full_state_;
    new_transition.new_state.state = new_state;
    {
      std::lock_guard<std::mutex> lock(tq_mutex_);
      transition_queue_.push_back(new_transition);
    }
  }

  void AddTimeTrigger(int64_t trigger_time, TriggerFunc trigger_func) {
      RegisterCallback([trigger_time] (const FullBrewState &new_state,
                                    const FullBrewState &prev_state ) {
                    return new_state.state.read_time > trigger_time; },
                    trigger_func, false);
  }

  bool quit_now_ = false;
  bool global_pause_ = false;
  static constexpr int64_t kMaxTimeBetweenEvents = 1000; // 1 second

  void CheckTriggerThread() {
    // we don't use conditional variables here, because we want to check
    // for conditions at a given frequency.
    int64_t last_event = 0;
    while (!quit_now_) {
      if (global_pause_) { // if paused, just sleep until not paused
        usleep(100000);
        continue;
      }
      bool has_transition = false;
      StateTransition transition;
      {
        std::lock_guard<std::mutex> lock(tq_mutex_);
        if (transition_queue_.size()) {
          transition = transition_queue_.front();
          transition_queue_.pop_front();
          has_transition = true;
        }
      }
      if(has_transition) {
        last_event = GetTimeMsec();
        int ret = OnChangeState(transition.new_state, transition.prev_state);
        if (ret == BREW_WARNING) {
          printf("Error processing event\n");
        }
        if (ret == BREW_ERROR) {
          // Pause and tweet
        }
        if (ret == BREW_FATAL) {
          // Stop the brew process
        }
      } else {
        int64_t time_now = GetTimeMsec();
        if (time_now - last_event > kMaxTimeBetweenEvents) {
          // make an event with just the latest state:
          transition.new_state = full_state_;
          transition.prev_state = full_state_;
          transition.new_state.state.read_time = time_now;
          std::lock_guard<std::mutex> lock(tq_mutex_);
          transition_queue_.push_back(transition);
        } else {
          // not creating a new event, nor have any in the queue. Take a nap!
        usleep(100000); // sleep for 100ms
        }
      }
    } // End While
  }




  public:

  void StartSession() {
    // Load Recipe from spreadsheet
    // Connect to Grainfather
    // Load Session
    // If has water, start heating, otherwise add trigger to start when water is added
    // Instruction:
    //   Set winch config (then do test to check alignment?)
    //   Add X water
    //   Add Y Additions
    //   Add hops to basket
    //   Add Mash
    //   Click Go!


  }

  void GlobalPause() {
    global_pause_ = true;
    if (full_state_.state.timer_on) {
      grainfather_serial_.PauseTimer();
    }
    grainfather_serial_.TurnPumpOff();
    grainfather_serial_.TurnHeatOff();
    SetFlow(NO_PATH);
    // tweet out a warning!
  }

  // Shut everything down because of error.
  void QuitSession() {
     GlobalPause();
     grainfather_serial_.QuitSession();
     quit_now_ = true;

  }
  // signal on transitions
  // Register wait conditions

  int OnChangeState(const FullBrewState &new_state, const FullBrewState &old_state) {
    std::vector<ConditionalFunc> call_list;
    {
      std::lock_guard<std::mutex> lock(trigger_mutex_);
      for (auto iter = triggers_.begin(); iter != triggers_.end();) {
        if (iter->condition(new_state, old_state)) {
          call_list.push_back(*iter);
          // triggers_[i].callback(new_state);
          // if it was a one shot condition, erase after the call
          if (!iter->repeat) {
            iter = triggers_.erase(iter);
          } else {
            iter++;
          }
        }
      }
    } // end lock
    // now, out of the lock, call the functions:
    for (auto call : call_list) {
      call.callback(new_state);
    }
  }

  int ParseBrewSheet(std::string sheet_name);
  int LoadTriggers() {
     RegisterCallback(std::bind(&BrewSession::IsMashTemp, this, _1, _2),
                      std::bind(&BrewSession::OnMashTemp, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsMashComplete, this, _1, _2),
                      std::bind(&BrewSession::OnMashComplete, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsBoilTemp, this, _1, _2),
                      std::bind(&BrewSession::OnBoilTemp, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsBoilDone, this, _1, _2),
                      std::bind(&BrewSession::OnBoilDone, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsDecantFinished, this, _1, _2),
                      std::bind(&BrewSession::OnDecantFinished, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsRapidWeightLoss, this, _1, _2),
                      std::bind(&BrewSession::OnRapidWeightLoss, this, _1), false);
     RegisterCallback(std::bind(&BrewSession::IsGrainfatherPickup, this, _1, _2),
                      std::bind(&BrewSession::OnGrainfatherPickup, this, _1), false);
  }


  bool IsMashTemp(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.state.stage!= 1) return false;
    return !new_state.state.waiting_for_temp && prev_state.state.waiting_for_temp;
  }

  int OnMashTemp(const FullBrewState &current_state) {
    full_state_.weights.RecordInitWater(current_state.weight);
   // tweet readiness
     return 0;
  }

  bool IsMashStart(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.state.stage != 1) return false;
    // TODO: also check for timer?
    return !new_state.state.waiting_for_input && prev_state.state.waiting_for_input;
  }

  int OnMashStart(FullBrewState current_state) {
    full_state_.weights.RecordInitGrain(current_state.weight);
    return 0;
  }
   // When mash starts
   // weigh grain bill, tweet

  bool IsMashComplete(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.current_stage != DRAINING) return false;
    if (prev_state.current_stage == DRAINING) return false;
    if (new_state.state.timer_on || !prev_state.state.timer_on) return false;
    return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
  }
  int OnMashComplete(FullBrewState current_state) {
    int ret = grainfather_serial_.TurnPumpOff();
    // if comms fails with grainfather, might as well turn off valves:
    SetFlow(NO_PATH);
    full_state_.weights.RecordAfterMash(current_state.weight);
    if (ret < 0) {
      return ret;
    }
    // Now we wait for a minute or so to for fluid to drain from hoses
    AddTimeTrigger(GetTimeMsec() + (60*1000),
                   std::bind(&BrewSession::RaiseStep1, this, _1));
    return 0;
  }

  // Raise a little bit to check that we are not caught
  int RaiseStep1(FullBrewState current_state) {
    int ret = winch_controller_.RaiseToDrain_1();
    if (ret < 0) {
      return ret;
    }
    // Now wait for 5 minutes, so any weight triggers have a chance to catch
    AddTimeTrigger(GetTimeMsec() + (5 * 60 * 1000),
                   std::bind(&BrewSession::RaiseStep2, this, _1));
    return 0;
  }
  // Raise the rest of the way
  int RaiseStep2(FullBrewState current_state) {
    int ret = winch_controller_.RaiseToDrain_2();
    if (ret < 0) {
      return ret;
    }
    // Wait one minute, take after lift weight
    AddTimeTrigger(GetTimeMsec() + (1 * 60 * 1000),
                   std::bind(&BrewSession::RaiseStep3, this, _1));
    return 0;
  }

  // After weight has settled from lifting the mash out
  int RaiseStep3(FullBrewState current_state) {
    full_state_.weights.RecordAfterLift(current_state.weight);
    // Drain for 45 minutes
    AddTimeTrigger(GetTimeMsec() + (45 * 60 * 1000),
                   std::bind(&BrewSession::OnDrainComplete, this, _1));
    return 0;
  }

  int OnDrainComplete(FullBrewState current_state) {
    full_state_.weights.RecordAfterDrain(current_state.weight);
    int ret = winch_controller_.MoveToSink();
    if (ret < 0) {
      return ret;
    }
    return grainfather_serial_.AdvanceStage();
  }


   // When Mash complete
   // Pump Off, valves closed
   // measure after_mash_weight, tweet loss
   // wait for minute, raise
   // wait 45 minutes
   // move to sink
   // advance to boil

  bool IsBoilTemp(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.current_stage != DRAINING) return false;
    if (prev_state.current_stage == DRAINING) return false;
    if (new_state.state.timer_on || !prev_state.state.timer_on) return false;
    return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
  }
  int OnBoilTemp(FullBrewState current_state) {
    int ret = winch_controller_.LowerHops();
    if (ret < 0) {
      return ret;
    }
    SetFlow(KETTLE);
    ret = grainfather_serial_.AdvanceStage();
    if (ret < 0) {
      return ret;
    }
    return grainfather_serial_.TurnPumpOn();
  }
   // When Boil Temp Reached
   // Lower Hops
   // Advance
   // Ensure Pump on
   
  bool IsBoilDone(const FullBrewState &new_state, const FullBrewState &prev_state) {
    // if (new_state.stage != SPARGE) return false;
    // Was just in boil stage:
    if (prev_state.current_stage == BOILING) return false;
    // timer just turned off:
    if (!new_state.state.timer_on || prev_state.state.timer_on) return false;
    // Now we are waiting for input:
    return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
  }
  int OnBoilDone(FullBrewState current_state) {
   // When Boil is done
   // Advance
   // Pump Off, valves closed
    int ret = grainfather_serial_.AdvanceStage();
    if (ret < 0) {
      return ret;
    }
    if (ret = grainfather_serial_.TurnPumpOff()) return ret;
    SetFlow(NO_PATH);
    // Wait one minute before raising hops
    AddTimeTrigger(GetTimeMsec() + (1 * 60 * 1000),
                   std::bind(&BrewSession::OnPostBoil1Min, this, _1));
    return 0;
  }
   
  int OnPostBoil1Min(FullBrewState current_state) {
   // 1 minute after boil off
   // Raise Hops
    int ret = winch_controller_.RaiseHops();
    if (ret < 0) {
      return ret;
    }
    AddTimeTrigger(GetTimeMsec() + (4 * 60 * 1000),
                   std::bind(&BrewSession::StartDecanting, this, _1));
    return 0;
  }
   
  int StartDecanting(FullBrewState current_state) {
    full_state_.weights.RecordAfterBoil(current_state.weight);
    SetFlow(CHILLER);
    ActivateChillerPump();
    grainfather_serial_.TurnPumpOn();
    full_state_.current_stage = DECANTING;
    return 0;
  }
   // 5 minutes after boil
   // take weight,
   // Set flow through chiller
   // Pump on
   // Chiller pump on
  
  static constexpr uint32_t kStopDecantingWeightGrams = 10000;
  bool IsDecantFinished(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.current_stage != DECANTING) return false;
    return new_state.weight < kStopDecantingWeightGrams;
  }


  int OnDecantFinished(FullBrewState current_state) {
    grainfather_serial_.TurnPumpOff();
    DeactivateChillerPump();
    SetFlow(NO_PATH);
    full_state_.current_stage = DONE;

    return 0;
  }
   // When Empty:
   // Stop pump, valves
   // Chiller Pump off


  // TODO: this should really be time based
  static constexpr uint32_t kWeightLossThresholdGrams = 75;

  bool IsRapidWeightLoss(const FullBrewState &new_state, const FullBrewState &prev_state) {
    if (new_state.current_stage == DECANTING) return false;
    if (new_state.weight > prev_state.weight) return false;
    return  prev_state.weight - new_state.weight > kWeightLossThresholdGrams;
  }


  int OnRapidWeightLoss(FullBrewState current_state) {
   // if weight decreases rapidly (when not decanting)
   // Stop pump, valves
   // pause session
    GlobalPause();
    // TODO: tweet out a warning!
  }


  static constexpr uint32_t kGrainfatherPickupThresholdGrams = 75;
  bool IsGrainfatherPickup(const FullBrewState &new_state, const FullBrewState &prev_state) {
    return new_state.weight < kGrainfatherPickupThresholdGrams;
  }
  int OnGrainfatherPickup(FullBrewState current_state) {
    GlobalPause();
   // if weight goes below empty grainfather
   // Stop session
  }

  int CheckSystemFunctionality();

};



// TODO: 
// Make init function, check all functionality
// make source fle, make main file for this.
