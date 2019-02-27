// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio.h"
#include "grainfather2.h"
#include "brew_session.h"
#include <utility>
#include <deque>
#include <mutex>
#include <list>
#include <functional>


using std::placeholders::_1;
using std::placeholders::_2;
void BrewSession::RegisterCallback(IsTriggered condition, TriggerFunc callback, bool repeat) {
  std::lock_guard<std::mutex> lock(trigger_mutex_);
  ConditionalFunc cf;
  cf.repeat = repeat;
  cf.condition = condition;
  cf.callback = callback;
  triggers_.emplace_back(cf);
}


// Called by other thread, when new weight is recorded
void BrewSession::OnNewWeight(double new_weight) {
  // TODO: better detections by maintaining more weight history here?
  StateTransition new_transition;
  new_transition.prev_state = full_state_;
  new_transition.new_state = full_state_;
  new_transition.new_state.weight = new_weight;
  // log weight to the spreadsheet.  weight_limiter_ makes sure we only log
  // often when the weight is changing.
  double weight_out;
  time_t log_time;
  if (weight_limiter_.PublishWeight(new_weight, &weight_out, &log_time)) {
    brew_logger_.LogWeight(weight_out, log_time);
  }
  {
    std::lock_guard<std::mutex> lock(tq_mutex_);
    transition_queue_.push_back(new_transition);
  }
}

// Called by other thread, when new state is recorded
void BrewSession::OnNewBrewState(BrewState new_state) {
  StateTransition new_transition;
  new_transition.prev_state = full_state_;
  new_transition.new_state = full_state_;
  new_transition.new_state.state = new_state;
  // TODO: update current_stage based on state
// enum BrewStage { PREMASH, MASHING, DRAINING, BOILING, CHILLING, DECANTING, DONE, CANCELLED};
  int mash_steps = brew_recipe_.mash_times.size();
  // {1,mash_steps} -> mashing
  //  1+mash_steps -> Draining
  //  2+mash_steps -> Boiling
  if (new_state.stage > 0 && new_state.stage < mash_steps) {
      new_transition.new_state.current_stage = MASHING;
  }
  if (new_state.stage == mash_steps + 1) {
      new_transition.new_state.current_stage = DRAINING;
  }
  if (new_state.stage == mash_steps + 2) {
      new_transition.new_state.current_stage = BOILING;
  }
  // TODO: Check if the grainfather registers anything after boil.


  {
    std::lock_guard<std::mutex> lock(tq_mutex_);
    transition_queue_.push_back(new_transition);
  }
}

void BrewSession::AddTimeTrigger(int64_t trigger_time, TriggerFunc trigger_func) {
  RegisterCallback([trigger_time] (const FullBrewState &new_state,
        const FullBrewState &prev_state ) {
      return new_state.state.read_time > trigger_time; },
      trigger_func, false);
}

#define UPDATE_ON_CHANGE(var) \
  if (new_state.var != prev_state.var) { \
    full_state_.var = new_state.var;     \
  }                                      \



void BrewSession::RecordNewState(FullBrewState new_state, FullBrewState prev_state) {
  // take all the new values, leave anything that did not change
  UPDATE_ON_CHANGE(weight);
  UPDATE_ON_CHANGE(current_stage);
  // UPDATE_ON_CHANGE(weights);
  // UPDATE_ON_CHANGE(times);
  UPDATE_ON_CHANGE(state);
}

void BrewSession::CheckTriggerThread() {
  // we don't use conditional variables here, because we want to check
  // for conditions at a given frequency.
  int64_t last_event = 0;
  while (enable_trigger_thread_) {
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
        GlobalPause();
        // Pause and tweet
      }
      if (ret == BREW_FATAL) {
        QuitSession();
        // Stop the brew process
      }
      // Otherwise, record the new state
      RecordNewState(transition.new_state, transition.prev_state);
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


void BrewSession::StartSession(const char *spreadsheet_id) {

  // start trigger thread, although with no triggers, it will not be doing anything...
  enable_trigger_thread_ = true;
  transition_queue_.clear();
  check_trigger_thread_ = std::thread(std::bind(&BrewSession::CheckTriggerThread, this));
  // ------------------------------------------------------------------
  // Initialize the logger, which reads from the google sheet
  int logger_status = brew_logger_.SetSession(spreadsheet_id);
  if (logger_status < 0) {
    printf("Failed to set session\n");
    return;
  }
  if (logger_status > 0) {
    printf("Restarting sessions currently not supported\n");
    return;
  }
  brew_recipe_ = brew_logger_.ReadRecipe();

  // Check Scale functionality:
  if(scale_.CheckScale() < 0) {
    printf("Scale did not pass tests.\n");
    return;
  }

  // ------------------------------------------------------------------
  // Initialize the Grainfather serial interface
  // Make sure things are working
  if(grainfather_serial_.TestCommands() < 0) {
    printf("Grainfather serial interface did not pass tests.\n");
    return;
  }
  grainfather_serial_.Init(std::bind(&BrewSession::OnNewBrewState, this, _1));
  // Load Recipe from spreadsheet
  // Connect to Grainfather
  // Load Session
  if(grainfather_serial_.LoadSession(brew_recipe_.GetSessionCommand().c_str())) {
    std::cout<<"Failed to Load Brewing Session into Grainfather. Exiting" <<std::endl;
    return;
  }

  user_interface_.PleaseFillWithWater(brew_recipe_.initial_volume_liters);
  ScaleStatus status = scale_.GetWeight();
   if (status.state != ScaleStatus::READY) {
     printf("Error reading Scale!\n");
     return;
   }
  full_state_.weights.RecordInitWater(status.weight);

  grainfather_serial_.AdvanceStage(); // TODO: this should be a StartHeatingForMash command
  // Now tell Grainfather to start heating

  user_interface_.PleaseAddHops(brew_recipe_.hops_grams, brew_recipe_.hops_type);
  user_interface_.PleasePositionWinches();
  // Get weight with water and winch
  status = scale_.GetWeight();
   if (status.state != ScaleStatus::READY) {
     printf("Error reading Scale!\n");
     return;
   }
  full_state_.weights.RecordInitRig(status.weight);
  // wait for temperature
  while (full_state_.state.current_temp < full_state_.state.target_temp) {
    usleep(500000); // sleep half a second
  }
  // The OnMashTemp should just turn the buzzer off.
  user_interface_.PleaseAddGrain();
  // Take weight with grain
  status = scale_.GetWeight();
   if (status.state != ScaleStatus::READY) {
     printf("Error reading Scale!\n");
     return;
   }
  full_state_.weights.RecordInitGrain(status.weight);

  if(user_interface_.PleaseFinalizeForMash()) return;
  // Okay, we are now ready for Automation!

  LoadTriggers();

  // Initialize the Scale
  scale_.InitLoop(std::bind(&BrewSession::OnNewWeight, this, _1));


  grainfather_serial_.AdvanceStage(); // TODO: this should be a StartMash command

}

void BrewSession::GlobalPause() {
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
void BrewSession::QuitSession() {
  GlobalPause();
  grainfather_serial_.QuitSession();
  enable_trigger_thread_ = false;

}
// signal on transitions
// Register wait conditions

int BrewSession::OnChangeState(const FullBrewState &new_state, const FullBrewState &old_state) {
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

int BrewSession::LoadTriggers() {
  // RegisterCallback(std::bind(&BrewSession::IsMashTemp, this, _1, _2),
      // std::bind(&BrewSession::OnMashTemp, this, _1), false);
  // RegisterCallback(std::bind(&BrewSession::IsMashComplete, this, _1, _2),
      // std::bind(&BrewSession::OnMashComplete, this, _1), false);
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

#if 0
bool BrewSession::IsMashTemp(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.state.stage!= 1) return false;
  return !new_state.state.waiting_for_temp && prev_state.state.waiting_for_temp;
}

int BrewSession::OnMashTemp(const FullBrewState &current_state) {
  full_state_.weights.RecordInitWater(current_state.weight);
  // tweet readiness
  return 0;
}

bool BrewSession::IsMashStart(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.state.stage != 1) return false;
  // TODO: also check for timer?
  return !new_state.state.waiting_for_input && prev_state.state.waiting_for_input;
}

int BrewSession::OnMashStart(FullBrewState current_state) {
  full_state_.weights.RecordInitGrain(current_state.weight);
  return 0;
}
// When mash starts
// weigh grain bill, tweet

#endif

bool BrewSession::IsMashComplete(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.current_stage != DRAINING) return false;
  if (prev_state.current_stage == DRAINING) return false;
  if (new_state.state.timer_on || !prev_state.state.timer_on) return false;
  return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
}
int BrewSession::OnMashComplete(FullBrewState current_state) {
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
int BrewSession::RaiseStep1(FullBrewState current_state) {
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
int BrewSession::RaiseStep2(FullBrewState current_state) {
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
int BrewSession::RaiseStep3(FullBrewState current_state) {
  full_state_.weights.RecordAfterLift(current_state.weight);
  // Drain for 45 minutes
  AddTimeTrigger(GetTimeMsec() + (45 * 60 * 1000),
      std::bind(&BrewSession::OnDrainComplete, this, _1));
  return 0;
}

int BrewSession::OnDrainComplete(FullBrewState current_state) {
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

bool BrewSession::IsBoilTemp(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.current_stage != DRAINING) return false;
  if (prev_state.current_stage == DRAINING) return false;
  if (new_state.state.timer_on || !prev_state.state.timer_on) return false;
  return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
}
int BrewSession::OnBoilTemp(FullBrewState current_state) {
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

bool BrewSession::IsBoilDone(const FullBrewState &new_state, const FullBrewState &prev_state) {
  // if (new_state.stage != SPARGE) return false;
  // Was just in boil stage:
  if (prev_state.current_stage == BOILING) return false;
  // timer just turned off:
  if (!new_state.state.timer_on || prev_state.state.timer_on) return false;
  // Now we are waiting for input:
  return new_state.state.waiting_for_input && !prev_state.state.waiting_for_input;
}
int BrewSession::OnBoilDone(FullBrewState current_state) {
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

int BrewSession::OnPostBoil1Min(FullBrewState current_state) {
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

int BrewSession::StartDecanting(FullBrewState current_state) {
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

bool BrewSession::IsDecantFinished(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.current_stage != DECANTING) return false;
  return new_state.weight < kStopDecantingWeightGrams;
}


int BrewSession::OnDecantFinished(FullBrewState current_state) {
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
bool BrewSession::IsRapidWeightLoss(const FullBrewState &new_state, const FullBrewState &prev_state) {
  if (new_state.current_stage == DECANTING) return false;
  if (new_state.weight > prev_state.weight) return false;
  return  prev_state.weight - new_state.weight > kWeightLossThresholdGrams;
}


int BrewSession::OnRapidWeightLoss(FullBrewState current_state) {
  // if weight decreases rapidly (when not decanting)
  // Stop pump, valves
  // pause session
  GlobalPause();
  // TODO: tweet out a warning!
}


bool BrewSession::IsGrainfatherPickup(const FullBrewState &new_state, const FullBrewState &prev_state) {
  return new_state.weight < kGrainfatherPickupThresholdGrams;
}
int BrewSession::OnGrainfatherPickup(FullBrewState current_state) {
  GlobalPause();
  // if weight goes below empty grainfather
  // Stop session
}

// TODO: 
// Make init function, check all functionality
// make source fle, make main file for this.
