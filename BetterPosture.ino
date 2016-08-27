// We're including two Libraries, "Ultrasonic.h" and "FastLED.h"
// FastLED.h gives us the ability to addres the WS2812 LEDs
// "Ultrasonic.h" allows us to trigger and read the ultrasonic sensor
#include "Ultrasonic.h"
#include "FastLED.h"

// How many LEDs will you have in your array?
#define NUM_LEDS 8

// Which Digital pin will be used to control the WS2812 lights?
// We'll be using Digital Data Pin #3
#define DATA_PIN 3

// we will use analog pin A0
#define ANLG_PIN 0

// NOTE FIXME DEPENDS ON INTVL BEING A POWER OF 2
#define POT_RAW_NUM_INTVL 16 // size of an interval
#define POT_HYST_DELTA 4 // size extra size of hysterisis each side in counts
#define POT_RAW_NUM_COUNTS 1024 // so max val is 1023
int interval_low[POT_RAW_NUM_INTVL];
int interval_high[POT_RAW_NUM_INTVL];
int interval_hyst_low[POT_RAW_NUM_INTVL];
int interval_hyst_high[POT_RAW_NUM_INTVL];

// Creates an array with the length set by NUM_LEDS above
// This is the array the library will read to determine how each LED in the strand should be set
CRGB led_display[NUM_LEDS];
CRGB led_mapping[8 * NUM_LEDS] = { \
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

// Now we're going to initialize the Ultrasonic Sensor
// The sensor itself needs four leads: ground, vcc, trigger, and echo
// This tells the ultrasonic library that the trigger pin is on digital 13
// This also tells the ultrasonic library that the echo pin is on digital 12
Ultrasonic ultrasonic(13,12);

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

// This tells the library that there is a strand of NEOPIXELS attached to the Arduino
// that they are connected to the DATA_PIN specified above
// that the library should look at the array called "leds"
// and that there are the number of LEDs in the strand as defined above in "NUM_LEDS" 
// It also opens the serial port so we can see the range values 
void setup()
{
  FastLED.addLeds<NEOPIXEL,DATA_PIN>(led_display, NUM_LEDS);
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


#define REGION_NUM 6
void loop() {
  
  static int idx = 0;
  static int range_regions_min[REGION_NUM] = { 42, 32, 27, 17, 12, 7 };
  static int range_regions[REGION_NUM] = { 42, 32, 27, 17, 12, 7 };
  static int range_avg;
  static int potIntvl = 0; // number calculated from potentiometer using hysterisis; previous at entry
  static int once = 0;

  if (0 == once)
  {
    once = 1;
    Serial.println("loop");
  }

  int aRead = analogRead(ANLG_PIN); // This checks the pot connected to Analog Pin - and gives us a value between 0-1024

  // calculate next potIntvl using hysterisis. If still in expanded region of past poTintvl, no change
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
  for (int i = 0; i < REGION_NUM; i += 1)
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
  idx = REGION_NUM;
  for (int i = 0; i < REGION_NUM; i += 1)
  {
    if (range_avg > range_regions[i])
    {
      idx = i;
      break;
    }
  }
  if (range_avg > 2200) idx = 7; // DEBUG FIXME what is correct bounce not returning value
  //Serial.println(idx);

  copy_leds(idx, led_mapping, led_display);
  FastLED.setBrightness(60);
  FastLED.show();
  
  delay(100);
}
