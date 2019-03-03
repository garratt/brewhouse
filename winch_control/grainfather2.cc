// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpio.h"
#include "grainfather2.h"
#include <utility>
#include <stdio.h>      // standard input / output functions
#include <stdlib.h>
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions
#include <iostream>
#include <mutex>



// Gets the latest state.  If |prev_read| == 0,
// just pulls the value of latest_state_ in a protected fashion.
// Otherwise, waits until a state is available
BrewState GrainfatherSerial::GetLatestState(int64_t prev_read) {
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

// Runs a command, and ensures that is completes successfully.  Blocks until
// a reading is performed, so could block up to 2 seconds.
// returns 0 if the brewstate is valid and the verify condition is true, either
//           already, or after the command
// returns -1 for all errors
int GrainfatherSerial::CommandAndVerify(const char *command, bool (*verify_condition)(BrewState)) {
  if (disable_for_test_) return 0; //TODO: actually change state to simulate brew
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

int GrainfatherSerial::TurnPumpOn() {
  return CommandAndVerify(kPumpOnString, [](BrewState bs) {return bs.pump_on; });
}
int GrainfatherSerial::TurnPumpOff() {
  return CommandAndVerify(kPumpOffString, [](BrewState bs) {return !bs.pump_on; });
}
int GrainfatherSerial::TurnHeatOn() {
  return CommandAndVerify(kHeatOnString, [](BrewState bs) {return bs.heater_on; });
}
int GrainfatherSerial::TurnHeatOff() {
  return CommandAndVerify(kHeatOffString, [](BrewState bs) {return !bs.heater_on; });
}
int GrainfatherSerial::QuitSession() {
  return CommandAndVerify(kQuitSessionString,
      [](BrewState bs) {return !bs.brew_session_loaded; });
}
int GrainfatherSerial::AdvanceStage() {
  return CommandAndVerify(kSetButtonString,
      [](BrewState bs) {return !bs.waiting_for_input; });
}
int GrainfatherSerial::PauseTimer() {
  return CommandAndVerify(kPauseTimerString,
      [](BrewState bs) {return !bs.timer_on || bs.timer_paused; });
}
int GrainfatherSerial::ResumeTimer() {
  return CommandAndVerify(kResumeTimerString,
      [](BrewState bs) {return !bs.timer_on || !bs.timer_paused; });
}

int GrainfatherSerial::LoadSession(const char *session_string) {
  int ret = QuitSession(); // make sure there is no current session
  if (ret) return ret;
  return CommandAndVerify(session_string,
      [](BrewState bs) {return bs.brew_session_loaded; });
}

int GrainfatherSerial::TestCommands() {
  // SetFlow(NO_PATH);
  if (TurnHeatOn() < 0) { printf("Failed to TurnHeatOn\n"); return -1; }
  if (TurnHeatOff() < 0) { printf("Failed to TurnHeatOff\n"); return -1; }
  if (TurnPumpOn() < 0) { printf("Failed to TurnPumpOn\n"); return -1; }
  if (TurnPumpOff() < 0) { printf("Failed to TurnPumpOff\n"); return -1; }
  // Now load a session
   const char *session_string = "R15,2,14.3,14.6,   "
   "0,1,1,0,0,         "
   "TEST CONTROLA      "
   "0,1,0,0,           "
   "1,                 "
   "5:16,              "
   "66:60,             ";
  if (LoadSession(session_string) < 0) { printf("Failed to LoadSession\n"); return -1; }
  // Advance to start heating.  It should be at temperature,
  // so it will go to the user input for starting mash
  if (AdvanceStage() < 0) { printf("Failed to AdvanceStage\n"); return -1; }
  // Advance to start mashing. Timer should be on
  if (AdvanceStage() < 0) { printf("Failed to AdvanceStage\n"); return -1; }
  if (PauseTimer() < 0) { printf("Failed to PauseTimer\n"); return -1; }
  if (ResumeTimer() < 0) { printf("Failed to ResumeTimer\n"); return -1; }
  if (QuitSession() < 0) { printf("Failed to QuitSession\n"); return -1; }
  return 0;
}

// Brew State:
// T<timer_on>,<min_left+1>,<total_min>,<sec_left>
// X<target_temp>,<current_temp>,
// Y<heater_on>,<pump_on>,<brew_session_loaded>,<waiting_for_temp>,<waitingforinput>,<substage>,<stage>,0,
// W<percent_heating>,<timer_paused>,0,1,0,1,



BrewState GrainfatherSerial::ParseState(char in[kStatusLength]) {
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
  ret.read_time = GetTimeMsec();
  ret.valid = true;
  return ret;
}

void GrainfatherSerial::RegisterBrewStateCallback(std::function<void(BrewState)> callback) {
  brew_state_callback_ = callback;
}


int GrainfatherSerial::Init(std::function<void(BrewState)> callback) {
  brew_state_callback_ = callback;
}


// Read status
void GrainfatherSerial::ReadStatusThread() {
  while (!quit_now_) {
    // Read until we get to the start bit: 'T'
    int first_byte = '\0';
    int current_read;
    do {
      current_read = read(fd_, &first_byte, 1);
      if (current_read < 0) {
        printf("Failed to Read\n");
        break;
      }
    } while (first_byte != (int)kStartChar);
    if (current_read < 0) {
      // If we are having read problems, raise flag and keep trying
      read_error_ = true;
      continue;
    }
    char ret[kStatusLength];
    ret[0] = kStartChar;
    int chars_read = 1;
    do {
      current_read = read(fd_, ret + chars_read, kStatusLength - chars_read);
      if (current_read < 0) {
        printf("Failed to Read\n");
        break;
      }
      chars_read += current_read;
    } while (chars_read < kStatusLength);
    if (current_read < 0) {
      // If we are having read problems, raise flag and keep trying
      read_error_ = true;
      continue;
    }
    // Now we have the correct number of chars, aligned correctly.
    // See if it parses:
    BrewState bs = ParseState(ret);
    if (bs.valid) {
      read_error_ = false;
      std::lock_guard<std::mutex> lock(state_mutex_);
      previous_state_ = latest_state_;
      latest_state_ = bs;
      if (previous_state_.valid) {
        // signal conditional variable
      }
    }
  } // end while
}


int GrainfatherSerial::Connect(const char *path) {
  fd_ = open(path, O_RDWR | O_NOCTTY); //TODO: use O_SYNC?
  if (fd_ <= 0) {
    printf("Failed to open serial device %s\n", path);
    return -1;
  }
  // Now configure:
  struct termios tty;
  if (tcgetattr ( fd_, &tty ) != 0 ) {
    std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
    return -1;
  }

  // Set Baud Rate
  cfsetospeed (&tty, (speed_t)B9600);
  cfsetispeed (&tty, (speed_t)B9600);

  // Setting other Port Stuff
  tty.c_cflag     &=  ~CSTOPB;
  tty.c_cflag     &=  ~CRTSCTS;           // no flow control
  tty.c_cc[VMIN]   =  1;                  // read doesn't block
  tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
  tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

  /* Make raw */
  cfmakeraw(&tty);

  /* Flush Port, then applies attributes */
  tcflush( fd_, TCIFLUSH );
  if (tcsetattr ( fd_, TCSANOW, &tty ) != 0) {
    std::cout << "Error " << errno << " from tcsetattr" << std::endl;
    return -1;
  }
  return 0;
}

int GrainfatherSerial::SendSerial(std::string to_send) {
  int n_written = write(fd_ , to_send.c_str(), to_send.size());
  if (n_written != to_send.size()) {
    printf("Failed to write!\n");
    return -1;
  }
  // TODO: either use O_SYNC, or store time when we believe write will be done
  // so we don't collide.
  usleep(1500 * to_send.size());
  return 0;
}
