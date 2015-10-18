#include <Adafruit_GFX.h>

#include <SoftwareSerial.h>
#include <FastLED.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <Time.h>         //http://www.arduino.cc/playground/Code/Time  
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)

#include <glcdfont.c>

SoftwareSerial btSerial(8, 9);
CRGB leds[10 * 11 + 4];

const byte ES[] =      {0, 0, 2};
const byte IST[] =     {0, 3, 3};
const byte FUENF[] =   {0, 7, 4};
const byte ZEHN[] =    {1, 0, 4};
const byte ZWANZIG[] = {1, 4, 7};
const byte VIERTEL[] = {2, 4, 7};
const byte NACH[] =    {3, 2, 4};
const byte VOR[] =     {3, 6, 3};
const byte HALB[] =    {4, 0, 4};
const byte UHR[] =     {9, 8, 3};

const byte HOUR[][3] = {
  //EIN       EINS       ZWEI       DREI       VIER       FUENF
  {5, 2, 3}, {5, 2, 4}, {5, 0, 4}, {6, 1, 4}, {7, 7, 4}, {6, 7, 4},
  //SECHS     SIEBEN     ACHT       NEUN       ZEHN       ELF        ZWÖLF
  {9, 1, 5}, {5, 5, 6}, {8, 1, 4}, {7, 3, 4}, {8, 5, 4}, {7, 0, 3}, {4, 5, 5}
};

/**
 * Contains all added words since  the last call of generateWords().
 */
const byte *new_words[6];
byte new_words_length = 0;

/**
 * Contains all removed words since the last call of generateWords().
 */
const byte *old_words[6];
byte old_words_length = 0;

/*
 * Contains all unchanged words since the last call of generateWords()
 */
const byte *const_words[6];
byte const_words_length = 0;

/**
 * Fore- and background color for the letters.
 */
CRGB foreground = CRGB(0, 255, 0);
CRGB background = CRGB(10, 50, 5); //CRGB(0, 255, 0) CRGB(10, 50, 5)

/**
 * Stores the effect number.
 */
byte effect = 2;

/**
 * Should "Es ist" be showed?
 */
bool showEsIst = false;

void setup() {
  Serial.begin(9600);

  //setup real time clock
  setSyncProvider(RTC.get);
  if(timeStatus() != timeSet) 
      Serial.println("Unable to sync with the RTC");
  else
      Serial.println("RTC has set the system time");

  //setup FastLED
  delay( 1000 ); // power-up safety delay
  FastLED.addLeds<NEOPIXEL, 7>(leds, 10 * 11 + 4);
  FastLED.setBrightness(255);

  for (int i = 0; i < 10 * 11; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  delay(100);

  btSerial.begin(9600);

  /*showChar(0, 2, 'A', CRGB(255, 255, 255));
  showChar(6, 2, 'O', CRGB(255, 255, 255));
  
  FastLED.show();
  delay(1500); */
  
  Serial.println("initialized");
}

void loop()
{
  generateWords();

  handleBluetooth();

  //255, 120, 0
  //CRGB(255, 245, 220), CRGB(30, 30, 20)
  //CRGB(255, 120, 30), CRGB(10, 10, 7)
  switch (effect) {
    case 0: showSimple(foreground, background); break;
    case 1: showFade(foreground, background); break;
    case 2: showTypewriter(foreground, background); break;
    case 3: showMatrix(foreground, background); break;
  }
  
  FastLED.show();
  delay(100);
}

/**
 * Generates the words to show using the actual time.
 */
void generateWords() {
  clearWords();

  if (showEsIst) {
    addWord(ES);
    addWord(IST);
  }
  
  byte shour = hourFormat12();
  switch(minute() / 5) {
    case 0:  if (shour == 1) shour = 0;                addWord(UHR); break;
    case 1:  addWord(FUENF);   addWord(NACH);                         break;
    case 2:  addWord(ZEHN);    addWord(NACH);                         break;
    case 3:  addWord(VIERTEL); addWord(NACH);                         break;
    case 4:  addWord(ZWANZIG); addWord(NACH);                         break;
    case 5:  addWord(FUENF);   addWord(VOR);  addWord(HALB); shour++; break;
    case 6:                                   addWord(HALB); shour++; break;
    case 7:  addWord(FUENF);   addWord(NACH); addWord(HALB); shour++; break;
    case 8:  addWord(ZWANZIG); addWord(VOR);                 shour++; break;
    case 9:  addWord(VIERTEL); addWord(VOR);                 shour++; break;
    case 10: addWord(ZEHN);    addWord(VOR);                 shour++; break;
    case 11: addWord(FUENF);   addWord(VOR);                 shour++; break;
  }
  
  if (shour == 13) shour = 1;
  addWord(HOUR[shour]);
}

/**
 * Shows all the new and constant words.
 */
void showSimple(CRGB on, CRGB off) {
  fillLeds(off);
  showAllWords(on, new_words, new_words_length);
  showAllWords(on, const_words, const_words_length);
}

/**
 * Fades new words in and old words out.
 */
void showFade(CRGB on, CRGB off) {
  if (new_words_length > 0) {
    for (int e = 1; e <= 256/8; e++) {
      int i = e * 8 - 1;
      fillLeds(off);
      showAllWords(on, const_words, const_words_length);
      showAllWords(off.lerp8(on, i), new_words, new_words_length);
      showAllWords(on.lerp8(off, i), old_words, old_words_length);
      FastLED.show();
      delay(50);
    }
  }
  
  showSimple(on, off);
}

/**
 * Shows the "matrix effect" in background color and the time in foreground color.
 */
byte matrix_worms[11] = {-5, -10, -3, -13, -1, 0, -1, -5, -6, -11, -4};
void showMatrix(CRGB on, CRGB off) {
  _fadeall();

  for (byte i = 0; i < 11; i++) {
    if (matrix_worms[i] < 10) {
      setLeds(matrix_worms[i], i, off, 1, false);
    }
    else if (matrix_worms[i] == 10) {
        matrix_worms[i] = -random8(14);
    }
    matrix_worms[i]++;
  }

  showAllWords(on, new_words, new_words_length);
  showAllWords(on, const_words, const_words_length);
}
void _fadeall() { for(int i = 0; i < 10 * 11; i++) { leds[i].nscale8(180); } } //180

/**
 * Removes old words and inserts new words in a typewriter style.
 */
void showTypewriter(CRGB on, CRGB off) {
  if (new_words_length > 0) {
    byte max_old_word = 0;
    byte max_new_word = 0;
  
    for (byte i = 0; i < old_words_length; i++)
      max_old_word = max(max_old_word, old_words[i][2]);
      
    for (byte i = 0; i < new_words_length; i++)
      max_new_word = max(max_new_word, new_words[i][2]);
  
    for (byte i = 0; i <= max_old_word; i++) {
      fillLeds(off);
      showAllWords(on, old_words, old_words_length, 200, i);
      showAllWords(on, const_words, const_words_length);
      FastLED.show();
      delay(150);
    }
    
    fillLeds(off);
    showAllWords(on, const_words, const_words_length);
    for (byte i = 0; i <= max_new_word; i++) {
      showAllWords(on, new_words, new_words_length, i, 0);
      FastLED.show();
      delay(150);
    }
  }

  showSimple(on, off);
}

/**
 * Parses and executes the bluetooh commands.
 */
void handleBluetooth() {
  while (btSerial.available() >= 4) {
    byte type = btSerial.read();
    switch (type) {
      case 'F': { //foreground color
        byte red = btSerial.read();
        byte green = btSerial.read();
        byte blue = btSerial.read();
        foreground = CRGB(red, green, blue);
        break;
      }
      case 'B': { //background color
        byte red = btSerial.read();
        byte green = btSerial.read();
        byte blue = btSerial.read();
        background = CRGB(red, green, blue);
        break;
      }
      case 'E':
        effect = btSerial.read();
        showEsIst = (btSerial.read() == 1);
        btSerial.read(); btSerial.read();
        break;
      default: {
        Serial.print("Unknown command ");
        Serial.println(type);
        break;
      }
    }
  }
}

/**
 * Adds a word to the new words array. When it is was previously in the new words it is added to the constant words.
 */
void addWord(const byte part[]) {
  bool in_old_words = 0;
  for (byte i = 0; i < old_words_length; i++) {  
    if (part == old_words[i]) {
      in_old_words = 1;
      
      const_words[const_words_length] = part;
      const_words_length++;

      //remove in old_words
      old_words_length--;
      for (byte e = i; e < old_words_length; e++) {
        old_words[e] = old_words[e + 1];
      }
    }
  }

  if (!in_old_words) {
    new_words[new_words_length] = part;
    new_words_length++;
  }
}

/**
 * Put all new and constant words to the old words.
 */
void clearWords() {
  for (byte i = 0; i < new_words_length; i++) {
    old_words[i] = new_words[i];
  }
  old_words_length = new_words_length;

  for (byte i = 0; i < const_words_length; i++) {
    old_words[old_words_length] = const_words[i];
    old_words_length++;
  }
  
  new_words_length = 0;
  const_words_length = 0;
}

/**
 * Set all Leds to a single color.
 */
void fillLeds(CRGB off) {
  setLeds(0, 0, off, 11 * 10, false);
}

/**
 * Shows a array of words in a specific color.
 */
void showAllWords(CRGB color, const byte *wds[], byte wds_length) {
  showAllWords(color, wds, wds_length, 200, 0);
}

/**
 * Shows a array of words in a specific color.
 * @param maxlen determines the max length of every word.
 * @param cut cuts the x last characters of every word
 */
void showAllWords(CRGB color, const byte *wds[], byte wds_length, byte maxlen, byte cut) {
  for (byte i = 0; i < wds_length; i++) {
    setLeds(wds[i][0], wds[i][1], color, min(max(wds[i][2] - cut, 0), maxlen), false);
  }
}

/**
 * Sets a number of leds starting at x, y to the a specific color.
 * When add is true the color is added to the existing color at the positions.
 */
void setLeds(int x, int y, CRGB color, int len, bool add) {
  int start_led = !(x % 2) ? x * 11 + y : x * 11 + (11 - y) - 1;
  int dir = !(x % 2) ? 1 : -1;

  if (add) {
    for (int i = 0; i < len; i++) {
      leds[start_led + i * dir] += color;
    }
  }
  else {
    for (int i = 0; i < len; i++) {
      leds[start_led + i * dir] = color;
    }
  }
}

/**
 * Shows a 5x7 char at a specific position.
 */
void showChar(byte x, byte y, unsigned char c, CRGB color) {
  for (int8_t i=0; i<6; i++ ) {
    uint8_t line;
    if (i == 5) 
      line = 0x0;
    else 
      line = pgm_read_byte(font+(c*5)+i);
    for (int8_t j = 0; j<8; j++) {
      if (line & 0x1) {
          setLeds(y+j, x+i, color, 1, false);
      }
      line >>= 1;
    }
  }
}
