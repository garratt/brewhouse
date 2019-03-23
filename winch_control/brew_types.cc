// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brew_types.h"

#include <deque>
#include <iostream>
#include <vector>

#include <sys/time.h>   // gettimeofday

int64_t GetTimeMsec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void BrewRecipe::Print() const {
  std::cout << " brew session: " << session_name << std::endl;
  std::cout << " boil time: " << boil_minutes << std::endl;
  std::cout << " Grain Weight: " << grain_weight_grams << std::endl;
  std::cout << " Initial Water: " << initial_volume_liters << std::endl;
  std::cout << " Sparge Volume: " << sparge_liters << std::endl;
  std::cout << " mash steps:: " << std::endl;
  for (unsigned i = 0; i < mash_temps.size(); ++i) {
    printf("  %2.2f C, %d minutes\n", mash_temps[i], mash_times[i]);
  }
}

// This creates the string which is passed to the Grainfather to
// Load a session
std::string BrewRecipe::GetSessionCommand() const {
  std::string ret;
  char buffer[20];
  snprintf(buffer, 20, "R%u,%lu,%2.1f,%2.1f,                  ", boil_minutes,
           mash_temps.size(), initial_volume_liters, sparge_liters);
  ret += buffer;
  // convert name string to all caps
  snprintf(buffer, 20, "%s                           ", session_name.c_str());
  ret += "0,1,1,0,0,         ";
  ret += buffer;
  ret += "0,0,0,0,           ";  // second number is number of additions
  // we would put in addition times here, but they don't change the heating
  for (unsigned i = 0; i < mash_temps.size(); ++i) {
    snprintf(buffer, 20, "%2.1f:%u,                      ", mash_temps[i],
             mash_times[i]);
    ret += buffer;
  }
  return ret;
}

int BrewRecipe::Load(const std::string &in) {
  std::string ret;
  unsigned mash_steps;
  sscanf(in.c_str(), "R%u,%u,%lf,%lf,", &boil_minutes,
      &mash_steps, &initial_volume_liters, &sparge_liters);
  // second line is: "0,1,1,0,0,         ";
  // Third line in name:
  session_name = in.substr(19*2, 19);
  size_t pos = session_name.find_last_not_of(" "); // find last non-space in name
  if (pos != std::string::npos)
    session_name.erase(pos + 1);
  else
    session_name.clear();            // session name is all whitespace
  // Fourth line is: "0,0,0,0,           ";  // second number is number of additions
  // we would put in addition times here, but they don't change the heating
  // fifth line, assuming no additions, is the mash steps:
  mash_temps.clear();
  mash_times.clear();
  for (unsigned i = 0; i < mash_steps; ++i) {
    unsigned offset = 19 * (4 + i);
    double temp;
    uint32_t mtime;
    sscanf(in.c_str() + offset, "%lf:%u,", &temp, &mtime);
    mash_temps.push_back(temp);
    mash_times.push_back(mtime);
  }
  return 0;
}

// just check the stuff that gets loaded
bool BrewRecipe::operator==(const BrewRecipe &other) const {
  if (session_name != other.session_name) return false;
  if (mash_temps.size() != other.mash_temps.size()) return false;
  if (mash_times.size() != other.mash_times.size()) return false;
  for (unsigned i = 0; i < mash_temps.size(); ++i) {
    if (mash_temps[i] != other.mash_temps[i]) return false;
    if (mash_times[i] != other.mash_times[i]) return false;
  }
  if (boil_minutes != other.boil_minutes) return false;
  if (initial_volume_liters != other.initial_volume_liters) return false;
  if (sparge_liters != other.sparge_liters) return false;
  return true;
}
bool BrewRecipe::operator!=(const BrewRecipe &other) const {
  return !(*this == other);
}

bool BrewState::operator!=(const BrewState& other) const {
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
bool BrewState::operator==(const BrewState &other) const {
  return !(*this != other);
}

std::string BrewState::ToString() const {
  char ret[17*4+10];
  // Each of these strings overruns the length, but is overwritten by the next:
  int sec_left = timer_seconds_left == 0? 0 : timer_seconds_left % 60 + 1;
  int min_left = timer_seconds_left == 0? 0 : timer_seconds_left / 60 + 1;

  sprintf(ret,"T%d,%d,%d,%d,ZZZZZZZZZZ", timer_on?1:0,
      min_left, timer_total_seconds / 60, sec_left);
  sprintf(ret+17, "X%2.1f,%2.1f,ZZZZZZZZZZZ", target_temp, current_temp);
  sprintf(ret+34, "Y%d,%d,%d,%d,%d,%u,%u,ZZZZZ", heater_on?1:0, pump_on?1:0,
      brew_session_loaded ? 1 : 0, waiting_for_temp ? 1 : 0,
      waiting_for_input ? 1 : 0, substage, stage);
  sprintf(ret + 51, "W%d,%u,0,1,0,1,ZZZZZZZ", (int)percent_heating,
      timer_paused ? 1 : 0);
  ret[68] = '\0';
  return std::string(ret);
}


int BrewState::Load(std::string in) {
  //T1,1,2,60,ZZZZZZZX19.0,19.1,ZZZZZZY1,1,1,0,0,0,1,0,W0,0,0,1,0,1,ZZZZ
  int min_left, sec_left, total_min, _timer_on, obj_read;
  obj_read = sscanf(in.c_str(), "T%d,%d,%d,%d", &_timer_on, &min_left, &total_min, &sec_left);
  if (obj_read != 4) {
    printf("BrewState TParsing error.\n");
    return -1;
  }
  timer_on = (_timer_on == 1);
  timer_seconds_left = 0;
  if (min_left > 0)
    timer_seconds_left += (min_left - 1) * 60;
  if (sec_left > 0)
    timer_seconds_left += sec_left - 1;
  timer_total_seconds = 60 * total_min;

  obj_read = sscanf(in.c_str() + 17, "X%lf,%lf,", &target_temp, &current_temp);
  if (obj_read != 2) {
    printf("BrewState XParsing error.\n");
    return -1;
  }
  unsigned heat, pump, brew_session, waitfortemp, waitforinput;
  obj_read = sscanf(in.c_str() + 34, "Y%u,%u,%u,%u,%u,%u,%u,", &heat, &pump, &brew_session,
      &waitfortemp, &waitforinput, &substage, &stage);
  if (obj_read != 7) {
    printf("BrewState YParsing error.\n");
    return -1;
  }
  heater_on = (heat == 1);
  pump_on = (pump == 1);
  brew_session_loaded = (brew_session == 1);
  waiting_for_temp = (waitfortemp == 1);
  waiting_for_input = (waitforinput == 1);

  unsigned _timer_paused;
  obj_read = sscanf(in.c_str() + 51, "W%lf,%u", &percent_heating, &_timer_paused);
  if (obj_read != 2) {
    printf("BrewState WParsing error.\n");
    return -1;
  }
  timer_paused = (_timer_paused == 1);
  read_time = GetTimeMsec();
  valid = true;
  return 0;
}

void BrewState::Print() const {
  std::cout << "read_time " << read_time << std::endl;
  std::cout << "timer_on " << timer_on << std::endl;
  std::cout << "timer_paused " << timer_paused << std::endl;
  std::cout << "timer_seconds_left " << timer_seconds_left << std::endl;
  std::cout << "timer_total_seconds " << timer_total_seconds << std::endl;
  std::cout << "waiting_for_input " << waiting_for_input << std::endl;
  std::cout << "waiting_for_temp " << waiting_for_temp << std::endl;
  std::cout << "brew_session_loaded " << brew_session_loaded << std::endl;
  std::cout << "heater_on " << heater_on << std::endl;
  std::cout << "pump_on " << pump_on << std::endl;
  std::cout << "current_temp " << current_temp << std::endl;
  std::cout << "target_temp " << target_temp << std::endl;
  std::cout << "percent_heating " << percent_heating << std::endl;
  std::cout << "stage " << stage << std::endl;
  std::cout << "substage " << substage << std::endl;
  std::cout << "read_time " << read_time << std::endl;
  std::cout << "valid " << valid << std::endl;
}
