// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brewmanager.h"
int main(void)
{
  BrewManager brewmanager("Test Weight Logger");
  brewmanager.WaitForInput(60 * 60 * 3);
  return 0;
}
