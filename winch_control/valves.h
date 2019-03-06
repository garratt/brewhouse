// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "gpio.h"


enum FlowPath {NO_PATH, KETTLE, CHILLER, CARBOY};

int SetFlow(FlowPath path);


int ActivateChillerPump();
int DeactivateChillerPump();


void Test_Valves(char valve_arg);
