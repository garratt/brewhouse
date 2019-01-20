// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logger.h"
int main(void)
{

  BrewLogger brewlogger("Test Stout Logger");
  brewlogger.LogWeight(34.5);
  brewlogger.LogWeight(346.5);
  return 0;
}
