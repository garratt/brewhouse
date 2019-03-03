// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SimulatedGrainfather.h"

void SimulatedGrainfather::ReceiveSerial(const char *serial_in) {
  switch (serial_in[0]) {
    case 'L':
      if (serial_in[1] == '1')
        current_state_.pump_on = true;
      if (serial_in[1] == '0')
        current_state_.pump_on = false;
      break;
    case 'K':
      if (serial_in[1] == '1')
        current_state_.heater_on = true;
      if (serial_in[1] == '0')
        current_state_.heater_on = false;
      break;
    case 'U':
      current_state_.target_temp++;
      break;
    case 'D':
      current_state_.target_temp--;
      break;
    case 'F':
      Reset();
      break;
    case 'I':
      Advance();
      break;
    case 'G':
      TogglePause();
      break;
  }
}

void SimulatedGrainfather::TogglePause() {
  if (current_state_.timer_on) {
    if (current_state_.timer_paused) {
      current_state_.timer_paused = false;
    } else {
      current_state_.timer_paused = true;
    }
  }
}


void SimulatedGrainfather::Reset() {
  current_state_.timer_on = false;
  current_state_.timer_paused = false;
  current_state_.timer_seconds_left = 0;
  current_state_.timer_total_seconds = 0;
  current_state_.waiting_for_input = false;
  current_state_.waiting_for_temp = false;
  current_state_.brew_session_loaded = false;
  current_state_.heater_on = false;
  current_state_.pump_on = false;
  current_state_.current_temp = 10.0;
  current_state_.target_temp = 60.0;
  current_state_.percent_heating = 0;
  current_state_.stage = 0;
  current_state_.substage = 0;
}

void SimulatedGrainfather::LoadSession(BrewRecipe recipe) {
  recipe_ = recipe;
  current_state_.waiting_for_input = true;
  waiting_for_start_heating = true;
  current_state_.target_temp = recipe_.mash_temps[0];
}

void SimulatedGrainfather::Advance() {
  if (!current_state_.waiting_for_input) return;
  current_state_.waiting_for_input = false;
  if (waiting_for_start_heating) {
    current_state_.heater_on = true;
    current_state_.waiting_for_temp = true;
    current_state_.stage = 1;
    waiting_for_start_heating = false;
    return;
  }
  if (waiting_for_mash_start) {
    current_state_.timer_total_seconds = recipe_.mash_times[0];
    current_state_.timer_seconds_left = recipe_.mash_times[0];
    current_state_.timer_on = true;
    waiting_for_mash_start = false;
    return;
  }
  if (waiting_for_start_sparge) {
    current_state_.waiting_for_input = true;
    waiting_for_sparge_done = true;
    waiting_for_start_sparge = false;
    return;
  }
  if (waiting_for_sparge_done) {
    current_state_.heater_on = true;
    current_state_.stage++;
    current_state_.target_temp = boil_temp_;
    current_state_.waiting_for_temp = true;
    waiting_for_sparge_done = false;
    return;
  }
  if (waiting_for_start_boil) {
    current_state_.timer_total_seconds = recipe_.boil_minutes;
    current_state_.timer_seconds_left = recipe_.boil_minutes;
    current_state_.timer_on = true;
    waiting_for_start_boil = false;
    return;
  }
  if (waiting_for_boil_done) {
    // not much to do here, just a confirmation.
    waiting_for_boil_done = false;
  }
}

void SimulatedGrainfather::OnDoneHeating() {
  current_state_.waiting_for_temp = false;
  // Times we heat:
  int ms = current_state_.stage;
  // Heat for initial mash
  if (ms == 1) {
    // now we wait for input
    current_state_.substage = 2;
    current_state_.waiting_for_input = true;
    waiting_for_mash_start = true;
  }
  // Heat between mash stages
  if (ms > 1 && ms <= recipe_.mash_temps.size()) {
    // As soon as we reach temp, start timer
    current_state_.timer_total_seconds = recipe_.mash_times[ms - 1];
    current_state_.timer_seconds_left = recipe_.mash_times[ms - 1];
    current_state_.timer_on = true;
  }
  // Heat during sparge? (steps+1) do nothing.
  // Done heating to boil:
  if (ms == recipe_.mash_temps.size() + 2) {
    // now we wait for input
    current_state_.substage = 2;
    current_state_.waiting_for_input = true;
    waiting_for_start_boil = true;
    current_state_.heater_on = true;
  }
}

void SimulatedGrainfather::OnTimerDone() {
  current_state_.timer_on = false;
  current_state_.timer_total_seconds = 0;
  current_state_.timer_seconds_left = 0;
  // Advance stage:
  int ms = current_state_.stage;
  // Timing Mash stage
  // Still have more stages:
  if (ms > 0 && ms < recipe_.mash_temps.size()) {
    // switch to heating for next mash:
    current_state_.target_temp = recipe_.mash_temps[ms];
    current_state_.waiting_for_temp = true;
    current_state_.heater_on = true;
    current_state_.stage += 1;
  }
  // Last mash step:
  if (ms == recipe_.mash_temps.size()) {
    current_state_.target_temp = sparge_temp_;
    current_state_.waiting_for_input = true;
    current_state_.heater_on = true;
    current_state_.stage += 1;
    waiting_for_start_sparge = true;
  }
  // Timing boil
  if (ms == recipe_.mash_temps.size() + 2) {
    //We're done yo!
    Reset();
    current_state_.waiting_for_input = true;
    waiting_for_boil_done = true;
  }
}

void SimulatedGrainfather::Update() {
  // we heat super fast, 1 degree per second
  int64_t now = GetTimeMsec();
  int64_t seconds_past = (now - current_state_.read_time) / 1000;
  if (seconds_past < 1) {
    return;
  }
  current_state_.read_time += 1000 * seconds_past;

  // Heater raises temp
  if (current_state_.heater_on) {
    if (current_state_.target_temp > current_state_.current_temp) {
      current_state_.percent_heating = 100.0;
      current_state_.current_temp += 1.0 * seconds_past;
    } else {
      if (current_state_.waiting_for_temp) {
        OnDoneHeating();
      }
      current_state_.percent_heating = 50.0;
      if (current_state_.target_temp < current_state_.current_temp) {
        current_state_.current_temp -= .1;
      }
    }
  }
  // Count timer down:
  if (current_state_.timer_on && !current_state_.timer_paused) {
    if (current_state_.timer_seconds_left > seconds_past) {
      current_state_.timer_seconds_left -= seconds_past;
    } else {
      // Timer done!
      OnTimerDone();
    }
  }
}

BrewState SimulatedGrainfather::ReadState() {
  Update();
  return current_state_;
}
