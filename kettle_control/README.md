Plans for Kettler controller

The main purpose of the kettle controller is to replace the grainfather controller.
The tasks it did were:
 - PID temperature control:
   Using thermo probe and heater output, control the temperature of the kettle
   as accurately and as responsively as possible.
   Possible future upgrade to use two thermo probe
 - Pump control - turn the pump on and off as needed

 It also had a lot of logic for doing different mashing schedules and such.  All
 that is going into the main controller now.

 Ideal end goal:
 There is a lot of I/O that is being done by the main controller right now.
 The winch control is pretty seperate, and can be handled by the main controller,
 but the following could be rolled into the kettle controller:
  - Weight reading - requires bit-banging a protocol pretty reliably.
  - Valve control - it would make sense that valve control is handled by the
    same controller as the pump...
  - Chiller Pump - mostly because it shares a relay block with the valves, but
    it is also the "chiller" component of the pid  temperature control.


API
----------------
Periodic updates from the controller:
 - Weight(s) + times
 - Current Temp
 - Target Temp
 - Heater power percent
 - pump status
 - Valve status


 Commands to Controller:
 SetTargetTemp(double temp);
 StopHeater();
 SetFlow(direction);
 StopFlow();
 CheckHardware() -> (int response);


