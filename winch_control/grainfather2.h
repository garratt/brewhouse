// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"
#include <utility>

int64_t GetTimeMsec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

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

struct Weights {
  uint32_t initial_rig, initial_with_water, initial_with_grain;
  uint32_t after_mash, after_lift, after_drain, after_boil;
  uint32_t after_decant;
  uint32_t latest;
};



struct BrewSession {
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
  std::mutex state_mutex_;
  std::fstream fin;
  bool quit_now_ = false;

  BrewState latest_state_, previous_state_;
  // Gets the latest state.  If |prev_read| == 0,
  // just pulls the value of latest_state_ in a protected fashion.
  // Otherwise, waits until a state is available
  BrewState GetLatestState(int64_t prev_read = 0) {
    BrewState current_state;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      current_state = latest_state_;
    }
    if (current_state.read_time > prev_read) {
      // if latest_state_ has a read time, then it is either what we want,
      // or we tried to read and failed.
      return current_state;
      // Otherwise, wait until we get a new reading.
    }
    // Make sure we won't be waiting super long:
    int64_t current_time = GetTimeMsec();
    if (prev_read > current_time + 2000) {
      printf("Error: requesting a read time too far in the future!\n");
      return BrewState();
    }
    do {
      if (GetTimeMsec() > current_time + 2000) {
        printf("Not getting new readings!\n");
        return BrewState();
      }
      usleep(5000);
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_state = latest_state_;
      }
    } while (current_state.read_time <= prev_read);
    // Now, we should have a current reading, even if it is invalid.
    return current_state;
  }

  int SendSerial(std::string to_send);

  // Runs a command, and ensures that is completes successfully.  Blocks until
  // a reading is performed, so could block up to 2 seconds.
  // returns 0 if the brewstate is valid and the verify condition is true, either
  //           already, or after the command
  // returns -1 for all errors
  int CommandAndVerify(const char *command, bool (*verify_condition)(BrewState)) {
    BrewState latest = GetLatestState();
    if (!latest.valid) return -1;
    // Already met condition, i.e. We asked to turn pump on, but it already was on.
    if (verify_condition((latest))) return 0;
    if (SendSerial(kPumpOnString)) {
      printf("Failed to send command '%s'\n", command);
      return -1;
    }
    int64_t command_time_ms = GetTimeMsec();
    BrewState next = GetLatestState(command_time_ms);
    if (!next.valid) {
      printf("Failed to get another reading from Grainfather.\n");
      return -1;
    }
    if (verify_condition(next)) {
      return 0;
    }
    // Otherwise, we failed to turn pump on.
    printf("Executed command failed to change state.\n");
    return -1;
  }

  int TurnPumpOn() {
    return CommandAndVerify(kPumpOnString, [](BrewState bs) {return bs.pump_on; });
  }
  int TurnPumpOff() {
    return CommandAndVerify(kPumpOffString, [](BrewState bs) {return !bs.pump_on; });
  }
  int TurnHeatOn() {
    return CommandAndVerify(kHeatOnString, [](BrewState bs) {return bs.heater_on; });
  }
  int TurnHeatOff() {
    return CommandAndVerify(kHeatOffString, [](BrewState bs) {return !bs.heater_on; });
  }
  int QuitSession() {
    return CommandAndVerify(kQuitSessionString,
                            [](BrewState bs) {return !bs.brew_session_loaded; });
  }
  int AdvanceStage() {
    return CommandAndVerify(kSetButtonString,
                            [](BrewState bs) {return !bs.waiting_for_input; });
  }

  int LoadSession(const char *session_string) {
    int ret = QuitSession(); // make sure there is no current session
    if (ret) return ret;
    return CommandAndVerify(session_string,
                            [](BrewState bs) {return bs.brew_session_loaded; });
  }



  BrewState ParseState(char in[kStatusLength]) {
    //T1,1,2,60,ZZZZZZZX19.0,19.1,ZZZZZZY1,1,1,0,0,0,1,0,W0,0,0,1,0,1,ZZZZ
    BrewState ret;
    int min_left, sec_left, total_min, timer_on, obj_read;
    obj_read = sscanf(in, "T%d,%d,%d,%d", &timer_on, &min_left, &total_min, &sec_left);
    if (obj_read != 4) {
      printf("BrewState TParsing error.\n");
      return ret;
    }
    ret.timer_on = (timer_on == 1);
    ret.timer_seconds_left = (min_left - 1) * 60 + sec_left;
    ret.timer_total_seconds = 60 * total_min;

    obj_read = sscanf(in + 17, "X%f,%f,", &ret.target_temp, &ret.current_temp);
    if (obj_read != 2) {
      printf("BrewState XParsing error.\n");
      return ret;
    }
    unsigned heat, pump, brew_session, waitfortemp, waitforinput;
    obj_read = sscanf(in + 34, "Y%u,%u,%u,%u,%u,%u,%u,", &heat, &pump, &brew_session,
                      &waitfortemp, &waitforinput, &ret.substage, &ret.stage);
    if (obj_read != 7) {
      printf("BrewState YParsing error.\n");
      return ret;
    }
    ret.heater_on = (heat == 1);
    ret.pump_on = (pump == 1);
    ret.brew_session_loaded = (brew_session == 1);
    ret.waiting_for_temp = (waitfortemp == 1);
    ret.waiting_for_input = (waitforinput == 1);

    unsigned timer_paused;
    obj_read = sscanf(in + 51, "W%f,%u", &ret.percent_heating, &timer_paused);
    if (obj_read != 2) {
      printf("BrewState WParsing error.\n");
      return ret;
    }
    ret.timer_paused = (timer_paused == 1);
    ret.valid = true;
    return ret;
  }

  using StateTransition = bool (*)(const BrewState&, const BrewState&);
  using TriggerFunc = void (*)(const BrewState&);
  struct ConditionalFunc {
    bool repeat = false;
    StateTransition condition;
    TriggerFunc callback;
  };

  std::list<ConditionalFunc> triggers_;

 public:
  // Read status
  void ReadStatusThread() {
    while (!quit_now_) {
      // Read until we get to the start bit: 'T'
      int first_byte;
      do {
        first_byte = fin.get();
      } while (first_byte != (int)kStartChar);
      char ret[kStatusLength];
      ret[0] = kStartChar;
      int chars_read = 1;
      do {
        fin.read(ret + chars_read, kStatusLength - chars_read);
        chars_read += fin.gcount();
      } while (chars_read < kStatusLength);
      // Now we have the correct number of chars, aligned correctly.
      //See if it parses:
      BrewState bs = ParseState(ret);
      if (bs.valid) {
        previous_state_ = latest_state_;
        latest_state_ = bs;
        if (previous_state_.valid) {
          // signal conditionals
        }
      }
    } // end while
  }
#if 0
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
#endif
};



