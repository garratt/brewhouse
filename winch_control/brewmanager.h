// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gpio.h"
#include "grainfather.h"
#include "scale.h"
#include "valves.h"
#include "winch.h"
#include "logger.h"
#include "brewtweeter.h"


struct WeightLimiter {
  time_t last_log_ = 0;
  time_t first_log_ = 0;
  int count_ = 0;
  double sum_ = 0;
  // if the reading is this far from the average, return the average and
  // start a new set.
  static constexpr int kMaxDeviationGrams = 30;
  // If there is a jump in weight logging, return the average and start a new set
  static constexpr int kMaxTimeJumpSeconds = 60;
  // This is the rate at which we log, even if the weight is constant
  static constexpr int kLoggingPeriodSeconds = 300; // 5 minutes
  
  bool PublishWeight(double weight_in, double *weight_out, time_t *log_time) {
    // If it is the first weight, publish it and save it.
    time_t now = time(NULL);
    if (count_ == 0) {
      last_log_ = now;
      count_ = 1;
      sum_ = weight_in;
    }
    double average = sum_ / count_;
    double dev = average > weight_in ? average - weight_in : weight_in - average;
    if (dev > kMaxDeviationGrams || difftime(now, last_log_) > kMaxTimeJumpSeconds
        || difftime(now, first_log_) > kLoggingPeriodSeconds) {
      // We always report the previous readings, and leave our current reading
      *weight_out = average;
      *log_time = last_log_;
      last_log_ = now;
      first_log_ = now;
      count_ = 1;
      sum_ = weight_in;
      return true;
    }
    // Otherwise, just record the reading
    count_++;
    sum_ += weight_in;
    return false;
  }

  double GetWeight() { return count_ ? sum_ / count_ : 0; }
};


class BrewManager {
 static constexpr double kGrainfatherWTun_Grams = 9414.0;
 static constexpr double kGrainfatherWOTun_Grams = 7144.0;
 double target_gravity_ = 1.10;
 double grain_weight_ = 0;
 BeepTracker beep_tracker_;
 BrewTweeter brew_tweeter_;
 BrewLogger brewlogger_;
 WeightFilter weight_filter_;
 WeightLimiter weight_limiter_;
 enum InterruptTrigger { NONE, SHORT_BEEP, LONG_BEEP, CONTINUOUS_BEEP, TIMER, WEIGHT};
 InterruptTrigger interrupt_trigger_;
 const char * TriggerString() {
   switch (interrupt_trigger_) {
     case NONE:
       return "No Trigger. WTF?";
     case LONG_BEEP:
       return "a long beep";
     case SHORT_BEEP:
       return "a short beep";
     case CONTINUOUS_BEEP:
       return "continous beeping";
     case TIMER:
       return "the time to expire";
     case WEIGHT:
       return "The weight to decrease below a level";
   }
   return "A case that TriggerString needs to be updated with";
 }
 
 double ToLiters(double weight, double specific_gravity, bool has_tun) {
   if (has_tun)
     return (weight-kGrainfatherWTun_Grams) / (specific_gravity * 1000.0);
   return (weight-kGrainfatherWOTun_Grams) / (specific_gravity * 1000.0);
 }

 WinchController winch_controller_; 
 double weight_trigger_level_;
 public:
  BrewManager(const char *brew_session) : brewlogger_(brew_session),
                                          weight_filter_("scale_calibration.txt") {

    std::string message = "Starting a new brew! Brewing: ";

    brew_tweeter_.Tweet(message + brew_session);
    if(InitIO() < 0) {
      printf("Failed during initialization. Make sure you can write to all gpios!\n");
    }
  }

  // waits for beeps and scale
  // returns 0 if the function stops for the correct reason (trigger event)
  int WaitForInput(uint32_t timeout_sec, InterruptTrigger new_trigger) {
    char status_msg[300];
    sprintf(status_msg, "Waiting %u minutes for %s.", timeout_sec / 60, TriggerString());
    brewlogger_.Log(1, status_msg);
    interrupt_trigger_ = new_trigger;
    time_t begin = time(NULL);
    do {
      usleep(1000);
      // Checks if the speaker is currently signalled.  Records the length
      // of a signal, and returns the beep length when the beep stops.
      // Also flags if the long "mash ready" beep occurrs, or if continous
      // beeping is detected.
      auto beep_status = beep_tracker_.CheckBeep();
      // Checks if new weight is available.  If it is, the weight is
      // read out (takes about 1 ms).  If there are enough readings, the
      // readings are filtered and a weight is produced.
      auto weight_status = weight_filter_.CheckWeight();
      if (HandleStatusUpdate(beep_status, weight_status)) {
        return 0;
      }
    } while (difftime(time(NULL), begin) < timeout_sec);
    // if we were waiting for a fixed amount of time, return 0.
    if (interrupt_trigger_ == InterruptTrigger::TIMER) {
      return 0;
    }
    return 1;
  }
  
  // Returns true if we should transition
  bool HandleStatusUpdate(BeepStatus beep_status, ScaleStatus scale_status) {
    // Log any beep:
    char status_msg[300];
    if (beep_status.state & (BeepStatus::SHORT | BeepStatus::LONG | BeepStatus::CONTINUOUS)) {
      if (beep_status.state & BeepStatus::SHORT)
        sprintf(status_msg, "Short Beep for %u ms.", beep_status.length);
      if (beep_status.state & BeepStatus::LONG)
        sprintf(status_msg, "LONG Beep for %u ms.", beep_status.length);
      if (beep_status.state & BeepStatus::CONTINUOUS)
        sprintf(status_msg, "CONTINUOUS Beep for %u ms.", beep_status.length);
      brewlogger_.Log(1, status_msg);
    }
    // Log any weight:
    if (scale_status.state & ScaleStatus::READY) {
      double logging_weight;
      time_t log_time;
      if(weight_limiter_.PublishWeight(scale_status.weight, &logging_weight, &log_time)) {
        brewlogger_.LogWeight(scale_status.weight, log_time);
      }
      if (interrupt_trigger_ == InterruptTrigger::WEIGHT &&
          scale_status.weight < weight_trigger_level_) {
         sprintf(status_msg, "Weight of %f < %f, transition triggered.",
                 scale_status.weight, weight_trigger_level_);
         brewlogger_.Log(1, status_msg);
         return true;
      }
    }
    if ((beep_status.state & BeepStatus::LONG) &&
        interrupt_trigger_ == InterruptTrigger::LONG_BEEP) {
         brewlogger_.Log(1, "Transition triggerd by long beep");
         return true;
    }
    if ((beep_status.state & BeepStatus::CONTINUOUS) &&
        interrupt_trigger_ == InterruptTrigger::CONTINUOUS_BEEP) {
         brewlogger_.Log(1, "Transition triggerd by CONTINUOUS_BEEP");
         return true;
    }
    if ((beep_status.state & BeepStatus::SHORT) &&
        interrupt_trigger_ == InterruptTrigger::SHORT_BEEP) {
         brewlogger_.Log(1, "Transition triggerd by SHORT_BEEP");
         return true;
    }
    return false;
  }

  int WaitForMashTemp() {
    return WaitForInput(60 * 60 * 3, InterruptTrigger::LONG_BEEP);  // Wait for up to 3 hours
    // TODO: Log mash started on overview page
    // This is a good measure of all the water we are mashing with
    // TODO: calculate water volume
    // return ret;
  }

  int WaitForBeeping(uint32_t minutes) {
    // 5 hours, including heating time
    return WaitForInput(60 * minutes, InterruptTrigger::CONTINUOUS_BEEP);
  }

  int WaitMinutes(uint32_t minutes) {
    return WaitForInput(60 * minutes, InterruptTrigger::TIMER);
  }

  int WaitForEmpty(double weight, uint32_t minutes) {
    weight_trigger_level_ = weight;
    return WaitForInput(60 * minutes, InterruptTrigger::WEIGHT);
  }



  int RunBrewSession() {
    if (WaitForMashTemp()) {
      return -1;
    }
    double current_weight = weight_limiter_.GetWeight();
    double weight_with_water = current_weight;
    char twitter_msg[240];
    sprintf(twitter_msg, "Mash temperature reached, volume is: %2.3f lets get mashing!", ToLiters(current_weight, 1.00, true));
    brew_tweeter_.Tweet(twitter_msg);

    SetFlow(KETTLE);
    // Wait for short beep to mean that mash has started:
    if(WaitForInput(25 * 60, InterruptTrigger::SHORT_BEEP)) {
      printf ("Never caught the short beep. Did mash start?\n");
      brew_tweeter_.Tweet("Never caught the short beep. Did mash start?\n");
      // return -1;
    }
    // Now the mash has started.  Get the weight for the grain bill
    // This call will return -1, because it is just a delay to read weight
    WaitForInput(300, InterruptTrigger::SHORT_BEEP);
    double with_grain_weight = weight_limiter_.GetWeight();
    sprintf(twitter_msg, "The Grain bill is estimated to be %2.3f kg.", (with_grain_weight - weight_with_water) / 1000.0);
    brew_tweeter_.Tweet(twitter_msg);

    // Wait for mash to complete
    if (WaitForBeeping(5 * 60)) {
      return -1;
    }
    HitButton(SET_BUTTON);
    double after_mash_weight = weight_limiter_.GetWeight();
    sprintf(twitter_msg, "Mash is done. I'm going to lift in 1 minute.  Volume loss: %2.3f L", (with_grain_weight - after_mash_weight) / 1000.0);
    brew_tweeter_.Tweet(twitter_msg);
    sleep(60);
    printf("Mash is Done! Lift and let drain\n");
    if(winch_controller_.RaiseToDrain_1() < 0) return -1;
    if(winch_controller_.RaiseToDrain_2() < 0) return -1;

    brew_tweeter_.Tweet("Okay, draining for 45 minutes.");
    if (WaitMinutes(45)) return -1;
    // Draining done.

    double after_lift_weight = weight_limiter_.GetWeight();
    sprintf(twitter_msg, "Done Draining.  I will move the mash in 3 minutes! Volume is now: %2.3f", ToLiters(after_lift_weight, target_gravity_, false));
    brew_tweeter_.Tweet(twitter_msg);
    if (WaitMinutes(3)) return -1;
    printf("Skip to Boil\n");
    HitButton(SET_BUTTON);

    if(winch_controller_.MoveToSink() < 0) return -1;

    // Wait for beeping, which indicates boil reached
    if (WaitForBeeping(90)) return -1;
    double before_boil_weight = weight_limiter_.GetWeight();
    sprintf(twitter_msg, "Boil temperature reached, the boil is on. Pre Boil Volume is: %2.3f", ToLiters(before_boil_weight, target_gravity_, false));
    brew_tweeter_.Tweet(twitter_msg);
    HitButton(SET_BUTTON);
    printf("Boil Reached\n");

    if (winch_controller_.LowerHops() < 0) return -1;

    HitButton(PUMP_BUTTON);
    HitButton(SET_BUTTON);

    // Wait for boil to complete:
    if (WaitForBeeping(60)) return -1;
    HitButton(SET_BUTTON);
    printf("Boil Done\n");

    WaitMinutes(3);
    winch_controller_.RaiseHops();
    // Wait to get some more weight readings
    WaitMinutes(3);
    double after_boil_weight = weight_limiter_.GetWeight();
    sprintf(twitter_msg, "Boil is complete. Post Boil Volume is: %2.3f, loss: %2.3f", ToLiters(after_boil_weight, target_gravity_, false), (after_boil_weight - before_boil_weight) / 1000.0);
    brew_tweeter_.Tweet(twitter_msg);
    SetFlow(CHILLER);
    // SetFlow(CARBOY);
    ActivateChillerPump();


    printf("Cooling Wort into Carboy\n");
    brew_tweeter_.Tweet("Boil is complete!");
    // Decant:
    HitButton(PUMP_BUTTON);

    if (WaitForEmpty(10000, 30)) return -1;
    HitButton(PUMP_BUTTON);
    DeactivateChillerPump();
    WaitMinutes(2);
    brew_tweeter_.Tweet("I'm done here. Come clean me out!");
    return 0;
  }


};

