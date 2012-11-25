
/*
	 This file is part of xmlbandplan.

    Xmlbandplan is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Xmlbandplan is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Xmlbandplan.  If not, see http://www.gnu.org/licenses/;.	  
*/

/*
 * This file has been created by xmlbandplan.
 */

// Repeater channels
typedef struct 
{
  char *name; // channel name
  long freq;  // frequency (Hz/10) 
  byte mode;  // mode
  long shift;   // repeater shift 
  char qth[7];
} t_repeater;

long rpt70cm = 760000; // 7,6 MHz
long rpt2m   = 60000; // 600 kHz

const t_repeater repeaters[] = {
	{"DB0VA",43932500,FT817_MODE_FM, -760000,"JO40BC"},
{"DB0ESW",43905000,FT817_MODE_FM, -760000,"JO51AE"},
	
};
int nrepeaters = sizeof(repeaters)/sizeof(repeaters[0]);
						
