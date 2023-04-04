/*
This is an SIP project designed for Arduino Uno r3, 2.2"TFT LCD Display by Adafruit,
Featherwing Music Maker(Amplifier) by Adafruit, and the MAX30102 Heart Rate and Blood
Oxygen sensor by SparkFun.

The Libraries included are:
SparkFun Max3010x Pulse and Proximity
Adafruit VS1053 Library
Adafruit GFX Library
Adafruit ILI9341

Examples from each library include:
mock_ili9341 - Adafruit GFX Library/Adafruit ILI9341
feather_midi - Adafruit VS1053 Library
Example5_HeartRate - SparkFun Max3010x Pulse and Proximity

The Heart Rate Sensor Reads BPM which is Displayed on the LCD and Converted to a MIDI
Signal on the Featherwing amplifier which is connected to a .5"/5ohm speaker. 

Author: Melissa Francis-Marie Teichert

*/
// Adafruit VS1053 Library - Version: Latest Featherwing Music Maker
#include <Adafruit_VS1053.h>
#include <SD.h>

/* PINS CONNECTED TO ARDUINO UNO */
#define VS1053_RESET -1 // VS1053 RESET PIN. -1 = NOT USED.
#define VS1053_CS 12 // VS1053 CHIP SELECTION PIN
#define VS1053_DCS 5 // DATA PIN/VS1053 CHIP COMMANDS
#define CARDCS 3 // SD CARD SELECTION PIN
#define VS1053_DREQ 4 // INTERRUPT/DATA REQUEST PIN FOR VS1053 CHIP
/* VALUES OF REFERENCE PINS FOR SPI CONNECTION */
#define MIDI 2
#define MISO 6
#define MOSI 7
#define SCK 8

//INSTANT THE MUSIC PLAYER
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer ( VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS );

// Solder closed jumper on bottom!
// See http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf Pg 31
#define VS1053_BANK_DEFAULT 0x00
#define VS1053_BANK_DRUMS1 0x78
#define VS1053_BANK_DRUMS2 0x7F
#define VS1053_BANK_MELODY 0x79

// See http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf Pg 32 for more!
#define VS1053_GM1_OCARINA 80
#define MIDI_NOTE_ON  0x90
#define MIDI_NOTE_OFF 0x80
#define MIDI_CHAN_MSG 0xB0
#define MIDI_CHAN_BANK 0x00
#define MIDI_CHAN_VOLUME 0x07
#define MIDI_CHAN_PROGRAM 0xC0
#if defined(ESP8266) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
  #define VS1053_MIDI Serial
#else
  // anything else? use the hardware serial1 port
  #define VS1053_MIDI Serial1
#endif
//=======================================================
// Adafruit GFX Library - Version: Latest 
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

//=======================================================
//SparkFun MAX3010X pulse and proximity libraries
#include <spo2_algorithm.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>

MAX30105 particleSensor;

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
//=======================================================

void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing...");
  
  //LCD Display Initialize 
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.println("Initializing...");
  delay(100);
  
  // Initialize MAX30102 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    tft.println("MAX30102 was not found. Please check wiring/power. ");
    while (1);
  }

  tft.println("Place your index finger on the sensor with steady pressure.");
  
  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  
  //Featherwing MIDI setup
  // WE GENERATE A VERIFICATION TONE....
  //INITIALIZE THE MUSIC PLAYER
  if ( ! musicPlayer.begin() ) {  
     tft.println( "VS1053 NOT FOUND. Are the pins connected correctly?" );
     while( 1 ); 
  }
  musicPlayer.setVolume( 3,3 ); //VOLUME (THE HIGHEST WOULD BE 0,0)
  musicPlayer.sineTest( 0x44 , 2000 );
  delay(500);
  VS1053_MIDI.begin(31250); // MIDI uses a 'strange baud rate'
  midiSetChannelBank(0, VS1053_BANK_MELODY);
  midiSetChannelVolume(0, 127);
  midiSetInstrument(0, VS1053_GM1_OCARINA);
  
}

void loop()
{
  //Featherwing MIDI Loop
  for (uint8_t i=60; i<69; i++) {
    midiNoteOn(0, i, 127);
    delay(100);
    midiNoteOff(0, i, 127);
  }
  delay(1000);
  
  //TFT Display
  unsigned long start = micros();
  
  //Max30102 Sensor Reading IR
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  
    
    tft.println("IR= ");
    tft.println(irValue);
    delay(3000);
    tft.println("BPM= ");
    tft.println(beatsPerMinute);
    delay(3000);
  
  if (irValue < 50000)
    Serial.println(" No finger?");
    Serial.println();
    tft.println(" No finger?");
    tft.println();
    
   return micros() - start;
   
   //FeatherWing music maker
   // HERE WE IMPLEMENT BACKGROUND CONTROL LOGIC
  Serial.println( "." );
  //DETERMINE IF THE MUSICPLAYER IS STOPPED...
  if ( musicPlayer.stopped()) {  
    Serial.println( "I'M FINISHED PLAYING MUSIC" );
    // ETERNAL CYCLE THAT DOES NOTHING:
    while( 1 ) {  
      delay( 10 );  
    }
  }
  //DETERMINE IF THERE IS DATA ON THE SERIAL PORT SENT TO THE ARDUINO UNO.
  if ( Serial.available()) {  
    char c = Serial.read();
    
    // IF THE COMMAND IS THE LETTER 's' WE STOP THE EXECUTION
    if ( c == 's' ) {  
      musicPlayer.stopPlaying();
    }
    
    // IF THE COMMAND IS A 'p' WE PAUSE OR QUIT IT
    if ( c == 'p' ) {  
      if ( ! musicPlayer.paused()) {  
        Serial.println( "Paused" );
        musicPlayer.pausePlaying( true );
      } else {   
        Serial.println( "Resumed" );
        musicPlayer.pausePlaying( false );
      }
    }
  }
  delay( 100 );
    
}

//Featherwing MIDI Configuration
/// MAKES A LIST OF ALL FILES ON THE SD CARD
void printDirectory ( File dir, int numTabs ) {  
   while ( true ) { 
     
     File entry = dir.openNextFile();
     if ( ! entry ) {  
       // NO MORE FILES
       Serial.println( "** NO MORE FILES **" );
       break;
     }
     for ( uint8_t i=0; i < numTabs; i++ ) {  
       Serial.print( '\t' );
     }
     Serial.print( entry.name ( ));
     if ( entry.isDirectory()) {  
       Serial.println( "/" );
       printDirectory( entry, numTabs+1 );
     } else {  
       // files have sizes, directories do not
       Serial.print( "\t\t" );
       Serial.println( entry.size ( ) , DEC );
     }
     entry.close();
   }
}

void midiSetInstrument(uint8_t chan, uint8_t inst) {
  if (chan > 15) return;
  inst --; // page 32 has instruments starting with 1 not 0 :(
  if (inst > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_PROGRAM | chan);  
  delay(10);
  VS1053_MIDI.write(inst);
  delay(10);
}


void midiSetChannelVolume(uint8_t chan, uint8_t vol) {
  if (chan > 15) return;
  if (vol > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write(MIDI_CHAN_VOLUME);
  VS1053_MIDI.write(vol);
}

void midiSetChannelBank(uint8_t chan, uint8_t bank) {
  if (chan > 15) return;
  if (bank > 127) return;
  
  VS1053_MIDI.write(MIDI_CHAN_MSG | chan);
  VS1053_MIDI.write((uint8_t)MIDI_CHAN_BANK);
  VS1053_MIDI.write(bank);
}

//Try activating this using the MAX30102 sensor input signal
void midiNoteOn(uint8_t chan, uint8_t n, uint8_t vel) {
  
  
  //Max30102 Sensor Reading IR
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  
  
  
  
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_ON | chan);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
  VS1053_MIDI.write(irValue);
}

void midiNoteOff(uint8_t chan, uint8_t n, uint8_t vel) {
  if (chan > 15) return;
  if (n > 127) return;
  if (vel > 127) return;
  
  VS1053_MIDI.write(MIDI_NOTE_OFF | chan);
  VS1053_MIDI.write(n);
  VS1053_MIDI.write(vel);
}



