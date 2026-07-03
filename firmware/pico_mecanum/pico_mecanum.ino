/*
 * pico_mecanum.ino
 * ----------------------------------------------------------------------------
 * Firmware for the Freenove FNK0089 "Mecanum Wheel Car Kit for Raspberry Pi
 * Pico W". Drives four DC motors through their on-board dual H-bridges and
 * accepts single-letter movement commands over native USB-CDC (the Pico's own
 * micro-USB, which shows up as /dev/ttyACM0 on the host Raspberry Pi).
 *
 * This firmware is the microcontroller half of the XBoxControllerDevice
 * project. The Pi-side library reads an Xbox Wireless Controller via evdev and
 * streams command letters to this sketch; this sketch turns those letters into
 * mecanum wheel motions.
 *
 * ---------------------------------------------------------------------------
 * TOOLCHAIN
 *   Arduino core : arduino-pico (earlephilhower)  -> board "Raspberry Pi Pico W"
 *   PWM          : the core's built-in analogWrite() (no extra library needed)
 *
 * ---------------------------------------------------------------------------
 * WIRING / PIN TABLE
 *   Each motor is one H-bridge with two inputs (IN1/IN2). The pair is driven
 *   complementarily: PWM on one input + LOW on the other = spin one way; swap
 *   them to reverse. The "forward" input differs per motor on this board, so
 *   the two GPIOs of every motor are listed as {forwardPin, reversePin}.
 *
 *     Motor            Position      forwardPin   reversePin
 *     ---------------  ------------  -----------  -----------
 *     M1  FL           Front-Left    GP18         GP19
 *     M2  BL           Back-Left     GP21         GP20
 *     M3  FR           Front-Right   GP7          GP6
 *     M4  BR           Back-Right    GP9          GP8
 *
 *   NOTE: the "forward/positive" pin is NOT simply the lower GPIO of each pair.
 *   For FL it is the lower pin (GP18), but for BL/FR/BR it is the HIGHER pin
 *   (GP21, GP7, GP9). setMotor() is always called with (forwardPin, reversePin)
 *   in that order so a positive speed always drives the wheel forward.
 *
 * ---------------------------------------------------------------------------
 * SERIAL WIRE PROTOCOL  (115200 baud, 8N1, native USB-CDC = 'Serial')
 *   Host (Pi) -> Pico: one ASCII command letter followed by '\n'.
 *
 *     'F'  forward
 *     'B'  backward
 *     'L'  strafe left        (translate left, no rotation)
 *     'R'  strafe right       (translate right, no rotation)
 *     'Q'  rotate left / CCW  (spin in place counter-clockwise)
 *     'E'  rotate right / CW   (spin in place clockwise)
 *     'S'  stop
 *
 *   The host sends a command on every state change AND repeats the current
 *   command as a ~100 ms heartbeat. Carriage returns ('\r') are ignored.
 *
 *   FAILSAFE: if no valid command arrives for FAILSAFE_MS (500 ms) all motors
 *   are stopped, so a dead/unplugged host can never leave the car running.
 * ---------------------------------------------------------------------------
 */

// ---------------------------------------------------------------------------
// Motor pin definitions  {forwardPin, reversePin}
// ---------------------------------------------------------------------------
static const uint8_t FL_FWD = 18, FL_REV = 19;  // M1 Front-Left
static const uint8_t BL_FWD = 21, BL_REV = 20;  // M2 Back-Left
static const uint8_t FR_FWD = 7,  FR_REV = 6;   // M3 Front-Right
static const uint8_t BR_FWD = 9,  BR_REV = 8;   // M4 Back-Right

// ---------------------------------------------------------------------------
// PWM / speed configuration
//
// We use the arduino-pico core's analogWrite(). We set the PWM resolution
// range to 100 so a speed magnitude of 0..100 maps directly to a duty cycle
// with no scaling math, and pick a ~20 kHz frequency (above the audible band).
// ---------------------------------------------------------------------------
static const int  PWM_RANGE   = 100;     // analogWrite duty range: 0..100
static const int  PWM_FREQ_HZ = 20000;   // 20 kHz, above audible range
static const int  DRIVE_SPEED = 60;      // fixed D-pad drive speed (0..100)

// ---------------------------------------------------------------------------
// Failsafe / command state
// ---------------------------------------------------------------------------
static const unsigned long FAILSAFE_MS = 500;
static unsigned long lastCmdMillis = 0;

// ---------------------------------------------------------------------------
// setMotor(forwardPin, reversePin, speed)
//   speed in -100..100. Positive -> PWM on forwardPin, 0 on reversePin.
//   Negative -> PWM on reversePin, 0 on forwardPin. Zero -> both LOW (coast).
// ---------------------------------------------------------------------------
void setMotor(uint8_t forwardPin, uint8_t reversePin, int speed) {
  if (speed > PWM_RANGE)  speed = PWM_RANGE;
  if (speed < -PWM_RANGE) speed = -PWM_RANGE;

  if (speed > 0) {
    analogWrite(forwardPin, speed);
    analogWrite(reversePin, 0);
  } else if (speed < 0) {
    analogWrite(forwardPin, 0);
    analogWrite(reversePin, -speed);
  } else {
    analogWrite(forwardPin, 0);
    analogWrite(reversePin, 0);
  }
}

// ---------------------------------------------------------------------------
// Set all four wheels at once. Each argument is a signed speed (-100..100).
//   Order: Front-Left, Front-Right, Back-Left, Back-Right.
// ---------------------------------------------------------------------------
void setWheels(int fl, int fr, int bl, int br) {
  setMotor(FL_FWD, FL_REV, fl);
  setMotor(FR_FWD, FR_REV, fr);
  setMotor(BL_FWD, BL_REV, bl);
  setMotor(BR_FWD, BR_REV, br);
}

// ---------------------------------------------------------------------------
// Discrete movement functions.
//
// Wheel sign matrix (multiplied by DRIVE_SPEED), verified for this chassis:
//   forward      FL+ FR+ BL+ BR+
//   backward     FL- FR- BL- BR-
//   strafe_left  FL- FR- BL+ BR+
//   strafe_right FL+ FR+ BL- BR-
//   rotate_left  FL- FR+ BL- BR+   (CCW spin in place)
//   rotate_right FL+ FR- BL+ BR-   (CW  spin in place)
// ---------------------------------------------------------------------------
void forward()      { setWheels( DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED); }
void backward()     { setWheels(-DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED); }
void strafeLeft()   { setWheels(-DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED,  DRIVE_SPEED); }
void strafeRight()  { setWheels( DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED, -DRIVE_SPEED); }
void rotateLeft()   { setWheels(-DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED); }
void rotateRight()  { setWheels( DRIVE_SPEED, -DRIVE_SPEED,  DRIVE_SPEED, -DRIVE_SPEED); }
void stopAll()      { setWheels(0, 0, 0, 0); }

// ---------------------------------------------------------------------------
// Dispatch a single command letter. Returns true if it was a valid command
// (so the caller can refresh the failsafe timer only on valid input).
// ---------------------------------------------------------------------------
bool dispatch(char c) {
  switch (c) {
    case 'F': forward();     return true;
    case 'B': backward();    return true;
    case 'L': strafeLeft();  return true;
    case 'R': strafeRight(); return true;
    case 'Q': rotateLeft();  return true;
    case 'E': rotateRight(); return true;
    case 'S': stopAll();     return true;
    default:  return false;   // unknown byte: ignore, do not reset failsafe
  }
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);  // native USB-CDC

  // Configure PWM: duty range 0..100 and ~20 kHz on all motor pins.
  analogWriteRange(PWM_RANGE);
  analogWriteFreq(PWM_FREQ_HZ);

  const uint8_t pins[] = {FL_FWD, FL_REV, BL_FWD, BL_REV,
                          FR_FWD, FR_REV, BR_FWD, BR_REV};
  for (uint8_t i = 0; i < sizeof(pins); i++) {
    pinMode(pins[i], OUTPUT);
  }

  stopAll();
  lastCmdMillis = millis();
}

// ---------------------------------------------------------------------------
// loop()
//   Reads bytes, buffering until a '\n' terminates a token. Only the first
//   character of each token is used as the command. Robust to partial reads
//   (bytes arrive across multiple loop iterations) and ignores '\r'.
// ---------------------------------------------------------------------------
void loop() {
  static char token = 0;        // first char of the current line
  static bool haveChar = false; // whether we've captured a command char yet
  static bool failsafed = false;// true while the failsafe has already stopped us

  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') {
      continue;                 // ignore carriage returns
    }

    if (c == '\n') {
      // End of token: dispatch if we captured a command character.
      if (haveChar) {
        if (dispatch(token)) {
          lastCmdMillis = millis();
          failsafed = false;    // valid command received: re-arm the failsafe
        }
      }
      token = 0;
      haveChar = false;
    } else if (!haveChar) {
      token = c;                // keep the first non-terminator as the command
      haveChar = true;
    }
    // Additional characters before '\n' are ignored (single-letter protocol).
  }

  // FAILSAFE: stop if the host has gone quiet. Fire once on the transition so
  // we don't re-issue PWM writes every iteration while the host stays silent.
  if (!failsafed && (millis() - lastCmdMillis > FAILSAFE_MS)) {
    stopAll();
    failsafed = true;
  }
}
