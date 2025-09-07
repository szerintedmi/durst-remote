// SPDX-License-Identifier: UNLICENSED or project default
// Self-contained Bluepad32 input helper.

#include <Arduino.h>
#include <Bluepad32.h>

#include "GamePad.h"

#define INITIAL_LED_R 255
#define INITIAL_LED_G 0
#define INITIAL_LED_B 0
#define INITIAL_PLAYER_LEDS 0x0f

namespace BtInput
{

  static ControllerPtr s_controllers[BP32_MAX_GAMEPADS];
  static volatile bool s_trianglePressed = false;
  static GamepadState s_state{};

  static void onConnectedController(ControllerPtr ctl)
  {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i)
    {
      if (s_controllers[i] == nullptr)
      {
        s_controllers[i] = ctl;
        ctl->setPlayerLEDs(INITIAL_PLAYER_LEDS);
        ctl->setColorLED(INITIAL_LED_R, INITIAL_LED_G, INITIAL_LED_B);
        break;
      }
    }
  }

  static void onDisconnectedController(ControllerPtr ctl)
  {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i)
    {
      if (s_controllers[i] == ctl)
      {
        s_controllers[i] = nullptr;
        break;
      }
    }
  }

  void dumpGamepad(ControllerPtr ctl)
  {
    Console.printf(
        "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: %4d, %4d, brake: %4d, throttle: %4d, "
        "misc: 0x%02x, gyro x:%6d y:%6d z:%6d, accel x:%6d y:%6d z:%6d\n",
        ctl->index(),       // Controller Index
        ctl->dpad(),        // D-pad
        ctl->buttons(),     // bitmask of pressed buttons
        ctl->axisX(),       // (-511 - 512) left X Axis
        ctl->axisY(),       // (-511 - 512) left Y axis
        ctl->axisRX(),      // (-511 - 512) right X axis
        ctl->axisRY(),      // (-511 - 512) right Y axis
        ctl->brake(),       // (0 - 1023): brake button
        ctl->throttle(),    // (0 - 1023): throttle (AKA gas) button
        ctl->miscButtons(), // bitmask of pressed "misc" buttons
        ctl->gyroX(),       // Gyro X
        ctl->gyroY(),       // Gyro Y
        ctl->gyroZ(),       // Gyro Z
        ctl->accelX(),      // Accelerometer X
        ctl->accelY(),      // Accelerometer Y
        ctl->accelZ()       // Accelerometer Z
    );

    // // less verbose dump, only buttons
    // Console.print(
    //     "Controller state dump: idx=");
    // Console.print(ctl->index());
    // Console.print(", buttons=0x");
    // Console.println(ctl->buttons());
  }

  void begin(bool startScanning)
  {
    // Basic setup: register callbacks. Older Arduino wrapper takes only 2 args.
    BP32.setup(&onConnectedController, &onDisconnectedController);

    // Allow new connections if supported. If not, scanning usually starts by default.
    // It's OK if this is a no-op on older versions.
    BP32.enableNewBluetoothConnections(startScanning);

    // Enable virtual devices so DS4/DS5 can spawn touchpad mouse without errors
    BP32.enableVirtualDevice(true);
    BP32.enableBLEService(false);
  }

  void update()
  {
    // Fetch controller updates.
    (void)BP32.update();

    GamepadState agg = {};

    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i)
    {
      auto ctl = s_controllers[i];
      if (!ctl)
        continue;
      if (!ctl->isConnected()) //|| !ctl->hasData())
        if (!ctl->isGamepad())
          continue;

      agg.y |= ctl->y(); // Triangle (△)
      agg.a |= ctl->a(); // Cross (×)
      agg.b |= ctl->b(); // Circle (○)
      agg.x |= ctl->x(); // Square (□)
      agg.r1 |= ctl->r1();
      agg.l1 |= ctl->l1();
      agg.r2 |= ctl->r2();
      agg.l2 |= ctl->l2();

      const uint8_t d = ctl->dpad();
// Use Bluepad32-compatible DPAD bit positions if not provided by headers
#ifndef DPAD_UP
#define DPAD_UP 0x01
#define DPAD_DOWN 0x02
#define DPAD_RIGHT 0x04
#define DPAD_LEFT 0x08
#endif
      agg.dpadUp |= (d & DPAD_UP);
      agg.dpadDown |= (d & DPAD_DOWN);
      agg.dpadLeft |= (d & DPAD_LEFT);
      agg.dpadRight |= (d & DPAD_RIGHT);
      // dumpGamepad(ctl);
    }
    s_state = agg;
  }

  const GamepadState &state() { return s_state; }

} // namespace BtInput
