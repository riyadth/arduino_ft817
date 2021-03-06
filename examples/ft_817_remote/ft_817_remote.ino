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
#include <Encoder.h>
Encoder Rotary(2, 3); // NB: connect to interrupt pins
long RotaryPosition;

void init_rotary ()
{
  lcd.setCursor(0,1);
  lcd.print("Init Rotary");
  RotaryPosition = 0;
}

void read_rotary ()
{
  long newRotary = Rotary.read();
  if (newRotary != RotaryPosition)
  {
    long diff = newRotary-RotaryPosition;
    browse_frequency (diff);
    RotaryPosition = newRotary;
  }
}

/*************************************************************************************************/
/* Configure the FT 817 stuff */
#include <FT817.h> 
#define SMETER_LEN 4
#define FREQ_LEN 12 // length of frequency display
#define MODE_LEN 5
#define CHANNEL_LEN 20
#define QTH_LEN 7
#define NO_CHANNEL -1
#define CHANNEL_FOUND 0
#define NO_CHANNEL_FOUND -1
typedef struct
{
  // current status
  long freq;
  char mode[MODE_LEN];
  char smeter[SMETER_LEN];
  byte smeterbyte;
} 
t_status;
t_status rig; 

#define FT817_SPEED 38400
FT817 ft817(&Serial1);

void initialize_ft817 ()
{
  lcd.setCursor(0,1);
  lcd.print("Init FT817");
  ft817.begin(FT817_SPEED);
  delay(INIT_WAIT_TIME);
  read_rig();  
}



/*************************************************************************************************/
// Bands configuration
#include "t_channels.h"
#include "t_bandplan.h"
t_channel curch;
char curchname[CHANNEL_LEN];
char curchqth[QTH_LEN];
int index_curchannel, index_curband;

// NB: indices as in list above!! 
const long watchdog_frequencies[] = {
  43340000, 43932500, 2706500, 14521250}; // FIXME: can be configured
const int num_watchdog_frequencies = 4;//FIXME

#define M_NONE 0
#define M_WATCHDOG 1
#define M_CHANNELS 2
#define M_FREQUENCY 3
#define M_SCANNING 4
byte modus;



/*************************************************************************************************/
// Position based stuff
#include <math.h>
typedef struct
{
  float lat;
  float lon;
  char qth[QTH_LEN];
  int dist; // distance to repeater (km)
} t_position;
t_position curpos;
float mz_lat, mz_lon;

void wgs_to_maidenhead (float lat, float lon, char *locator)
{
  char *m = locator;
  lon += 180.;
  lat += 90.;

  m[0] = 0x41+(int)(lon/20.);
  m[1] = 0x41+(int)(lat/10.);
  m[2] = 0x30+(int)((fmod(lon,20.))/2.);
  m[3] = 0x30+(int)((fmod(lat,10.))/1.);
  m[4] = 0x61+(int)((lon - ((int)(lon/2.)*2.)) / (5./60.));
  m[5] = 0x61+(int)((lat - ((int)(lat/1.)*1.)) / (2.5/60.));;
}

void maidenhead_to_wgs (float *lat, float *lon, char *locator)
{
  *lon -= 180.;
  *lat -= 90.;
  char *m = locator;

  *lon += (m[0]-0x41)*20.;
  *lat += (m[1]-0x41)*10.;
  *lon += (m[2]-0x30)*2.;
  *lat += (m[3]-0x30)*1.;
  *lat += (m[4]-0x61)*(5./60.);
  *lon += (m[5]-0x61)*(2.5/60.);
}

float calculate_distance_wgs84 (float lat1, float lon1, float lat2, float lon2)
{
  // NB: accuracy: (a) formula (b) 6letter locator: +-32km
  float r = 6378.; //km
  float fac = 3.1415/180.;
  float a1 = lat1*fac,
	b1 = lon1*fac,
	a2 = lat2*fac,
	b2 = lon2*fac;
  float dd = 
	  acos(cos(a1)*cos(b1)*cos(a2)*cos(b2) 
		+ cos(a1)*sin(b1)*cos(a2)*sin(b2) 
		+ sin(a1)*sin(a2)) * r;
  return dd;
}



/*************************************************************************************************/
#include <Adafruit_GPS.h>
#define PMTK_SET_NMEA_UPDATE_01HZ  "$PMTK220,10000*2F" 
#define GPS_SPEED 9600
Adafruit_GPS GPS(&Serial2);

uint32_t timer;

void initialize_gps ()
{
  lcd.setCursor(0,1);
  lcd.print("Init GPS");

  GPS.begin(GPS_SPEED);

  // initialize gps module
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  delay(INIT_WAIT_TIME);
  Serial2.println(PMTK_Q_RELEASE);
  // 1st signal
  timer = millis();

  read_gps(); 
  
  // default cooridnated MZ
  mz_lat = 50.03333;
  mz_lon = 8.28438;
}





/*************************************************************************************************/
// Initialize the screen
void initialize_screen ()
{
  lcd.begin(LCD_NUM_COL, LCD_NUM_ROW);     // start the library
  lcd.clear();
  lcd.print("FT 817 (DG6FL)"); // print a simple message
}



/*************************************************************************************************/
void read_rig ()
{
  do // rig frequency may initially be 0
  {
    rig.freq = ft817.getFreqMode(rig.mode);
    rig.smeterbyte = ft817.getRxStatus(rig.smeter);
  } 
  while (rig.freq == 0); 
} 


/*************************************************************************************************/

void display_frequency ()
{
  // Frequency
  // All of the stuff below only creates a good frequency output - looks chaotic :(
  long freq = rig.freq * 10; //in Hz
  int mhz = freq / 1000000;
  int khz = (freq % 1000000)/1000;
  int hz = freq % 1000;

  lcd.setCursor(0,0);
  if (mhz < 100) { lcd.print("0"); }
  if (mhz < 10) { lcd.print("0"); }
  lcd.print(mhz);
  lcd.print(".");  
  if (khz < 100) { lcd.print("0"); }
  if (khz < 10) { lcd.print("0"); }
  lcd.print(khz);
  lcd.print(".");    
  if (hz < 100) { lcd.print("0"); }
  if (hz < 10) { lcd.print("0"); }
  lcd.print(hz);
  lcd.print(" ");
  lcd.print(rig.mode);
}

void display_channel ()
{
  int i = index_curchannel;
  int j = index_curband;
  
  lcd.setCursor(0,1);
  if (i >= 0) { lcd.print(curch.name); for (i=strlen(curch.name);i<CHANNEL_LEN;i++){lcd.print(" ");}}
  else
  { 
    if (j >= 0) { lcd.print(bands[j].name); for (i=strlen(bands[j].name);i<CHANNEL_LEN;i++){lcd.print(" ");}}
    else { lcd.print("No bandplan        "); }
  }
}

void update_cur_ch_band ()
{
  index_curchannel = get_cur_ch_name(rig.freq);
  index_curband = get_cur_band_name(rig.freq);
}

void display_smeter ()
{
  lcd.setCursor(0,2); 
  lcd.print(rig.smeter);
}


void update_curpos ()
{
  if (GPS.fix) { curpos.lat = (float)(GPS.lat); curpos.lon = (float)(GPS.lon); }
  else { curpos.lat =mz_lat;curpos.lon= mz_lon; } //FIXME
  wgs_to_maidenhead(curpos.lat,curpos.lon,curpos.qth);
  char *qth2 = curch.qth; //"JO21da";// FIXME: repeater position
  float lat2,lon2;
  maidenhead_to_wgs (&lat2,&lon2,qth2);
  curpos.dist = (int)calculate_distance_wgs84 (curpos.lat,curpos.lon,lat2,lon2);
}


void display_time()
{  
  // time
  lcd.setCursor(0,3); 
  if (GPS.hour < 10) { lcd.print("0"); }
  lcd.print(GPS.hour);
  lcd.print(":");
  if (GPS.minute < 10) { lcd.print("0"); }
  lcd.print(GPS.minute);
  lcd.print(" ");
  
  // qth
  lcd.print(curpos.qth);
  lcd.print(" ");
  
  // distance to repeater
  if (curpos.dist < 1000) {
    if (curpos.dist < 100) { lcd.print("0"); }
    if (curpos.dist < 10) { lcd.print("0"); }
    lcd.print(curpos.dist);
    lcd.print("km");
  }
}


void display_frequency_mode_smeter ()
{
  lcd.clear();
  display_frequency();
  display_channel();
  display_smeter();
  display_time();
}


/*************************************************************************************************/
// return the channel number of the given frequency, -1 if no channel
int freq_to_channel (long freq)
{
  int i;
  for (i = 0; i < nchannels-1; i++)
  {
    //FIXME if (freq == channels[i].freq) return i;
  }
  return NO_CHANNEL;
}

/*************************************************************************************************/
int get_cur_ch_name (long freq) // FIXME: rename this function
{
  int i;
  long ff;
  for (i = 0; i < nchannels-1; i++)
  {
    ff = pgm_read_dword_far(&((channels+i)->freq));
    if (freq == ff) { 
      curch.freq = ff; //pgm_read_dword_far(&((channels+i)->freq));
      curch.shift = pgm_read_dword_far(&((channels+i)->shift));
      curch.mode = pgm_read_word_far(&((channels+i)->mode));
      strcpy_P(curchname, (char*)pgm_read_word( &((channels+i)->name)) );
      strcpy_P(curchqth, (char*)pgm_read_word( &((channels+i)->qth)) );
      curch.name=curchname;
      curch.qth=curchqth;
      return i;   
    }
  }
  curch.freq = -1;
  curch.shift = -1;
  curch.mode = NULL;
  curch.name = NULL;
  curch.qth = NULL;
  return -1;
}

int get_cur_band_name (long freq)
{
  int i;
  for (i = 0; i < nbands; i++)
  {
    if (bands[i].low <= freq && freq <= bands[i].high) { return i; }
  }
  return -1;
}


/*************************************************************************************************/
float delta_freq;
void browse_frequency (long delta)
{
  delta_freq = 0.25*3; // 10 == 100 Hz . 0.25 due to rotary
  long f = rig.freq + delta*delta_freq;
  do // it may happen, that the frequency is not set correctly during the 1st attempt.
  {
    ft817.setFreq(f);
    read_rig();
  } 
  while (rig.freq != f);
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
  index_curchannel = ch;

  // update the rig 
  long f = pgm_read_dword_far(&((channels+index_curchannel)->freq));
  byte mode = pgm_read_word_far(&((channels+index_curchannel)->mode));
  long shift = pgm_read_dword_far(&((channels+index_curchannel)->shift));

  do // it may happen, that the frequency is not set correctly during the 1st attempt.
  {
    ft817.setFreq(f);
    read_rig();
  } 
  while (rig.freq != f);

  ft817.setMode(mode);
  if (shift) //FIXMEchannels[cur_ch].freq < 0) // negative freq => repeater 
  {    
    ft817.setRPTshift(shift); 
  }
}

/*************************************************************************************************/
int find_nearest_channel ()
{

  int i;
  int nearest_channel = 0;
  long delta_freq_min = LONG_MAX;
  for (i = 0; i < nchannels-1; i++)
  {

    long delta_freq = 0;//FIXMEchannels[i].freq - rig.freq;

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
  for (i = 0; i < nchannels-1; i++)
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
  long oldfreq = rig.freq;
  lcd.setCursor(0,0);
  lcd.print ("Watchdog            ");
  lcd.setCursor(0,1);
  lcd.print ("                    ");
  lcd.setCursor(0,2);
  lcd.print ("                    ");
  for (i = 0; i < num_watchdog_frequencies; i++)
  {
    ft817.setFreq(watchdog_frequencies[i]);
    //set_channel (freq_to_channel(watchdog_frequencies[i]));
    delay(SCAN_DELAY);
    read_rig();

    // FIXME: other display for watchdog mode
    if (signal_detected() == TRUE)
    {
      lcd.setCursor(0,1);
      lcd.print ("Signal detected!");
      display_smeter();
      //modus = M_NONE;
      return CHANNEL_FOUND;
    }
  }
  ft817.setFreq(oldfreq);
  read_rig();
  display_frequency();
  display_channel();
  return 1;
}


/*************************************************************************************************/
#define TIMER 80000 //timer in ms
#define TIMER_GPS 2000
#define TIMER_SMETER 500
#define TIMER_FREQUENCY 4500
#define TIMER_WATCHDOG 40000

void read_gps ()
{
  char c = GPS.read();

  if (GPS.newNMEAreceived()) {
   
    if (!GPS.parse(GPS.lastNMEA())) 
      return; 
  }
  if (timer > millis()) timer = millis();
}

/*************************************************************************************************/
// Global Setup Routing 
void setup ()
{
  Serial.begin(9600);
  initialize_screen();

  initialize_gps();
  init_rotary();

  initialize_ft817();

  modus = M_CHANNELS;
  index_curchannel = find_nearest_channel();
  //cur_ch = 0;
  display_frequency_mode_smeter ();
}



void read_buttons ()
{
  lcd_key = lcd.readButtons();
  
  if (lcd_key & BUTTON_RIGHT)  { 
    set_channel (index_curchannel+1); 
  }
  if (lcd_key & BUTTON_LEFT)   { 
    set_channel (index_curchannel-1); 
  }
}


/*************************************************************************************************/
// Main loop
void loop ()
{    
  int update_display = 0;
  read_rotary();
  read_buttons();
  read_gps();
  read_rig();
  
  uint32_t curtimer = millis() - timer;
  if (curtimer > TIMER) {
    timer = millis(); // reset the timer
  }
  if (curtimer > TIMER_GPS)  {  update_cur_ch_band(); update_curpos();  display_time();  } //show_gps();}
  if (curtimer > TIMER_SMETER)  {    display_smeter();  }
  if (curtimer > TIMER_FREQUENCY)  {   update_cur_ch_band();  display_frequency(); display_channel(); }
  //if (curtimer > TIMER_WATCHDOG)  {  watchdog(); timer = millis(); }
}


