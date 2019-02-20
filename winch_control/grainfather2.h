// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include <utility>
#include <mutex>
#include <functional>

int64_t GetTimeMsec();


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
};


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
  std::function<void(BrewState)> brew_state_callback_;
  std::mutex state_mutex_;
  bool quit_now_ = false;
  bool read_error_ = false;
  int fd_;

  int Connect(const char *path);

  BrewState latest_state_, previous_state_;
 public:
  // Gets the latest state.  If |prev_read| == 0,
  // just pulls the value of latest_state_ in a protected fashion.
  // Otherwise, waits until a state is available
  BrewState GetLatestState(int64_t prev_read = 0);

  int SendSerial(std::string to_send);

  // Runs a command, and ensures that is completes successfully.  Blocks until
  // a reading is performed, so could block up to 2 seconds.
  // returns 0 if the brewstate is valid and the verify condition is true, either
  //           already, or after the command
  // returns -1 for all errors
  int CommandAndVerify(const char *command, bool (*verify_condition)(BrewState));

  int TurnPumpOn();
  int TurnPumpOff();
  int TurnHeatOn();
  int TurnHeatOff();
  int QuitSession();
  int AdvanceStage();
  int PauseTimer();
  int ResumeTimer();

  int LoadSession(const char *session_string);

  // Register a callback to be called when a new brewstate is read:
  void RegisterBrewStateCallback(std::function<void(BrewState)> callback);


  // tests all the commands, to make sure they change the state.
  int TestCommands();

  BrewState ParseState(char in[kStatusLength]);
  // Read status
  void ReadStatusThread();
};



