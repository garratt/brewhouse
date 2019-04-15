// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <iostream>
#include <thread>

#include "winch.h"


class UserInterface {
  bool disable_for_test_ = false;
  int WaitForEnter() {
    if (disable_for_test_) {
      std::cout << "---- User Input disabled for testing ----" << std::endl;
      return 0;
    }
      char buf[256];
      std::cout << "Press [Enter] when done, or [q] to quit." << std::endl;
      std::cin.getline (buf,256);
      if (strlen(buf) > 0) {
        std::cout << "Really Quit? [y/N]" << std::endl;
        std::cin.getline (buf,256);
        if (strlen(buf) > 0 && (buf[0] == 'y' || buf[0] == 'Y')) {
          std::cout << "Okay, quitting." <<std::endl;
          return -1;
        }
      }
      std::cout << "Great, thanks!  Moving on..." <<std::endl;
      return 0;
    }

  // std::thread ui_thread_;

  public:

   void DisableForTest() { disable_for_test_ = true;}

    // Ask the user to fill with water
    int PleaseFillWithWater(double initial_volume_liters) {
      std::cout << "Please fill the Grainfather with " << initial_volume_liters;
      std::cout << " liters of water." << std::endl;
      return WaitForEnter();
      // ui_thread_ = std::thread([on_done]() { on_done(WaitForEnter()); });
    }

    // Ask the user to add hops to basket
    int PleaseAddHops(double grams, std::string type) {
      std::cout << "Please Add " << grams << " grams of " << type;
      std::cout << " hops into the basket" << std::endl;
      return WaitForEnter();
    }

    // Ask the user to position the winches so the mash tun is in
    // the grainfather
    int PleasePositionWinches() {
      std::cout << "Please position the winches so that the hops basket\n";
      std::cout << " is at the trolly, the trolley is against the left stop,\n";
      std::cout << " and the mash tun is in the Grainfather."<< std::endl;
      std::cout << "You can control the winch through this terminal."<< std::endl;
      std::cout << "[lrb][udlr]<ms>"<< std::endl;
      std::cout << "  |------------ activate [l]eft, [r]ight or [b]oth winches"<< std::endl;
      std::cout << "      |-------- go [u]p, [d]own, or for both: [l]eft or [r]ight.s"<< std::endl;
      std::cout << "            |-- the time to leave the winches on."<< std::endl;
      char buf[256];
      if (disable_for_test_) {
        std::cout << "---- User Input disabled for testing ----" << std::endl;
        return 0;
      }
      std::cout << "Press [Enter] when done, or [q] to quit." << std::endl;
      WinchController winch;
      while(1) {
        printf(">");
        std::cin.getline (buf,256);
        if (strlen(buf) > 0) {
          if (buf[0] == 'r' || buf[0] == 'l' || buf[0] == 'b') {
            winch.ManualWinchControl(buf[0], buf[1], atoi(buf + 2));

          }
          if (buf[0] == 'q') 
            return 0;
        } else {
          return 0;
        }
      }
    }

    // Ask the user to add Grain
    int PleaseAddGrain() {
      std::cout << "Please Add grains. Don't forget the adjuncts!" << std::endl;
      return WaitForEnter();
    }

    // Ask the user to add the top parts of the tun,
    // and position the hoses
    int PleaseFinalizeForMash() {
      std::cout << "Please install the top mesh, then screw in the lift bar." << std::endl;
      std::cout << "Finally, check that the Kettle hose is in the kettle," << std::endl;
      std::cout << "and the chiller hose is attached to the carboy." << std::endl;
      std::cout << "After this step, we go fully autonomous!!" << std::endl;
      return WaitForEnter();
    }
    int UpdateVolume(double current_volume_liters);



};
