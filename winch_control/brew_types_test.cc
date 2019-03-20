// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "brew_types.h"

void VerifyBrewstate(BrewState bs, std::string changed) {
  BrewState bs1;
  bs1.Load(bs.ToString());
  EXPECT_EQ(bs1, bs) << "Brewstate differs when changing " << changed;
}

void VerifyBrewRecipe(BrewRecipe br, std::string changed) {
  BrewRecipe br1;
  br1.Load(br.GetSessionCommand());
  EXPECT_EQ(br1, br) << "BrewRecipe differs when changing " << changed;
}

TEST(Serialization, Brewstate) {
  BrewState bs;
  bs.valid = true;
  VerifyBrewstate(bs, "valid");
  bs.timer_on = true;
  VerifyBrewstate(bs, "timer_on");
  bs.timer_paused = true;
  VerifyBrewstate(bs, "timer_paused");
  bs.timer_seconds_left = 115;
  VerifyBrewstate(bs, "timer_seconds_left");
  bs.timer_total_seconds = 120;
  VerifyBrewstate(bs, "timer_total_seconds");
  bs.waiting_for_input = true;
  VerifyBrewstate(bs, "waiting_for_input");
  bs.waiting_for_temp = true;
  VerifyBrewstate(bs, "waiting_for_temp");
  bs.brew_session_loaded = true;
  VerifyBrewstate(bs, "brew_session_loaded");
  bs.heater_on = true;
  VerifyBrewstate(bs, "heater_on");
  bs.pump_on = true;
  VerifyBrewstate(bs, "pump_on");
  bs.target_temp = 65.3;
  VerifyBrewstate(bs, "target_temp");
  bs.current_temp = 32.5;
  VerifyBrewstate(bs, "current_temp");
  bs.percent_heating = 20;
  VerifyBrewstate(bs, "percent_heating");
  bs.stage = 3;
  VerifyBrewstate(bs, "stage");
  bs.substage = 2;
  VerifyBrewstate(bs, "substage");
  bs.read_time = 22;
  VerifyBrewstate(bs, "read_time");
  bs.valid = true;
  VerifyBrewstate(bs, "valid");
}

TEST(Serialization, BrewRecipe) {
  // Brew Recipe:
  BrewRecipe br;
  VerifyBrewRecipe(br, "nothing");
  br.boil_minutes = 5;
  VerifyBrewRecipe(br, "boil_minutes");
  br.grain_weight_grams = 15.3;
  VerifyBrewRecipe(br, "grain_weight_grams");
  br.hops_grams = 25.6;
  VerifyBrewRecipe(br, "hops_grams");
  br.initial_volume_liters = 5.4;
  VerifyBrewRecipe(br, "initial_volume_liters");
  br.sparge_liters = 5.2;
  VerifyBrewRecipe(br, "sparge_liters");
  br.session_name = "mytest brew";
  VerifyBrewRecipe(br, "session_name");
  br.hops_type = "hoppy mchopface";
  VerifyBrewRecipe(br, "hops_style");
  br.mash_temps.push_back(30.2);
  br.mash_times.push_back(22);
  VerifyBrewRecipe(br, "mash step 1");
  br.mash_temps.push_back(46.2);
  br.mash_times.push_back(25);
  VerifyBrewRecipe(br, "mash step 2");
  br.mash_temps.push_back(80.6);
  br.mash_times.push_back(52);
  VerifyBrewRecipe(br, "mash step 3");
}
