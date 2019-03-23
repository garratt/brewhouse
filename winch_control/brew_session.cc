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
#include <iostream>


using std::placeholders::_1;
using std::placeholders::_2;

int BrewSession::InitSession(const char *spreadsheet_id) {
  // ------------------------------------------------------------------
  // Initialize the logger, which reads from the google sheet
  int logger_status = brew_logger_.SetSession(spreadsheet_id);
  if (logger_status < 0) {
    printf("Failed to set session\n");
    return -1;
  }
  if (logger_status > 0) {
    printf("Restarting sessions currently not supported\n");
    return -1;
  }
  brew_recipe_ = brew_logger_.ReadRecipe();

  // Start Scale Loop
  if(scale_.InitLoop(std::bind(&BrewSession::OnScaleError, this)) < 0) {
    printf("Scale did not initialize correctly\n");
    return -1;
  }

  // Log the weight every 10 seconds
  scale_.SetPeriodicWeightCallback(10*1000*1000,
                                   std::bind(&BrewLogger::LogWeight, &brew_logger_, _1, _2));

  // Set the function that the winch controller uses to see if it should abort movement
  winch_controller_.SetAbortCheck(std::bind(&ScaleFilter::HasKettleLifted, &scale_));
  // ------------------------------------------------------------------
  // Initialize the Grainfather serial interface
  // Make sure things are working
  grainfather_serial_.Init(std::bind(&BrewLogger::LogBrewState, &brew_logger_, _1));
  if(grainfather_serial_.TestCommands() < 0) {
    printf("Grainfather serial interface did not pass tests.\n");
    return -1;
  }
  // Load Recipe from spreadsheet
  // Connect to Grainfather
  // Load Session
  if(grainfather_serial_.LoadSession(brew_recipe_.GetSessionCommand().c_str())) {
    std::cout<<"Failed to Load Brewing Session into Grainfather. Exiting" <<std::endl;
    return -1;
  }
  // Okay, initializations complete!
  return 0;
}



int BrewSession::PrepareSetup() {
  user_interface_.PleaseFillWithWater(brew_recipe_.initial_volume_liters);
  brew_logger_.LogWeightEvent(WeightEvent::InitWater, scale_.GetWeightStartingNow());

  if (grainfather_serial_.HeatForMash()) return -1;

  user_interface_.PleaseAddHops(brew_recipe_.hops_grams, brew_recipe_.hops_type);
  user_interface_.PleasePositionWinches();
  // Get weight with water and winch
  brew_logger_.LogWeightEvent(WeightEvent::InitRig, scale_.GetWeightStartingNow());

  while (!grainfather_serial_.IsMashTemp()) {  // wait for temperature
    usleep(1000000); // sleep a second
    // TODO: debug prints
    // std::cout << "Waiting for temp.  Target: " << full_state_.state.target_temp;
    // std::cout << "  Current: " << full_state_.state.current_temp << std::endl;
  }

  // The OnMashTemp should just turn the buzzer off.
  user_interface_.PleaseAddGrain();
  // Take weight with grain
  brew_logger_.LogWeightEvent(WeightEvent::InitGrain, scale_.GetWeightStartingNow());

  if(user_interface_.PleaseFinalizeForMash()) return -1;
  // Okay, we are now ready for Automation!
  //Watch for draining
  scale_.EnableDrainingAlarm(std::bind(&BrewSession::OnDrainAlarm, this));
  return 0;
}

int BrewSession::Mash() {
  if (grainfather_serial_.StartMash()) return -1;
  while (!grainfather_serial_.IsMashDone()) {  // wait for mash to complete
    SleepSeconds(1);
    // TODO: debug prints
    // std::cout << "Waiting for temp.  Target: " << full_state_.state.target_temp;
    // std::cout << "  Current: " << full_state_.state.current_temp << std::endl;
  }
  return 0;
}

void BrewSession::Fail(const char *segment) {
  std::cout << "Encountered Failure during " << segment;
  std::cout << " stage." << std::endl;
  GlobalPause(); 
}

void BrewSession::GlobalPause() {
  // TODO: figure out how to pause timer, how to interrupt other waits
  // if (full_state_.state.timer_on) {
    // grainfather_serial_.PauseTimer();
  // }
  TurnPumpOff();
  grainfather_serial_.TurnHeatOff();
  // tweet out a warning!
}


// Shut everything down because of error.
void BrewSession::QuitSession() {
  GlobalPause();
  grainfather_serial_.QuitSession();
}

int BrewSession::Drain() {
  // std::cout << "Triggered: Mash is completed" << std::endl; // TODO: debug print
  if (grainfather_serial_.StartSparge()) return -1;
  // Might as well turn off valves:
  if (TurnPumpOff()) return -1;
  brew_logger_.LogWeightEvent(WeightEvent::AfterMash, scale_.GetWeightStartingNow());
  // Now we wait for a minute or so to for fluid to drain from hoses
  SleepMinutes(1);
  // Raise a little bit to check that we are not caught
  if (winch_controller_.RaiseToDrain_1() < 0) return -1;
  SleepSeconds(3);
  if (scale_.HasKettleLifted()) {
    std::cout << "We lifted the kettle!" << std::endl;
    return -1;
  }
  // TODO: wait any more?
  // Raise the rest of the way
  std::cout << "RaiseStep2" << std::endl;
  if (winch_controller_.RaiseToDrain_2()) return -1;
  SleepMinutes(1);
  // After weight has settled from lifting the mash out
  brew_logger_.LogWeightEvent(WeightEvent::AfterLift, scale_.GetWeightStartingNow());
  // Drain for 45 minutes // TODO: detect drain stopping
  SleepMinutes(45);
  std::cout << "Draining is complete" << std::endl;
  brew_logger_.LogWeightEvent(WeightEvent::AfterDrain, scale_.GetWeightStartingNow());
  if (winch_controller_.MoveToSink()) return -1;
  return 0;
}

// When Mash complete
// Pump Off, valves closed
// measure after_mash_weight, tweet loss
// wait for minute, raise
// wait 45 minutes
// move to sink
// advance to boil

int BrewSession::Boil() {
  if (grainfather_serial_.HeatToBoil()) return -1;
  while (!grainfather_serial_.IsBoilTemp()) {  // wait for Boil temp
    usleep(1000000); // sleep a second
  }

  std::cout << "Boiling Temp reached" << std::endl;
  if (winch_controller_.LowerHops()) return -1;
  //Watch for draining, because we are opening the valves
  if(PumpToKettle()) return -1;
  if (grainfather_serial_.StartBoil()) return -1;
  while (!grainfather_serial_.IsBoilDone()) {  // wait for Boil temp
    usleep(1000000); // sleep a second
  }
  if(TurnPumpOff()) return -1;
  if (grainfather_serial_.QuitSession()) return -1;
  // Wait one minute before raising hops for lines to drain
  SleepMinutes(1);
  if (winch_controller_.RaiseHops()) return -1;
  SleepMinutes(1);  // sleep for 1 minute to drain
  brew_logger_.LogWeightEvent(WeightEvent::AfterBoil, scale_.GetWeightStartingNow());
  return 0;
}

int BrewSession::Decant() {
  std::cout << "Decanting" << std::endl;
  if (PumpToCarboy()) return -1;
  ActivateChillerPump();
  while (!scale_.CheckEmpty()) {  // wait kettle to empty
    usleep(500000); // sleep .5 second
  }
  DeactivateChillerPump();
  if (TurnPumpOff()) return -1;
  return 0;
}

// TODO: make a function that handles the pump serial, valve status
// and scale callbacks
int BrewSession::TurnPumpOff() {
  SetFlow(NO_PATH);
  if (grainfather_serial_.TurnPumpOff()) return -1;
  // with the valves shut, can safely  stop worrying about draining for the moment
  scale_.DisableDrainingAlarm();
  return 0;
}

int BrewSession::PumpToKettle() {
  SetFlow(KETTLE);
  if (grainfather_serial_.TurnPumpOn()) return -1;
  scale_.EnableDrainingAlarm(std::bind(&BrewSession::OnDrainAlarm, this));
  return 0;
}

int BrewSession::PumpToCarboy() {
  scale_.DisableDrainingAlarm();
  SetFlow(CHILLER);
  if (grainfather_serial_.TurnPumpOn()) return -1;
  return 0;
}




int BrewSession::Run(const char *spreadsheet_id) {
  if (InitSession(spreadsheet_id)) { Fail("Init"); return -1; }
  if (PrepareSetup()) { Fail("Prepare"); return -1; }
  if (Mash())   { Fail("Mash"); return -1; }
  if (Drain())  { Fail("Drain"); return -1; }
  if (Boil())   { Fail("Boil"); return -1; }
  if (Decant()) { Fail("Decant"); return -1; }
  std::cout << "Brew finished with no problems!" << std::endl;
  return 0;
}


