/*
  Program lnfill-u6 to handle LN filling and monitoring of Ge
  when RMS system is not available.

  Will put relevant data into shared memory where other programs
  can access it and decide actions such as turning off HV.

  Program will be based on LabJack U6 USB daq, dataport probe
  power strip with remote relay control.

*/
//#include <gtk/gtk.h>
//#include <cairo.h>

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
#include <sys/ipc.h>   /* for shared memory stuff */
#include <sys/shm.h>   /* for shared memory stuff */
#include <fcntl.h>   /* File control definitions */
#include <sys/types.h> /* for shared memory stuff */
#include <sys/mman.h>   /* for memory mapping stuff */


#define LNFILLDATAPATH "../data/lnfill.bin"            // user data for input to xia2disk
#define LNFILLDATASIZE sizeof(struct lnfill)

  key_t shmKey;
  long int shmid;

  time_t curtime = -1;

  struct dewar {
    int chanRTD;          // U6 ADC channel for RTD
    int chanOFLO;         // U6 ADC channel for overflo temperature
    int chanIbar;         // iBootbar channel
    int onoff;            // 0=off, 1=on
    int u1;               // mpod channel to shutdown
    int u2;               // mpod channel to shutdown
    int u3;               // mpod channel to shutdown
    int u4;               // mpod channel to shutdown
    char name[10];        // name of detector; ORNL's are A1, B2, ...
    double rtd;           // temperature of detector
    double limit;         // temperature limit of RTD (determines HV shutdown)
    double oflo;          // temperature of outlet
    double olimit;        // temperature limit of RTD (determines HV shutdown)
    double interval;      // time between fills (seconds); usually 6-8 hours
    double max;           // time to wait before giving up this fill
    double min;           // time that at least has to go by for complete fill
    double last;          // last fill duration
    double next;          // next time to start a fill
    char status[10];      // current status: COLD, WARM, FILL, ALARM, NO RTD
    char email[40];       // up to 20 email addresses to send warning
  };// ge[20];
  
  struct manifold {
    int chanRTD;          // U6 ADC channel for RTD
    int chanPRES;         // U6 ADC channel for pressure
    int chanIbar;         // iBootbar channel for tankvalve
    int chanMani;         // iBootbar channel for manifold
    double pressure;      // pressure in the manifold line
    double rtd;           // temperature of manifold
    double cooltime;      // time to cool down the manifold
    double timeout;       // time to give up cooling
    double limit;         // RTD temp when to open to detectors
    double olimit;        // manifold temp when to open to detectors
    char status[10];      // current status: COLD, WARM, FILL, ALARM, NO RTD
  }; // tank; 

  struct lnfill {
    long int shm;             // communication method between the shared memory programs 
    pid_t pid;                // communication method between the shared memory programs
    time_t time1;             // time to check rtd temperatures
    time_t secRunning;        // time lnfill has been running in seconds
    int command;              // main communication method between the shared memory programs 
    int com1;                // extra communication method between the shared memory programs 
    int com2;                // communication method between the shared memory programs (lnfill and hv) 
    char bitstatus[24];       // bit status revealing which outlets are on or off
    char comStatus[24];       // status of the command that program is doing
    int maxAddress;            // number of email addresses to send warnings
    char ibootbar_IP[30];     // ibootbar IP address
    char mpod_IP[30];         // mpod IP address
    struct manifold tank;     // merge the two struct to load shared memory segment
    struct dewar ge[20];
  } ln;

  struct lnfill *lnptr; 

//  char file_shm[]    = "/Users/c4g/src/Labjack/LNfill/include/lnfill.conf";        // used in shmSetup
//  char lnfill_conf[] = "/Users/c4g/src/Labjack/LNfill/include/lnfill.conf";        // used in readConf
//  char lnfill_mail[] = "/Users/c4g/src/Labjack/LNfill/include/lnfill-email.txt";   // used in readEmail
