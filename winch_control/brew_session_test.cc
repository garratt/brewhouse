// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brew_session.h"
#include <iostream>
int main(int argc, char **argv) {
  BrewSession session;

  // session.SetOfflineTest();
  // session.SetFakeGrainFather();
  // session.SetFakeWinch();
  // session.SetFakeScale();
  // session.SetZippyTime();
  // session.BypassUserInterface();

  session.Run("1mgoqF94u_d-Ai0f22Yza0wH4_0OSuHhLmXeSwA5GPAI");
  return 0;
}
