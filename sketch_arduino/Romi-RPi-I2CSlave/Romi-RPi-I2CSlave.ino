#include <Servo.h>
#include <Romi32U4.h>
#include <PololuRPiSlave.h>


/* Pololu Romi Example Code included in here:

   This example program shows how to make the Romi 32U4 Control Board
   into a Raspberry Pi I2C slave.  The RPi and Romi 32U4 Control Board can
   exchange data bidirectionally, allowing each device to do what it
   does best: high-level programming can be handled in a language such
   as Python on the RPi, while the Romi 32U4 Control Board takes charge
   of motor control, analog inputs, and other low-level I/O.

   Make sure to set the Sketchbook location to
   ../RomiPi/sketch_arduino

*/

// NeoPixel Setup Materials
// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// released under the GPLv3 license to match the rest of the AdaFruit NeoPixel library

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      2

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

int delayval = 500; // delay for half a second

// end NeoPixel Setup


/* Custom data structure that we will use for interpreting the buffer.
   We recommend keeping this under 64 bytes total.  If you change the
   data format, make sure to update the corresponding code in
   a_star.py on the Raspberry Pi.

   If you change the struct, make sure to update the FIRMWARE VERSION

   The number written after each variable declaration is the byte offset
   from the start of the buffer. Refer to these when writing the
   Raspberry Pi-side driver.
*/
#define ROMI_FIRMWARE_VERSION (13)
struct Data
{
  uint8_t romi_fw_version = ROMI_FIRMWARE_VERSION; // 0
  uint8_t red, green, blue; // 1,2,3
  bool buttonB, buttonC; // 3,4,5

  int16_t left_vel_target_meter_per_sec, right_vel_target_meter_per_sec; // 6,7,8,9, 10,11,12,13
  //uint16_t analog[4]; // 14,15, 16,17, 18,19, 20,21

  bool resetEncoders; // 14
  bool fillerBool; // 15

  int16_t leftEncoder, rightEncoder; // 16,17, 18,19

  float pose_x, pose_y;     // 20,21,22,23, 24,25,26,27,
  float pose_quat_z, pose_quat_w;   // 28,29,30,31, 32,33,34,35,
  float pose_twist_linear_x, pose_twist_angle_z; //  43,37,38,39, 40,41,42,43,
  uint16_t batteryMillivolts;  // 44,45
  bool buttonA; // 46
};


// PololuRPiSlave<BufferType, pi_delay_us>
// NOTE: the 20 us delay here and the address of 20 below
//       are just a coincidence, not a typo.
PololuRPiSlave<struct Data, 20> slave;
PololuBuzzer buzzer;
Romi32U4Motors motors;
Romi32U4ButtonA buttonA;
Romi32U4ButtonB buttonB;
Romi32U4ButtonC buttonC;
Romi32U4Encoders encoders;

/* PREVIOUS TIME AND ENCODER VALUES */
unsigned long last_time_ms = 0;
int prev_left_count_ticks, prev_right_count_ticks;
float left_vel_meter_per_sec, right_vel_meter_per_sec;

/* CURRENT POSE */
float pose_x_m = 0;
float pose_y_m = 0;
float pose_th_rad = 0;
float pose_quat_z_unitless = 0;
float pose_quat_w_unitless = 0;
float pose_twist_linear_x_m_per_s = 0;
float pose_twist_angle_z_rad_per_s  = 0;

/* MOTOR CONTROL */
float left_vel_target_meter_per_sec  = 0.0;
float right_vel_target_meter_per_sec = 0.0;
int16_t left_motor, right_motor;

void setup()
{
  // Set up the slave at I2C address 20.
  slave.init(20);

  // Play startup sound.
  buzzer.play("v10>>g16>>>c16");

  pixels.begin(); // This initializes the NeoPixel library.
  for (int i = 0; i < 150; i++) {
    pixels.setPixelColor(0, pixels.Color(i, i, i));
    pixels.show();
    delay(10);
  }

  ledYellow(false);
  ledGreen(true);
  ledRed(false);
}

void loop()
{
  // Call updateBuffer() before using the buffer, to get the latest
  // data including recent master writes.
  slave.updateBuffer();

  // Write various values into the data structure.
  slave.buffer.buttonA = buttonA.isPressed();
  slave.buffer.buttonB = buttonB.isPressed();
  slave.buffer.buttonC = buttonC.isPressed();

  // Change this to readBatteryMillivoltsLV() for the LV model.
  slave.buffer.batteryMillivolts = readBatteryMillivolts();

  // READING the buffer is allowed before or after finalizeWrites().

  // update pixel colors
  for (int i=0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(slave.buffer.red,
                                         slave.buffer.green, 
                                         slave.buffer.blue));
  }
  pixels.show();

  // update encoders
  if (slave.buffer.resetEncoders)
  {
    // reset and update encoder buffer
    slave.buffer.resetEncoders = 0;
    slave.buffer.leftEncoder   = encoders.getCountsAndResetLeft();
    slave.buffer.rightEncoder  = encoders.getCountsAndResetRight();
  } else {
    // update encoder buffer without reset
    slave.buffer.leftEncoder = encoders.getCountsLeft();
    slave.buffer.rightEncoder = encoders.getCountsRight();
  }


  if ( everyNmillisec(10) ) {
    // ODOMETRY
    /*
      calculateOdom();
      slave.buffer.pose_x              = pose_x_m;
      slave.buffer.pose_y              = pose_y_m;
      slave.buffer.pose_quat_z         = pose_quat_z_unitless;
      slave.buffer.pose_quat_w         = pose_quat_w_unitless;
      slave.buffer.pose_twist_linear_x = pose_twist_linear_x_m_per_s;
      slave.buffer.pose_twist_angle_z  = pose_twist_angle_z_rad_per_s;
    */

    slave.buffer.left_vel_target_meter_per_sec   = left_wheel_velocity_target();
    slave.buffer.right_vel_target_meter_per_sec  = right_wheel_velocity_target();
    doPID();
  }


  /*
    if ( every100millisec() ) {
    debug_motors();
    }
  */

  // When you are done WRITING, call finalizeWrites() to make modified
  // data available to I2C master.
  slave.finalizeWrites();

  /*
    Serial.print("x: ");
    Serial.print(slave.buffer.pose_x);
    Serial.print(", y: ");
    Serial.print(slave.buffer.pose_y);
    Serial.print(", z: ");
    Serial.print(slave.buffer.pose_quat_z);
    Serial.print(", w: ");
    Serial.print(slave.buffer.pose_quat_w);
    Serial.print(", vel x_m: ");
    Serial.print(slave.buffer.pose_twist_linear_x);
    Serial.print(", vel z_ang: ");
    Serial.print(slave.buffer.pose_twist_angle_z);
    Serial.println(" ");
  */
}
