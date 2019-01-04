// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/*
 *  Winch control test
 *
 */
#include <time.h>
#include "relay.h"

// The two ftdi 4 channel relay boards:
const char* kLeftSerial = "AI04XORW";
const char* kRightSerial = "A505FQAL";

void PrettyTime() {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char s[64];
  strftime(s, sizeof(s), "%c", tm);
  printf("%s\n", s);
}


// Reel the winch in, running for |ms| milliseconds.
// the serial number can be specified using serial.
void GoUp(uint32_t ms, const char* serial = "") {
   SetRelay(0, serial); // reset / set direction
   usleep(100000); // let the relay click around
   SetRelay(0x06, serial); // enable
   usleep(1000 * ms); // run for given amount of time
   SetRelay(0, serial); // reset / set direction
   usleep(100000); // let the relay click around
}

// Reel the winch out, running for |ms| milliseconds.
// the serial number can be specified using serial.
void GoDown(uint32_t ms, const char* serial = "") {
   // SetRelay(0, serial); // reset / set direction
   usleep(100000); // let the relay click around
   SetRelay(0x09, serial); // set direction
   usleep(100000); // let the relay click around
   SetRelay(0x0f, serial); // enable
   usleep(1000 * ms); // run for 1.5 seconds
   SetRelay(0, serial); // reset / set direction
   usleep(100000); // let the relay click around
}

// USAGE:
// relay_test [L | [l | r][u | d] <ms_to_run>]
// L - list devices
// l/r left or right winch
// u/d up or down
// Example: relay_test lu 1000 
//    To get make the left winch go up for 1 second.


int main(int argc, char **argv) {

  if (argc < 2) {
    DisconnectUSB();
    SetRelay(0, kLeftSerial);
    SetRelay(0, kRightSerial);
    return 0;
  }

  if (argv[1][0] == 'L') {
    ListDevs();
    return 0;
  }

  const char *serial = "";
  int index = 0;

  if (argv[1][0] == 'l') {
    serial = kLeftSerial;
    index++;
  }
  if (argv[1][0] == 'r') {
    serial = kRightSerial;
    index++;
  }

  // Default to 200ms, which is ~8cm.
  uint32_t ms = 200;  
  if (argc > 2) {
   int ms_in = atoi(argv[2]);
   ms = (ms_in < 0) ? 0 : ms_in;
  }

  if (argv[1][index] == 'u') {
    GoUp(ms, serial);
    return 0;
  }

  if (argv[1][index] == 'd') {
    GoDown(ms, serial);
    return 0;
  }


  return 0;
// Not doing anything else at the moment.

  if (strlen(argv[1]) < 4) {
    DisconnectUSB();
    return 0;
  }

  uint8_t state = 0;
  for (int i = 0; i < 4; ++i) {
    if (argv[1][i] == '1') {
      state += (1 << i);
    }
  }
  SetRelay(state);
  return 0;


  DisconnectUSB();
  usleep(600000); // let the relay click around
  while(1) {
    if(ConnectUSB()) {
      printf("Error with Relay\n");
      return -1;
    }
    usleep(900000); // let the relay click around
    DisconnectUSB();
    usleep(600000); // let the relay click around

  }
  return 0;
}





