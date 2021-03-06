// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include "brew_types.h"
#include "SimulatedGrainfather.h"
#include <utility>
#include <mutex>
#include <functional>


class GrainfatherSerial {
  static constexpr const char *kPumpOnString  = "L1                 ";
  static constexpr const char *kPumpOffString = "L0                 ";
  static constexpr const char *kHeatOnString  = "K1                 ";
  static constexpr const char *kHeatOffString = "K0                 ";
  static constexpr const char *kTempUpString  = "U                  ";
  static constexpr const char *kTempDownString = "D                  ";
  static constexpr const char *kSetButtonString = "I                  ";
  static constexpr const char *kQuitSessionString = "F                  ";
  static constexpr const char *kPauseTimerString = "G                  ";
  static constexpr const char *kResumeTimerString = "G                  ";
  static constexpr char kStartChar = 'T';
  static constexpr unsigned kStatusLength = 4 * 17;
  bool reading_thread_enabled_ = false;
  std::thread reading_thread_;
  std::function<void(BrewState)> brew_state_callback_;
  std::mutex state_mutex_;
  bool read_error_ = false;
  int fd_;
  bool disable_for_test_ = false;
  bool testing_communications_ = false;  // active during startup check
  SimulatedGrainfather simulated_grainfather_;

  int Connect(const char *path);
  int SendSerial(std::string to_send);

  // Runs a command, and ensures that is completes successfully.  Blocks until
  // a reading is performed, so could block up to 2 seconds.
  // returns 0 if the brewstate is valid and the verify condition is true, either
  //           already, or after the command
  // returns -1 for all errors
  int CommandAndVerify(const char *command, bool (*verify_condition)(BrewState));

  BrewState latest_state_, previous_state_;
 public:
  // Gets the latest state.  If |prev_read| == 0,
  // just pulls the value of latest_state_ in a protected fashion.
  // Otherwise, waits until a state is available
  BrewState GetLatestState(int64_t prev_read = 0);

  int TurnPumpOn();
  int TurnPumpOff();
  int TurnHeatOn();
  int TurnHeatOff();
  int QuitSession();
  int AdvanceStage(); // TODO: this is a difficult command to verify.
                      //       Break it into the stateful functions.
  int PauseTimer();
  int ResumeTimer();

  int LoadSession(const char *session_string);

  int WaitForValid();

  // Stateful functions:
  int HeatForMash() { AdvanceStage(); return 0; }
  bool IsMashTemp();
  int  StartMash();
  bool IsMashDone();
  int StartSparge();
  bool IsInSparge(); // just used as a check for HeatToBoil
  int HeatToBoil();
  bool IsBoilTemp();
  int StartBoil();
  bool IsBoilDone();

  // Test all the commands and register a callback for brewstate updates
  int Init(std::function<void(BrewState)> callback);

  // tests all the commands, to make sure they change the state.
  int TestCommands();

  // Read status
  void ReadStatusThread();

  void DisableForTest() { disable_for_test_ = true; }
  ~GrainfatherSerial();
};



