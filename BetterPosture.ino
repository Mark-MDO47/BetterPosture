// We're including two Libraries, "Ultrasonic.h" and "FastLED.h"
// FastLED.h gives us the ability to addres the WS2812 LEDs
// "Ultrasonic.h" allows us to trigger and read the ultrasonic sensor
#include "Ultrasonic.h"
#include "FastLED.h"

// Mark Olson 2016-09
//
// adapted from TWIT-TV Know How https://twit.tv/shows/know-how/episodes/178
//     major kudos to Fr. Robert Ballecer and Bryan Burnett for their show
//             and for pointing me to the FastLED library
//     major kudos to Daniel Garcia and Mark Kriegsman for the FANTASTIC FastLED library and examples!!!
//
// My buddy Jim and I pretty much re-wrote the code for our own sensibilities
//
// Using a moving average of the utrasound ranges so crazy individual readings don't goof us
//
// Discovered it REALLY helps to use AREF when using an analog signal; maybe that is needed due to
//    using inexpensive clone Arduino nano
// Also using a software hysteresis. When the potentiometer is found to be in a certain interval
//    then we make it "sticky" by extending that interval a bit on both sides (POT_HYST_DELTA)

// We are using an 8-LED stick
#define NUM_LEDS 8

// We'll be using Digital Data Pin #3 to control the WS2812 LEDs
#define LED_DATA_PIN 3

// This tells the ultrasonic library that the trigger pin is on digital 13
// This also tells the ultrasonic library that the echo pin is on digital 12
#define ULTRA_TRIG_PIN 13
#define ULTRA_ECHO_PIN 12
// we use the range value from the Ultrasonic sensor to decide we are in one of
//    ULTRA_REGION_NUM display ranges (or OFF)
//    ranges are mapped to appropriate entry in led_ultra_range_mapping[] array
#define ULTRA_REGION_NUM 6

// we will use analog pin A0 to read the potentiometer
// the potentiometer adjusts the desired car position (fine grain)
#define ANLG_PIN 0

// These are the definitions for our software hysteresis
// the setup() routine initializes the interval_* arrays
// NOTE FIXME DEPENDS ON POT_RAW_NUM_INTVL BEING A POWER OF 2 AND EXACTLY DIVISIBLE INTO COUNTS
#define POT_RAW_NUM_INTVL 16 // size of an interval
#define POT_HYST_DELTA 4 // size extra size of hysteresis each side in counts
#define POT_RAW_NUM_COUNTS 1024 // so max val is 1023
int interval_low[POT_RAW_NUM_INTVL];
int interval_high[POT_RAW_NUM_INTVL];
int interval_hyst_low[POT_RAW_NUM_INTVL];
int interval_hyst_high[POT_RAW_NUM_INTVL];

// Creates an array with the length set by NUM_LEDS above
// This is the array the library will read to determine how each LED in the strand should be set
CRGB led_display[NUM_LEDS];
CRGB led_ultra_range_mapping[8 * NUM_LEDS] = { \
    CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,  \
    CRGB::Orange,  CRGB::Orange,  CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Orange, \
    CRGB::Orange,  CRGB::Orange,  CRGB::Orange,  CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Green,   CRGB::Orange, \
    CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Orange,  CRGB::Orange,  CRGB::Orange,  CRGB::Red,  \
    CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Orange,  CRGB::Orange,  CRGB::Red,  \
    CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Orange,  CRGB::Red,  \
    CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,     CRGB::Red,  \
    CRGB::Black,   CRGB::Black,   CRGB::Black,   CRGB::Black,   CRGB::Black,   CRGB::Black,   CRGB::Black,   CRGB::Black};

int blnk_cnt = 0;
int blnk_on = 0;
#define BLNK_CNT_MAX 5

// for now - DEBUG FIXME calc range from potentiometer
#define RANGE_MVNG_AVG_NUM 8
int range_mvng_avg_array[RANGE_MVNG_AVG_NUM] = { 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000 };
int range_mvng_avg_idx = 0;

// Now we're going to initialize an instance from the Ultrasonic Sensor library with our pin numbers
// The sensor itself needs four leads: ground, vcc, trigger, and echo
Ultrasonic ultrasonic(ULTRA_TRIG_PIN,ULTRA_ECHO_PIN);

// copy_leds does the copying of the CRGB values
// also takes care of blinking
void copy_leds(int idx, CRGB *from, CRGB *to)
{
  for (int i = 0; i < NUM_LEDS; i += 1)
  {
    to[i] = from[i + NUM_LEDS * idx];
  }

  // do we need to blink the edges
  blnk_cnt += 1;
  blnk_cnt %= BLNK_CNT_MAX;
  if (0 == blnk_cnt) blnk_on ^= 1;
  if ((idx > 2) && (1 == blnk_on))
  {
    to[0] = CRGB::Black;
    to[NUM_LEDS-1] = CRGB::Black;
  }
}

// setup()
//   initializes FastLED library for our config
//   initializes serial port
//   initializes hysteresis range arrays
void setup()
{
  FastLED.addLeds<NEOPIXEL,LED_DATA_PIN>(led_display, NUM_LEDS);
  Serial.begin(9600);

  Serial.println("setup");
  
  int raw_num_per_intvl = (POT_RAW_NUM_COUNTS / POT_RAW_NUM_INTVL);
  for (int i = 0; i < POT_RAW_NUM_INTVL; i += 1)
  {
    interval_low[i] = raw_num_per_intvl * i;
    interval_high[i] = interval_low[i] + raw_num_per_intvl - 1;
    interval_hyst_low[i] = max(0, interval_low[i] - POT_HYST_DELTA);
    interval_hyst_high[i] = min(POT_RAW_NUM_COUNTS - 1, interval_high[i] + POT_HYST_DELTA);
    Serial.print(i);
    Serial.print(", ");
    Serial.print(interval_low[i]);
    Serial.print(", ");
    Serial.print(interval_high[i]);
    Serial.print(", ");
    Serial.print(interval_hyst_low[i]);
    Serial.print(", ");
    Serial.println(interval_hyst_high[i]);
  }
  delay(1000);
}

// loop()
//    sorry; gets a little complex here ... ;^)
//
//    we read the potentiometer and calculate its interval using hysteresis
//       note: normally the potentiometer only moved during calibration
//    using this we calculate the range_regions[] to use for the ultrasonic ranging
//
//    we get the range from the instance of the Ultrasonic library and pass it
//       through a moving average (a low pass filter (LPF) with some zeros)
//
//    we calculate what region the LPF range is in with a check for "no ultrasound bounce" return
//    the index of that region is the index into led_ultra_range_mapping[] for display
//       so we call copy_leds() with that index
//    copy_leds() takes care of LED blinking using a count based on time
//
void loop() {
  
  static int idx = 0;
  static int range_regions_min[ULTRA_REGION_NUM] = { 42, 32, 27, 17, 12, 7 };
  static int range_regions[ULTRA_REGION_NUM] = { 42, 32, 27, 17, 12, 7 };
  static int range_avg;
  static int potIntvl = 0; // number calculated from potentiometer using hysteresis; previous at entry
  static int once = 0;

  if (0 == once)
  {
    once = 1;
    Serial.println("loop");
  }

  int aRead = analogRead(ANLG_PIN); // This checks the pot connected to Analog Pin - and gives us a value between 0-1024

  // calculate next potIntvl using hysteresis. If still in expanded region of past poTintvl, no change
  if ((aRead <= interval_hyst_low[potIntvl]) || (aRead >= interval_hyst_high[potIntvl]))
  {
    for (int i = 0; i < POT_RAW_NUM_INTVL; i += 1)
    {
      if ((aRead >= interval_low[i]) && (aRead <= interval_high[i]))
      {
        potIntvl = i;
        break;
      }
    }
  }
  for (int i = 0; i < ULTRA_REGION_NUM; i += 1)
  {
    range_regions[i] = range_regions_min[i] + potIntvl;
  }
  
  //Serial.print(aRead);
  //Serial.print(", ");
  //Serial.println(potVal);

  // This creates an integer variable called "range", then fills it with the range from the Ultrasonic sensor in centimeters
  int range=(ultrasonic.Ranging(CM));
  //Serial.println(range);

  range_mvng_avg_array[range_mvng_avg_idx] = range;
  range_mvng_avg_idx = (range_mvng_avg_idx + 1) % RANGE_MVNG_AVG_NUM;
  // brute force moving average
  range_avg = 0;
  for (int i = 0; i < RANGE_MVNG_AVG_NUM; i += 1)
  {
    //Serial.print(range_avg);
    //Serial.print(", ");
    range_avg += range_mvng_avg_array[i];
  }
  //Serial.println(range_avg);
  range_avg /= RANGE_MVNG_AVG_NUM;
  //Serial.println(range_avg);

  // compute regions for range measurement

  // see what region idx we are in
  idx = ULTRA_REGION_NUM;
  for (int i = 0; i < ULTRA_REGION_NUM; i += 1)
  {
    if (range_avg > range_regions[i])
    {
      idx = i;
      break;
    }
  }
  if (range_avg > 2200) idx = 7; // DEBUG FIXME what is correct bounce not returning value
  //Serial.println(idx);

  copy_leds(idx, led_ultra_range_mapping, led_display);
  FastLED.setBrightness(60);
  FastLED.show();
  
  delay(100);
}
