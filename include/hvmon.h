#include <unistd.h>  /* UNIX standard function definitions */
#include <time.h>    /* Time definitions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <ctype.h>
#include <math.h>    /* Math definitions */
#include <signal.h>  /* Signal interrupt definitions */
//#include <stddef.h>
#include <limits.h>
//#include <float.h>
//#include <termios.h> /* POSIX terminal control definitions */
//#include <errno.h>   /* Error number definitions */
//#include <sys/ipc.h>   /* for shared memory stuff */
//#include <sys/shm.h>   /* for shared memory stuff */
#include <fcntl.h>   /* File control definitions */
#include <sys/types.h> /* for shared memory stuff */
#include <sys/mman.h>   /* for memory mapping stuff */
#include <sys/time.h>

#define HVMONDATAPATH "../data/hvmon.bin"            // user data for input to xia2disk
#define HVMONDATASIZE sizeof(struct hvmon)
#define MPOD_MIB "../include/WIENER-CRATE-MIB.txt"
#define CONFIGFILE   "../include/hvmon.conf"
#define INTERVAL 60  //wake up every 15 seconds 

struct hvchan {
  int type;       // 0 = MPOD, 1 = CAEN
  char ip[30];    // ip address used for MPOD and CAEN
  int caenH;      // caen handle
  int tOK;
  unsigned short int chan;
  unsigned short int slot;
  int onoff;
  unsigned short int reset;
  float iMeas;   // uA
  float vMeas;   // Volts
  float vSet;    // Volts
  float vRamp;   // Volts per seconds
  float iSet;    // uA
  float trip;    // seconds
  float vMax;    // Volts
  float v1Set;   // Volts
  float i1Set;   // uA
  float downRamp;  // Volts per second
  unsigned int intTrip;  // probably boolean but not sure
  unsigned int extTrip;  // not boolean since 16 observed in tests
  char name[15];          // name of channel  
};

struct hvmon {
  pid_t pid;
  int com0;     // commands between programs -> control
  int com1;     // commands between programs -> detector
  int com2;     // commands between programs -> function to change
  int com3;     // commands between programs -> int   value to change
  float xcom3;  // commands between programs -> float value to change
  int maxchan;
  int maxtchan;
  int interval;
  time_t time0;
  time_t time1;
  time_t secRunning;
  //unsigned int caenCrate[16];  // 16 = slots, bit pattern = occupied channels
  //unsigned int caenSlot[16];   // 16 = number of channels in each slot
  float mpodTemp[16];
  int mpodUnit[16];
  struct hvchan xx[1000];
}hv;

struct hvmon *hvptr;



