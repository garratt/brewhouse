// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SimulatedGrainfather.h"

#define DEBUG_LOG(x) printf(x)


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
    case 'R':
      BrewRecipe recipe;
      if (recipe.Load(serial_in) == 0) {
        LoadSession(recipe);
      } else {
        printf("Failed to load recipe!\n");
      }
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
  current_state_.current_temp = 40.0;
  current_state_.target_temp = 60.0;
  current_state_.percent_heating = 0;
  current_state_.stage = 0;
  current_state_.substage = 0;
}

void SimulatedGrainfather::LoadSession(BrewRecipe recipe) {
  recipe_ = recipe;
  current_state_.brew_session_loaded = true;
  current_state_.waiting_for_input = true;
  waiting_for_start_heating = true;
  DEBUG_LOG("Waiting for input to start heating\n");
  current_state_.target_temp = recipe_.mash_temps[0];
}

void SimulatedGrainfather::Advance() {
  if (!current_state_.waiting_for_input) {
    DEBUG_LOG("Asked to advance, but not waiting for input\n");
    return;
  }
  current_state_.waiting_for_input = false;
  if (waiting_for_start_heating) {
    DEBUG_LOG("Advancing to start heating\n");
    current_state_.heater_on = true;
    current_state_.waiting_for_temp = true;
    current_state_.target_temp = recipe_.mash_temps[0];
    DEBUG_LOG("Heating for first mash temp\n");
    current_state_.stage = 1;
    waiting_for_start_heating = false;
    return;
  }
  if (waiting_for_mash_start) {
    DEBUG_LOG("Advancing to start mashing\n");
    current_state_.timer_total_seconds = recipe_.mash_times[0];
    current_state_.timer_seconds_left = recipe_.mash_times[0];
    current_state_.timer_on = true;
    DEBUG_LOG("Starting timer for mash\n");
    waiting_for_mash_start = false;
    return;
  }
  if (waiting_for_start_sparge) {
    DEBUG_LOG("Advancing to start sparging\n");
    current_state_.waiting_for_input = true;
    DEBUG_LOG("Waiting for input to finish sparge\n");
    waiting_for_sparge_done = true;
    waiting_for_start_sparge = false;
    current_state_.substage++;   // TODO: verify that it incriments here...
    return;
  }
  if (waiting_for_sparge_done) {
    DEBUG_LOG("Advancing to finish sparging\n");
    current_state_.heater_on = true;
    current_state_.stage++;
    current_state_.target_temp = boil_temp_;
    current_state_.waiting_for_temp = true;
    DEBUG_LOG("Heating for boil temp\n");
    waiting_for_sparge_done = false;
    return;
  }
  if (waiting_for_start_boil) {
    DEBUG_LOG("Advancing to start boiling\n");
    current_state_.timer_total_seconds = recipe_.boil_minutes;
    current_state_.timer_seconds_left = recipe_.boil_minutes;
    current_state_.timer_on = true;
    DEBUG_LOG("Starting timer for boil\n");
    waiting_for_start_boil = false;
    return;
  }
  if (waiting_for_boil_done) {
    DEBUG_LOG("Advancing to finish boil\n");
    // not much to do here, just a confirmation.
    waiting_for_boil_done = false;
    Reset();
  }
}

void SimulatedGrainfather::OnDoneHeating() {
  current_state_.waiting_for_temp = false;
  // Times we heat:
  unsigned ms = current_state_.stage;
  // Heat for initial mash
  if (ms == 1) {
    // now we wait for input
    current_state_.substage = 2;
    current_state_.waiting_for_input = true;
    DEBUG_LOG("Waiting for input to start mash\n");
    waiting_for_mash_start = true;
  }
  // Heat between mash stages
  if (ms > 1 && ms <= recipe_.mash_temps.size()) {
    // As soon as we reach temp, start timer
    current_state_.timer_total_seconds = recipe_.mash_times[ms - 1];
    current_state_.timer_seconds_left = recipe_.mash_times[ms - 1];
    current_state_.timer_on = true;
    DEBUG_LOG("Starting timer for next mash\n");
  }
  // Heat during sparge? (steps+1) do nothing.
  // Done heating to boil:
  if (ms == recipe_.mash_temps.size() + 2) {
    // now we wait for input
    current_state_.substage = 2;
    current_state_.waiting_for_input = true;
    waiting_for_start_boil = true;
    DEBUG_LOG("Waiting for input to start boil\n");
    current_state_.heater_on = true;
  }
}

void SimulatedGrainfather::OnTimerDone() {
  current_state_.timer_on = false;
  current_state_.timer_total_seconds = 0;
  current_state_.timer_seconds_left = 0;
  // Advance stage:
  unsigned ms = current_state_.stage;
  // Timing Mash stage
  // Still have more stages:
  if (ms > 0 && ms < recipe_.mash_temps.size()) {
    // switch to heating for next mash:
    current_state_.target_temp = recipe_.mash_temps[ms];
    current_state_.waiting_for_temp = true;
    DEBUG_LOG("Heating for mash temp\n");
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
    DEBUG_LOG("Waiting for input to start sparge\n");
  }
  // Timing boil
  if (ms == recipe_.mash_temps.size() + 2) {
    //We're done yo!
    current_state_.pump_on = false;
    current_state_.heater_on = false;
    current_state_.waiting_for_input = true;
    waiting_for_boil_done = true;
    DEBUG_LOG("Waiting for input to finish boil\n");
  }
}

bool SimulatedGrainfather::Update() {
  // we heat super fast, 1 degree per second
  int64_t now = GetTimeMsec();
  int64_t seconds_past = (now - current_state_.read_time) / 1000;
  if (seconds_past < 1) {
    return false;
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
  return true;
}

BrewState SimulatedGrainfather::ReadState() {
  if(Update()) {
    current_state_.valid = true;
  } else {
    current_state_.valid = false;
  }
  return current_state_;
}
