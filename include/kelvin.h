#include <unistd.h>  /* UNIX standard function definitions */
#include <time.h>    /* Time definitions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <ctype.h>
#include <math.h>    /* Math definitions */
#include <signal.h>  /* Signal interrupt definitions */
//#include <stddef.h>
//#include <limits.h>
//#include <float.h>
//#include <termios.h> /* POSIX terminal control definitions */
//#include <errno.h>   /* Error number definitions */
#include <sys/ipc.h>   /* for shared memory stuff */
#include <sys/shm.h>   /* for shared memory stuff */

#include <fcntl.h>   /* File control definitions */
#include <sys/types.h> /* for shared/mapped memory stuff */
#include <sys/mman.h> /* for memory mapping  */

#define KELVINDATAPATH "../data/kelvin.bin"            // user data for input to xia2disk
#define KELVINDATASIZE sizeof(struct thermometer)
#define KELVINCONF "../include/kelvin.conf"

key_t shmKey;
long int shmid;

long int time0 = 0;

int ljmax = 0;  // max number of labjacks

struct therm {
  int model;               // 1034 (metal tip @MTAS) or 1022 (plastic tip @Dubna)
  char name[10];           // name of thermometer
  int lj;                  // labjack model serial number 
  int ljnum;               // labjack number for referencing model-specific information
  HANDLE ljh;              // handle for addressing a particular labjack
  short int chan;          // channel on labjack - usually 0-3
  short int range;         // range of labjack ADC - 31 = normal range for either LV or HV, 32 = extended range
  short int onoff;         // 1 = on, 0 = off 
  short int unit;          // 0 = celcius; 1 = kelvin, 2=Fahrenheit
  unsigned short para;     // parameter for DAQ
  short int limit;         // temperature limits used for logic/action elsewhere with the same units as unit. def -500
  unsigned short data;     // data for daq (so integer only * 10...ie 20 C will be 200 in data (tenth degree)
  double degree;           // data read from LabJack
};

union tclk {
  time_t time1;                 // 32-bit computer clock
  unsigned short int data[2];   // two 16-bit computer clock
};

struct thermometer {
  long int shm;                // communication method between shared memory programs
  pid_t pid;                   // shared memory or memory mapped identifier
  int com1;                    // commands to pass between programs through shared memory (deg-u3-read -> deg-u3)
  int com2;                    // commands to pass between programs through shared memory (deg-u3-read -> deg-u3-log)
  int com3;                    // commands to pass between programs through shared memory (deg-u3-read -> deg-u3-daq)
  int maxchan;                 // number of thermometers
  int interval;                // time between temperature reads
  time_t time0;                // start time
  unsigned short int timeMSB;  // time data for daq...note that this isn't really needed given the pixie data is 32-bit
  unsigned short int timeLSB;  // time data for daq
  union tclk tim;              // union to break up int time into 2 short unsigned ints for daq
  struct therm temps[16];      // thermometer information  
};      // deg;

  struct thermometer *degptr; 

