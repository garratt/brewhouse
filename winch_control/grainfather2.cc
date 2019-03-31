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
  BrewState latest = GetLatestState();
  std::cout<<" CommandAndVerify: initial state: "<< latest.ToString() <<std::endl;
  if (!latest.valid) return -1;
  // Already met condition, i.e. We asked to turn pump on, but it already was on.
  if (verify_condition((latest))) return 0;
  if (SendSerial(command)) {
    printf("Failed to send command '%s'\n", command);
    return -1;
  }
  int64_t command_time_ms = GetTimeMsec();
  BrewState next = GetLatestState(command_time_ms);
  std::cout<<" CommandAndVerify: after state: "<< next.ToString() <<std::endl;
  if (!next.valid) {
    printf("Failed to get another reading from Grainfather.\n");
    return -1;
  }
  // next.Print();
  // Have to have different conditions for advance...
  if (command == kSetButtonString) {
    if (!next.waiting_for_input  ||
        next.input_reason != latest.input_reason ||
        next.stage != latest.stage ) {
      return 0;
    }
  }
  if (verify_condition(next)) {
    return 0;
  }
  // Otherwise, we failed to turn pump on.
  printf("Executed command failed to change state.\n");
  return -1;
}

int GrainfatherSerial::TurnPumpOn() {
  std::cout << "Sending Command to turn pump on" << std::endl;
  return CommandAndVerify(kPumpOnString, [](BrewState bs) {return bs.pump_on; });
}
int GrainfatherSerial::TurnPumpOff() {
  std::cout << "Sending Command to turn pump off" << std::endl;
  return CommandAndVerify(kPumpOffString, [](BrewState bs) {return !bs.pump_on; });
}
int GrainfatherSerial::TurnHeatOn() {
  std::cout << "Sending Command to turn heat on" << std::endl;
  return CommandAndVerify(kHeatOnString, [](BrewState bs) {return bs.heater_on; });
}
int GrainfatherSerial::TurnHeatOff() {
  std::cout << "Sending Command to turn heat off" << std::endl;
  return CommandAndVerify(kHeatOffString, [](BrewState bs) {return !bs.heater_on; });
}
int GrainfatherSerial::QuitSession() {
  std::cout << "Sending Command to quit session" << std::endl;
  return CommandAndVerify(kQuitSessionString,
      [](BrewState bs) {return !bs.brew_session_loaded; });
}
int GrainfatherSerial::AdvanceStage() {
  std::cout << "Sending Command to advance stage" << std::endl;
  return CommandAndVerify(kSetButtonString,
      [](BrewState bs) {return !bs.waiting_for_input; });
}
int GrainfatherSerial::PauseTimer() {
  std::cout << "Sending Command to pause timer" << std::endl;
  return CommandAndVerify(kPauseTimerString,
      [](BrewState bs) {return !bs.timer_on || bs.timer_paused; });
}
int GrainfatherSerial::ResumeTimer() {
  std::cout << "Sending Command to resume timer" << std::endl;
  return CommandAndVerify(kResumeTimerString,
      [](BrewState bs) {return !bs.timer_on || !bs.timer_paused; });
}

int GrainfatherSerial::LoadSession(const char *session_string) {
  // std::cout << "Sending Command to load session " << session_string << std::endl;
  int ret = QuitSession(); // make sure there is no current session
  if (ret) return ret;
  return CommandAndVerify(session_string,
      [](BrewState bs) {return bs.brew_session_loaded; });
}

int GrainfatherSerial::TestCommands() {
  testing_communications_ = true;
  std::cout << "++++++++++++  Running Test Commands ++++++++++++++" << std::endl;
  // SetFlow(NO_PATH);
  if (TurnHeatOn() < 0) { printf("Failed to TurnHeatOn\n"); return -1; }
  if (TurnHeatOff() < 0) { printf("Failed to TurnHeatOff\n"); return -1; }
  if (TurnPumpOn() < 0) { printf("Failed to TurnPumpOn\n"); return -1; }
  if (TurnPumpOff() < 0) { printf("Failed to TurnPumpOff\n"); return -1; }
  // Now load a session
   const char *session_string = "R15,2,14.3,14.6,   "
   "0,1,1,0,0,         "
   "TEST CONTROLA      "
   "0,0,0,0,           "
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
  std::cout << "+++++++++ Done Running Test Commands ++++++++++++++" << std::endl;
  testing_communications_ = false;
  // TODO: we don't set testing_communications_ to false if we fail, but
  // we should be stopping everything anyway at that point.
  return 0;
}

// Brew State:
// T<timer_on>,<min_left+1>,<total_min>,<sec_left>
// X<target_temp>,<current_temp>,
// Y<heater_on>,<pump_on>,<brew_session_loaded>,<waiting_for_temp>,<waitingforinput>,<input_reason>,<stage>,0,
// W<percent_heating>,<timer_paused>,0,1,0,1,


// callback could be a nullptr, I don't care here
int GrainfatherSerial::Init(std::function<void(BrewState)> callback) {
  brew_state_callback_ = callback;
  if (!disable_for_test_) {
    int ret = Connect("/dev/ttyUSB0");
    if (ret < 0) {
      printf("Failed to connect to port\n");
      return ret;
    }
  }
  reading_thread_enabled_ = true;
  reading_thread_ = std::thread(&GrainfatherSerial::ReadStatusThread, this);

  BrewState bs = GetLatestState();
  int quit_counter = 0;
  while (!bs.valid) {
    usleep(300000);
    bs = GetLatestState();
    quit_counter++;
    if (quit_counter >= 10) {
      reading_thread_enabled_ = false;
      return -1;
    }
  }
  return 0;
}

GrainfatherSerial::~GrainfatherSerial() {
  reading_thread_enabled_ = false;
  if (reading_thread_.joinable()) {
    reading_thread_.join();
  }
}

// Read status
void GrainfatherSerial::ReadStatusThread() {
  while (reading_thread_enabled_) {
    if (disable_for_test_) {
      usleep(300000);
      BrewState bs = simulated_grainfather_.ReadState();
      if (bs.valid) {
        {
          std::lock_guard<std::mutex> lock(state_mutex_);
          latest_state_ = bs;
        }
        // std::cout<<bs.ToString()<<std::endl;
        if (brew_state_callback_) {
          brew_state_callback_(bs);
        }
      }
      continue;
    }

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
    unsigned chars_read = 1;
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
    BrewState bs;
    if (bs.Load(ret) == 0) {
      read_error_ = false;
      {
      std::lock_guard<std::mutex> lock(state_mutex_);
      latest_state_ = bs;
      }
      if (brew_state_callback_ && !testing_communications_) {
        brew_state_callback_(bs);
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
  // std::cout << to_send << std::endl;
  if (disable_for_test_) {
    simulated_grainfather_.ReceiveSerial(to_send.c_str());
    return 0;
  }
  unsigned n_written = write(fd_ , to_send.c_str(), to_send.size());
  if (n_written != to_send.size()) {
    printf("Failed to write!\n");
    return -1;
  }
  // TODO: either use O_SYNC, or store time when we believe write will be done
  // so we don't collide.
  usleep(15000 * to_send.size());
  return 0;
}




bool GrainfatherSerial::IsMashTemp() {
      std::lock_guard<std::mutex> lock(state_mutex_);
  return latest_state_.input_reason == BrewState::InputReason::StartMash;
}
bool GrainfatherSerial::IsMashDone() {
      std::lock_guard<std::mutex> lock(state_mutex_);
  return latest_state_.input_reason == BrewState::InputReason::StartSparge;
}
bool GrainfatherSerial::IsBoilTemp() {
      std::lock_guard<std::mutex> lock(state_mutex_);
  return latest_state_.input_reason == BrewState::InputReason::StartBoil;
}
bool GrainfatherSerial::IsBoilDone() {
      std::lock_guard<std::mutex> lock(state_mutex_);
  return latest_state_.input_reason == BrewState::InputReason::FinishSession;
}
bool GrainfatherSerial::IsInSparge() {
      std::lock_guard<std::mutex> lock(state_mutex_);
  return latest_state_.input_reason == BrewState::InputReason::FinishSparge;
}

int GrainfatherSerial::StartMash() {
  if (!IsMashTemp()) {
    printf("GrainfatherSerial::StartMash: in wrong state!\n");
    return -1;
  }
  if (AdvanceStage()) return -1;
  return 0;
}



int GrainfatherSerial::StartSparge() {
  // Check that we are at end of mash
  if (!IsMashDone()) {
    printf("GrainfatherSerial::StartSparge: in wrong state!\n");
    return -1;
  }
  if (TurnPumpOff()) return -1;
  if (AdvanceStage()) return -1;
  return 0;

}

int GrainfatherSerial::HeatToBoil() {
  if (!IsInSparge()) {
    printf("GrainfatherSerial::HeatToBoil: in wrong state!\n");
    return -1;
  }
  if (AdvanceStage()) return -1;
  return 0;
}


int GrainfatherSerial::StartBoil() {
  if (!IsBoilTemp()) {
    printf("GrainfatherSerial::StartBoil: in wrong state!\n");
    return -1;
  }
  if (AdvanceStage()) return -1;
  return 0;
}

