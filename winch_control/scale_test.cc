// Copyright 2019 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "scale.h"
int main(int argc, char **argv) {
 while (1) {
    WaitForHX711();
    std::cout << ReadHX711Data() << std::endl;
 }
  return 0;
}
