// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "scale_filter.h"

#include "brew_types.h"
#include "gpio.h"
#include "grainfather2.h"
#include "user_interface.h"
#include "winch.h"
#include "valves.h"
#include "logger.h"
#include <utility>
#include <deque>
#include <mutex>
#include <assert.h>
#include <vector>
#include <list>
#include <functional>

// The brew session can be divided into a few main parts:
// 1) Setup and adding ingredients.
//   There are a couple of states that we get from the grainfather,
//   but it is mostly user input until we start the mash
//     1) Test connections, interfaces
//     2) Check recipe
//     3) Fill with water
//     4) Add hops to basket
//     5) Position Winches
//     6) Wait for temp, add Grains
//     7) Install top hardware
//     8) Start mash
//
// 2) Mashing
//   There is no user input or other automation here.
//   We can monitor the state, and don;t have to have tight control
//   loops.
//     1) Wait for mash stages, maybe take weight at each stage
// 3) Draining and Boil
//   Draining is all about using the winch and scale.  We want to make
//   sure we don't lift the kettle or pump the wort out of the kettle.
//   Most transitions are based on time and weight
//   Start: when mash is done.
//     1) turn everything off, lift a little,
//        check that we didn't lift the kettle
//        lift more to drain.
//     2) After a while, declare draining done
//        Lift and move mash to sink
//        Initiate heat to boil
//     3) When Boil temp is reached, lower in hops
//        Enable pumps, check that we aren't losing wort
//        wait for boil to be done
//     4) When boil is done:
//        Turn pumps off
//        Turn scale warnings off
//        Raise up hops
//        let hops drain for a bit
//        raise the rest of the way
//        Can quit session at this point.
// 4) Decanting
//    For this stage, the only thing we do with the grainfather is
//    turn the pump on.
//      1) may do something with valves and outputs to make sure the
//         boiling wort sterilizes them
//      2) Set Valves, enable pump
//         Monitor weight until empty, to turn off the pump
//
//
//  Grainfather interface:
//  state callback -> just for logging to the spreadsheet
//  bool IsMashtemp();
//  int  StartMash();
//  bool IsMashDone();
//  int StartSparge() {
//    // Check that we are at end of mash
//    TurnPumpOff();
//    AdvanceStage();
//    }
//  int HeatToBoil();
//  bool IsBoilTemp();
//  int StartBoil();
//  bool IsBoilDone();
//
//






class BrewSession {
// session info, shouldn't change:
  BrewRecipe brew_recipe_;
  int64_t drain_duration_s_ = 45 * 60;  // loaded from spreadsheet
  // std::string spreadsheet_id_;
  GrainfatherSerial grainfather_serial_;
  FullBrewState full_state_;
  WinchController winch_controller_;
  // WeightLimiter weight_limiter_;
  BrewLogger brew_logger_;
  ScaleFilter scale_;
  UserInterface user_interface_;

  bool logger_disabled_ = false;
  bool grainfather_disabled_ = false;
  bool winch_disabled_ = false;
  bool scale_disabled_ = false;
  // Makes all waits 30 times faster -> 5 minutes becomes 10 seconds
  // bool zippy_time_ = false;
  uint32_t zippy_time_divider_ = 1;
  bool user_interface_bypassed_ = false;

  void RunForReal() {
    assert(logger_disabled_ == false);
    assert(winch_disabled_ == false);
    assert(scale_disabled_ == false);
    assert(grainfather_disabled_ == false);
    assert(zippy_time_divider_ == 1);
    assert(user_interface_bypassed_ == false);
  }

  void Fail(const char *segment);

  // These functions control the valves,
  // the pump command and the DrainAlarm
  // to put them in the correct states when
  // We change where we want wort to go.
  int TurnPumpOff();
  int PumpToCarboy();
  int PumpToKettle();

  // if the scale stops reading correctly
  void OnScaleError() {GlobalPause();}

  // If we are losing wort
  void OnDrainAlarm() {GlobalPause();}

  // Not to be used for precise timing!
  void SleepMinutes(int minutes) {
    SleepSeconds(minutes * 60);
  }

  // Not to be used for precise timing!
  void SleepSeconds(int seconds) {
    int64_t one_second = 1000000 / zippy_time_divider_;
    for (int i = 0; i < seconds; ++i) {
      usleep(one_second);
    }
  }

  public:
  BrewSession() : scale_("calibration.txt") {}

  // Starts entire brewing session
  int Run(const char *spreadsheet_id);
  // The individual stages of the brew session are here:
  int InitSession(const char *spreadsheet_id);
  int PrepareSetup();
  int Mash();
  int Drain();
  int Boil();
  int Decant();


  void GlobalPause();

  // Shut everything down because of error.
  void QuitSession();

  void OnChangeState(const FullBrewState &new_state, const FullBrewState &old_state);

  void SetOfflineTest() { brew_logger_.DisableForTest();  logger_disabled_ = true; }
  void SetFakeGrainFather() {grainfather_serial_.DisableForTest();  grainfather_disabled_ = true; }
  void SetFakeWinch() { winch_controller_.Disable(); winch_disabled_ = true; }
  void SetFakeScale() { scale_.DisableForTest(); scale_disabled_ = true; }
  void SetZippyTime() { zippy_time_divider_ = 30; }
  void BypassUserInterface() { user_interface_.DisableForTest(); user_interface_bypassed_ = true; }

};
