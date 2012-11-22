/*
    This file is part of the FT817 Arduino Library.
 
 The FT817 Arduino Library is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 The FT817 Arduino Library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with FT817 Arduino Library.  If not, see http://www.gnu.org/licenses/;.	  
 
 Author: Gerolf Ziegenhain, DG6FL
 
 */

#define LONG_MAX 1000000000
#define TRUE 0
#define FALSE 1
#define INIT_WAIT_TIME 1000
#include <SoftwareSerial.h>
#include <HardwareSerial.h>


/*************************************************************************************************/
/* Configure the display screen  */
#define LCD_NUM_COL 22//16
#define LCD_NUM_ROW 4//2

#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

uint8_t lcd_key;


/*************************************************************************************************/
/* Configure the FT 817 stuff */
#include <FT817.h> 
#define SMETER_LEN 4
#define FREQ_LEN 12 // length of frequency display
typedef struct
{
  // current status
  long freq, freq_old;
  char mode[4], mode_old[4];
  char smeter[SMETER_LEN], smeter_old[SMETER_LEN];
  byte smeterbyte, smeterbyte_old;
} 
t_status;
t_status rig; 

#define FT817_TX_PIN 18 // defined in header -- FIXME
#define FT817_RX_PIN 17 
#define FT817_SPEED 38400
FT817 ft817(&Serial1);


void initialize_ft817 ()
{
  //lcd.setCursor(0,1);
 // lcd.print("Init FT817");

Serial.print("Init 817");
  ft817.begin(FT817_SPEED);
  delay(INIT_WAIT_TIME);

  read_rig();  
}


/*************************************************************************************************/
// Bands configuration
#include "t_channels.h"
#include "t_bandplan.h"

int cur_ch;
#define CH_NAME_LEN 5 // NB: must be adjusted in bandplan header creation script, too!
char cur_ch_name[CH_NAME_LEN];
#define NO_CHANNEL -1
#define CHANNEL_FOUND 0
#define NO_CHANNEL_FOUND -1

// NB: indices as in list above!! 
const int watchdog_frequencies[] = {
  43932500, 2706500, 14521250}; // FIXME: can be configured
const int num_watchdog_frequencies = 3;//FIXME

/*************************************************************************************************/
#define M_NONE 0
#define M_WATCHDOG 1
#define M_CHANNELS 2
#define M_FREQUENCY 3
#define M_SCANNING 4
byte modus;


/*************************************************************************************************/

#include <Adafruit_GPS.h>

#define GPS_SPEED 9600

Adafruit_GPS GPS(&Serial2);

uint32_t timer;

#define PMTK_SET_NMEA_UPDATE_01HZ  "$PMTK220,10000*2F" // http://www.hhhh.org/wiml/proj/nmeaxor.html
// http://www.adafruit.com/datasheets/PMTK_A08.pdf
void initialize_gps ()
{

  lcd.setCursor(0,1);
  lcd.print("Init GPS");

  GPS.begin(GPS_SPEED);

  // initialize gps module
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_01HZ);
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  delay(INIT_WAIT_TIME);
  Serial2.println(PMTK_Q_RELEASE);
  // 1st signal
  timer = millis();

  read_gps(); 

}


/*************************************************************************************************/
// Initialize the screen
void initialize_screen ()
{
  lcd.begin(LCD_NUM_COL, LCD_NUM_ROW);     // start the library
  lcd.clear();
  lcd.print("FT 817 (DG6FL)"); // print a simple message
  //delay(INIT_WAIT_TIME);
}



/*************************************************************************************************/
void read_rig ()
{
  // save old state
  rig.freq_old = rig.freq;
  //sprintf(rig.smeter_old, "%s", rig.smeter);
  rig.smeterbyte_old = rig.smeterbyte;
  //sprintf(rig.mode_old, "%s", rig.mode);

  do // rig frequency may initially be 0
  {
    rig.freq = ft817.getFreqMode(rig.mode);
    rig.smeterbyte = ft817.getRxStatus(rig.smeter);
  } 
  while (rig.freq == 0); 
  /*
  Serial.print("rig: ");
  Serial.print(rig.freq);
  Serial.print(" mode ");
  Serial.print(rig.mode);
  Serial.print("   ");
   Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
  Serial.print("\n");*/
} 

/*************************************************************************************************/
#define CHANGED 1
#define UNCHANGED 0
int rig_state_changed ()
{
  if (rig.freq_old == rig.freq &&   
    rig.smeterbyte_old == rig.smeterbyte )
    // rig.smeter_old == rig.smeter &&  // FIXME: string comparison?
    // rig.mode_old = rig.mode)
  {
    return UNCHANGED;
  }
  return CHANGED;
}

/*************************************************************************************************/
void display_frequency_mode_smeter ()
{


  // Frequency
  // All of the stuff below only creates a good frequency output - looks chaotic :(
  long freq = rig.freq * 10; //in Hz
  int mhz = freq / 1000000;
  int khz = (freq % 1000000)/1000;
  int hz = freq % 1000;
  char ffreq[FREQ_LEN];
  sprintf (ffreq, "%03d.%03d.%03d",mhz,khz,hz);


  // Channel name
  get_cur_ch_name(rig.freq);


  // Formatted output
  char line1[LCD_NUM_COL+1];
  char line2[LCD_NUM_COL+1];
  char line3[LCD_NUM_COL+1];
  char line4[LCD_NUM_COL+1];

  sprintf(line1, "%s %s",ffreq,rig.mode);
  sprintf(line2, "%s",cur_ch_name);
  sprintf(line3, "%s",rig.smeter);
  sprintf(line4, "%02d:%02d %2.2f %2.2f",(int)(GPS.hour), (int)(GPS.minute), (float)(GPS.lat), (float)(GPS.lon));


  // LCD output

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
  lcd.setCursor(0,2);
  lcd.print(line3);
  lcd.setCursor(0,3);
  lcd.print(line4);
}


/*************************************************************************************************/
// return the channel number of the given frequency, -1 if no channel
int freq_to_channel (long freq)
{
  int i;
  for (i = 0; i < nchannels; i++)
  {
    if (freq == channels[i].freq) return i;
  }
  return NO_CHANNEL;
}

/*************************************************************************************************/
// set global variable cur_ch_name to band name or better if possible channel name
void get_cur_ch_name (long freq)
{
  int i;
  for (i = 0; i < nchannels; i++)
  {
    if (freq == channels[i].freq) 
    {
      sprintf (cur_ch_name, "%s", channels[i].name);
      return;
    }
  }

  sprintf (cur_ch_name, "");
  return;
}




/*************************************************************************************************/
void channels_mode ()
{
  if (lcd_key & BUTTON_RIGHT)  { 
    set_channel (cur_ch+1); 
  }
  if (lcd_key & BUTTON_LEFT)   { 
    set_channel (cur_ch-1); 
  }
  if (lcd_key & BUTTON_UP)     { 
    modus = M_SCANNING; 
  }
}


/*************************************************************************************************/
void freq_plus_minus_mode ()
{
  float delta_freq = 10; // 10 == 100 Hz

  // TBD
}

/*************************************************************************************************/
void set_channel (int ch)
{  
  // setup the internal current channel
  if (ch > nchannels - 1)
  {
    ch = 0;
  }
  if (ch < 0)
  {
    ch =  nchannels-1;
  }
  cur_ch = ch;

  // update the rig 
  do // it may happen, that the frequency is not set correctly during the 1st attempt.
  {
    ft817.setFreq(channels[cur_ch].freq);
    read_rig();
  } 
  while (rig.freq != channels[cur_ch].freq);

  ft817.setMode(channels[cur_ch].mode);
  if (channels[cur_ch].rpt != 0) 
  { 
    ft817.setRPTshift(channels[cur_ch].rpt); 
  }
}

/*************************************************************************************************/
int find_nearest_channel ()
{

  int i;
  int nearest_channel = 0;
  long delta_freq_min = LONG_MAX;
  for (i = 0; i < nchannels; i++)
  {

    long delta_freq = channels[i].freq - rig.freq;

    delay(10);
    if (delta_freq < 0) { 
      delta_freq = -delta_freq; 
    }
    if (delta_freq < delta_freq_min)
    {
      nearest_channel = i;
      delta_freq_min = delta_freq;
    }
  }


  return nearest_channel;
}

/*************************************************************************************************/
byte signal_detected ()
{
  switch (rig.smeterbyte)
  {
  case FT817_S0:   
    { 
      return FALSE; 
      break; 
    }
  case FT817_S1:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S2:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S3:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S4:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S5:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S6:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S7:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S8:   
    { 
      return TRUE; 
      break; 
    }
  case FT817_S10:  
    { 
      return TRUE; 
      break; 
    }
  case FT817_S20:  
    { 
      return TRUE; 
      break; 
    }
  case FT817_S30:  
    { 
      return TRUE; 
      break; 
    }
  case FT817_S40:  
    { 
      return TRUE; 
      break; 
    }
  case FT817_S50:  
    { 
      return TRUE; 
      break; 
    }
  case FT817_S60:  
    { 
      return TRUE; 
      break; 
    }
  default:         
    { 
      return FALSE; 
      break; 
    }
  }
}

/*************************************************************************************************/
#define SCAN_DELAY 30 //ms
// Scan function
int scan_function()
{
  int i;
  for (i = 0; i < nchannels; i++)
  {
    set_channel (i);
    delay(SCAN_DELAY);
    read_rig();
    display_frequency_mode_smeter();
    if (signal_detected() == TRUE)
    {
      modus = M_CHANNELS;
      return CHANNEL_FOUND;
    }
  }
  modus = M_CHANNELS;
  return NO_CHANNEL_FOUND;
}


/*************************************************************************************************/
#define SCAN_DELAY 30 //ms
int watchdog ()
{
  int i;
  for (i = 0; i < num_watchdog_frequencies; i++)
  {
    set_channel (freq_to_channel(watchdog_frequencies[i]));
    delay(SCAN_DELAY);
    read_rig();
    display_frequency_mode_smeter();
    // FIXME: other display for watchdog mode
    if (signal_detected() == TRUE)
    {
      modus = M_NONE;
      return CHANNEL_FOUND;
    }
  }
}


/*************************************************************************************************/
//#define TIMER 2000 //timer in ms
//int ncycles = 0;
void read_gps ()
{
 // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  //if (GPSECHO)
    //if (c) Serial.print(c);
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis()) timer = millis();
     
  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer
      display_frequency_mode_smeter ();

     Serial.print("rig: ");
  Serial.print(rig.freq);
  Serial.print(" mode ");
  Serial.print(rig.mode);
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality);
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
    }
  }
}


/*************************************************************************************************/
// Global Setup Routing 
void setup ()
{
  Serial.begin(9600);
  initialize_screen();

  initialize_gps();
  initialize_ft817();

  modus = M_CHANNELS;
  //cur_ch = find_nearest_channel();
  cur_ch = 0;
  //display_frequency_mode_smeter ();
}



/*************************************************************************************************/
// Main loop
void loop ()
{    
  read_gps();
  read_rig(); 
  //delay(50);display_frequency_mode_smeter ();
  //display_frequency_mode_smeter ();
return;
  if (rig_state_changed() == CHANGED)  { 
    display_frequency_mode_smeter (); 
  }

  //lcd_key = lcd.readButtons();
  switch (modus)
  {
  case M_WATCHDOG: 
    { 
      watchdog(); 
      break; 
    }
  case M_CHANNELS: 
    { 
      channels_mode(); 
      break; 
    }
  case M_FREQUENCY: 
    { 
      freq_plus_minus_mode (); 
      break; 
    }
  case M_SCANNING: 
    { 
      scan_function(); 
      break; 
    }
  }

  //if (lcd_key) { display_frequency_mode_smeter (); }
}


