# Pico Mecanum Firmware (`pico_mecanum.ino`)

Arduino firmware for the **Freenove FNK0089 "Mecanum Wheel Car Kit for Raspberry
Pi Pico W"**. It drives the four mecanum motors and accepts single-letter
movement commands over the Pico's native USB-CDC link.

This is the microcontroller half of the **XBoxControllerDevice** project: the
Raspberry Pi reads an Xbox Wireless Controller (via evdev) and streams command
letters to this sketch over USB.

---

## Hardware

- **Board:** Raspberry Pi Pico W (on the Freenove FNK0089 mecanum chassis)
- **Link:** the Pico's own micro-USB plugged into the Raspberry Pi -> native
  USB-CDC. On the Pico (Arduino) side this is `Serial`; on the Pi it enumerates
  as `/dev/ttyACM0` (fall back to `/dev/ttyACM1`). 115200 baud, 8N1.

### Motor pin table

Each motor is one H-bridge with two inputs. The pair is driven complementarily:
PWM on the "forward" pin + LOW on the "reverse" pin spins the wheel forward;
swap them to reverse. The forward pin is **not** always the lower GPIO.

| Motor | Position    | forward pin | reverse pin |
|-------|-------------|-------------|-------------|
| M1 FL | Front-Left  | GP18        | GP19        |
| M2 BL | Back-Left   | GP21        | GP20        |
| M3 FR | Front-Right | GP7         | GP6         |
| M4 BR | Back-Right  | GP9         | GP8         |

---

## Serial wire protocol

Host (Pi) -> Pico: one ASCII command letter followed by `\n`, at 115200 8N1.

| Byte | Action                              |
|------|-------------------------------------|
| `F`  | forward                             |
| `B`  | backward                            |
| `L`  | strafe left (translate, no spin)    |
| `R`  | strafe right                        |
| `Q`  | rotate left / CCW (spin in place)   |
| `E`  | rotate right / CW (spin in place)   |
| `S`  | stop                                |

- The host sends a command on every state change **and** repeats the current
  command as a ~100 ms heartbeat.
- `\r` is ignored; the firmware is robust to partial reads.
- **Failsafe:** if no valid command arrives for 500 ms, all motors stop.
- Fixed drive speed is 60 (out of 0..100).

---

## Build & flash from the Raspberry Pi (Arduino IDE)

1. **Install the arduino-pico core (earlephilhower).**
   Open Arduino IDE -> *File* -> *Preferences* -> *Additional Boards Manager
   URLs* and add:

   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```

   Then *Tools* -> *Board* -> *Boards Manager…*, search **"pico"**, and install
   **"Raspberry Pi Pico/RP2040/RP2350"** by Earle F. Philhower, III.

2. **Select the board.**
   *Tools* -> *Board* -> *Raspberry Pi RP2040 Boards* -> **"Raspberry Pi Pico W"**.

3. **(Optional) RP2040_PWM library.**
   This sketch uses the core's built-in `analogWrite()` and needs **no extra
   library**. If you prefer RP2040_PWM, install it via *Tools* -> *Manage
   Libraries…* and search **"RP2040_PWM"** — but the shipped sketch does not
   require it.

4. **Open the sketch.**
   *File* -> *Open…* -> `firmware/pico_mecanum/pico_mecanum.ino`.

5. **Flash.**
   - First flash: hold **BOOTSEL** on the Pico while plugging its micro-USB into
     the Pi, then release. The Pico mounts as a mass-storage drive. Click
     **Upload** in the IDE.
   - Subsequent flashes: with the sketch already running USB-CDC, the IDE can
     usually reset and upload without BOOTSEL. If an upload fails, repeat the
     BOOTSEL step.

6. **Confirm enumeration.**
   After upload the Pico re-enumerates as a serial device. Verify on the Pi:

   ```bash
   ls -l /dev/ttyACM*
   ```

   You should see `/dev/ttyACM0` (or `/dev/ttyACM1`).

7. **Quick manual test.**

   ```bash
   stty -F /dev/ttyACM0 115200 raw -echo
   printf 'F\n' > /dev/ttyACM0   # forward
   printf 'S\n' > /dev/ttyACM0   # stop
   ```

   (If you send `F` and then stop sending, the 500 ms failsafe stops the car on
   its own.)
