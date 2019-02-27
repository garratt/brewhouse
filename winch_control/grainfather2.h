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

  bool operator!=(const BrewState& other) {
    if (timer_on != other.timer_on) return true;
    if (timer_paused != other.timer_paused) return true;
    if (timer_seconds_left != other.timer_seconds_left) return true;
    if (timer_total_seconds != other.timer_total_seconds) return true;
    if (waiting_for_input != other.waiting_for_input) return true;
    if (waiting_for_temp != other.waiting_for_temp) return true;
    if (brew_session_loaded != other.brew_session_loaded) return true;
    if (heater_on != other.heater_on) return true;
    if (pump_on != other.pump_on) return true;
    if (current_temp != other.current_temp) return true;
    if (target_temp != other.target_temp) return true;
    if (percent_heating != other.percent_heating) return true;
    if (stage != other.stage) return true;
    if (substage != other.substage) return true;
    if (valid != other.valid) return true;
    return false;
  }

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
  bool disable_for_test_ = false;

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
  int AdvanceStage();
  int PauseTimer();
  int ResumeTimer();

  int LoadSession(const char *session_string);

  // Register a callback to be called when a new brewstate is read:
  void RegisterBrewStateCallback(std::function<void(BrewState)> callback);

  // Test all the commands and register a callback for brewstate updates
  int Init(std::function<void(BrewState)> callback);

  // tests all the commands, to make sure they change the state.
  int TestCommands();

  BrewState ParseState(char in[kStatusLength]);
  // Read status
  void ReadStatusThread();

  void DisableForTest() { disable_for_test_ = true; }
};



