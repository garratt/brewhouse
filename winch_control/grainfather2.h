// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"


struct SessionInfo {
  unsigned boil_minutes = 0;
  std::vector<pair<double, uint32_t>> mash_steps;
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

class Grainfather {
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
  std::fstream fin;

  BrewState ParseState(char in[kStatusLength]) {
    //T1,1,2,60,ZZZZZZZX19.0,19.1,ZZZZZZY1,1,1,0,0,0,1,0,W0,0,0,1,0,1,ZZZZ
    BrewState ret;
    int min_left, sec_left, total_min, timer_on, obj_read;
    obj_read = sscanf(in, "T%d,%d,%d,%d", &timer_on, &min_left, &total_min, &sec_left);
    if (obj_read != 4) {
      printf("Brewstate TParsing error.\n");
      return ret;
    }
    ret.timer_on = (timer_on == 1);
    ret.timer_seconds_left = (min_left - 1) * 60 + sec_left;
    ret.timer_total_seconds = 60 * total_min;

    obj_read = sscanf(in + 17, "X%f,%f,", &ret.target_temp, &ret.current_temp);
    if (obj_read != 2) {
      printf("Brewstate XParsing error.\n");
      return ret;
    }
    unsigned heat, pump, brew_session, waitfortemp, waitforinput;
    obj_read = sscanf(in + 34, "Y%u,%u,%u,%u,%u,%u,%u,", &heat, &pump, &brew_session,
                      &waitfortemp, &waitforinput, &ret.substage, &ret.stage);
    if (obj_read != 7) {
      printf("Brewstate YParsing error.\n");
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
      printf("Brewstate WParsing error.\n");
      return ret;
    }
    ret.timer_paused = (timer_paused == 1);
    ret.valid = true;
    return ret;
  }

  using StateTransition = bool (*)(const Brewstate&, const Brewstate&);
  using TriggerFunc = void (*)(const Brewstate&);
  struct ConditionalFunc {
    bool repeat = false;
    StateTransition condition;
    TriggerFunc callback;
  };

  std::list<ConditionalFunc> triggers_;
  
 public:
  // Read status
  void ReadStatus() {
    // Read until we get to the start bit: 'T'
    do {
      int first_byte = fin.get();
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
     previous_brew_state_ = current_brew_state_;
     current_brew_state_ = bs;
     if (previous_brew_state_.valid) {
        // signal conditionals
     }
   }
  }

  int RegisterCallback(void (*callback)(BrewState), StateTransition condition, bool repeat) {
   triggers_.push_back({repeat, condition, callback});
  }

  // signal on transitions
  // Register wait conditions

  // Called when stage counter ticks over to Sparge
  int OnChangeState(BrewState new_state, BrewState old_state) {
    for (int i = triggers_.size() - 1; i >= 0; --i) {
      if (triggers_[i].condition(new_state, old_state)) {
        triggers_[i].callback(new_state);
        // if it was a one shot condition, erase after the call
        if (!triggers_[i].repeat) {
          triggers_.erase(triggers_.begin() + i);
        }


  

};



