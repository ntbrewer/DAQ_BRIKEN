/*
  Program lnfill-u6 to handle LN filling and monitoring of Ge
  when RMS system is not available.

  Will put relevant data into shared memory where other programs
  can access it and decide actions such as turning off HV.

  Program will be based on LabJack U6 USB daq, dataport probe
  power strip with remote relay control.

*/

#include "lnfill.h"
#include "u6.h"
#include "labjackusb.h"

#define INTERVAL 1

void readConf();
void shmSetup();
int labjackSetup();
double readRTD(long int ii);
void resetU6(uint8 res);

void setTimer (); 
void alarm_wakeup ();
struct itimerval tout_val;

void handlerCommand(int sig, siginfo_t *siginfo, void *context);

HANDLE hU6;
u6CalibrationInfo caliInfo;

char *errormsg[] =
   {
     "no msg code",   //0
     "no msg code",   //1
     "no msg code",   //2
	"DATA_BUFFER_OVERFLOW",   //3	
	"ADC0_BUFFER_OVERFLOW",   //4
	"FUNCTION_INVALID",   //5	
	"SWDT_TIME_INVALID",   //6	
	"XBR_CONFIG_ERROR",   //7	
        "no msg code",   //8
        "no msg code",   //9
        "no msg code",   //10
        "no msg code",   //11
        "no msg code",   //12
        "no msg code",   //13
        "no msg code",   //14
        "no msg code",   //15
	"FLASH_WRITE_FAIL",   //	16
	"FLASH_ERASE_FAIL",   //	17
	"FLASH_JMP_FAIL",   //	18
	"FLASH_PSP_TIMEOUT",   //19	
	"FLASH_ABORT_RECEIVED",   //20	
	"FLASH_PAGE_MISMATCH",   //21	
	"FLASH_BLOCK_MISMATCH",   //22	
	"FLASH_PAGE_NOT_IN_CODE_AREA",   //23	
	"MEM_ILLEGAL_ADDRESS",   //24	
	"FLASH_LOCKED",   //	25
	"INVALID_BLOCK",   //	26
	"FLASH_ILLEGAL_PAGE",   //27	
	"FLASH_TOO_MANY_BYTES",   //28	
	"FLASH_INVALID_STRING_NUM",   //29	
        "no msg code",   //30
        "no msg code",   //31
        "SMBus_inQ_overflow",   //32
        "SMBus_outQ_underflow",   //33
        "SMBus_CRC_failed",   //34
        "no msg code",   //35
        "no msg code",   //36
        "no msg code",   //37
        "no msg code",   //38
        "no msg code",   //39
	"SHT1x_COMM_TIME_OUT",   //40	
	"SHT1x_NO_ACK",   //	41
	"SHT1x_CRC_FAILED",   //	42
	"SHT1X_TOO_MANY_W_BYTES",   //43	
	"SHT1X_TOO_MANY_R_BYTES",   //44	
	"SHT1X_INVALID_MODE",   //45	
	"SHT1X_INVALID_LINE",   //46
        "no msg code",   //47	
	"STREAM_IS_ACTIVE",   //	48
	"STREAM_TABLE_INVALID",   //	49
	"STREAM_CONFIG_INVALID",   //	50
	"STREAM_BAD_TRIGGER_SOURCE",   //51	
	"STREAM_NOT_RUNNING",   //	52
	"STREAM_INVALID_TRIGGER",   //	53
	"STREAM_ADC0_BUFFER_OVERFLOW",   //54	
	"STREAM_SCAN_OVERLAP",   //	55
	"STREAM_SAMPLE_NUM_INVALID",   //56	
	"STREAM_BIPOLAR_GAIN_INVALID",   //57	
	"STREAM_SCAN_RATE_INVALID",   //	58
	"STREAM_AUTORECOVER_ACTIVE",   //59	
	"STREAM_AUTORECOVER_REPORT",   //60
        "STREAM_SOFTPWM_ON",   //61	
        "no msg code",   //62	
	"STREAM_INVALID_RESOLUTION",   //63	
	"PCA_INVALID_MODE",   //	64
	"PCA_QUADRATURE_AB_ERROR",   //65	
	"PCA_QUAD_PULSE_SEQUENCE",   //66	
	"PCA_BAD_CLOCK_SOURCE",   //67
	"PCA_STREAM_ACTIVE",   //68	
	"PCA_PWMSTOP_MODULE_ERROR",   //69	
	"PCA_SEQUENCE_ERROR",   //70	
	"PCA_LINE_SEQUENCE_ERROR",   //71	
	"PCA_SHARING_ERROR",   //72	
        "no msg code",   //73
        "no msg code",   //74
        "no msg code",   //75
        "no msg code",   //76
        "no msg code",   //77
        "no msg code",   //78
        "no msg code",   //79
	"EXT_OSC_NOT_STABLE",   //80	
	"INVALID_POWER_SETTING",   //81	
	"PLL_NOT_LOCKED",   //82	
        "no msg code",   //83
        "no msg code",   //84
        "no msg code",   //85
        "no msg code",   //86
        "no msg code",   //87
        "no msg code",   //88
        "no msg code",   //89
        "no msg code",   //90
        "no msg code",   //91
        "no msg code",   //92
        "no msg code",   //93
        "no msg code",   //94
        "no msg code",   //95
	"INVALID_PIN",   //96	
	"PIN_CONFIGURED_FOR_ANALOG",   //97	
	"PIN_CONFIGURED_FOR_DIGITAL",   //98	
	"IOTYPE_SYNCH_ERROR",   //99	
	"INVALID_OFFSET",   //100
	"IOTYPE_NOT_VALID",   //101	
	"no msg code",   //102	
        "no msg code",   //103
        "no msg code",   //104
        "no msg code",   //105
        "no msg code",   //106
        "no msg code",   //107
        "no msg code",   //108
        "no msg code",   //109
        "no msg code",   //110
        "no msg code",   //111
	"no msg code",   //112	
	"no msg code",   //113	
	"no msg code",   //114	
   };

long int time0=0;

/***********************************************************/
int main(int argc, char **argv){
/*
  key_t shmKey;
  int shmid;

  time_t curtime = -1;
  char xtime[40]="\0";
  long int size=4194304; //   = 4 MB // 131072;  //65536;
*/
  long int ii=0;
  long int adc=0;
  pid_t pid;
  int xx=0;

  struct sigaction act;

/*
  Set up LabJack U6
*/
  labjackSetup();

/*
  Shared memory creation and attachment
*/

  shmSetup();

/*  
   Get the process number for interupts when the two programs need to interact
*/
  pid = getpid();
  lnptr->pid = pid;
  printf("pid = %li => %li =>%lf\n",pid,lnptr->pid);
/*  
   Set up the signal capture routine for when the reading
   program want something changed
*/

  memset (&act,'\0', sizeof(act));
  act.sa_sigaction = &handlerCommand;
  act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGALRM,&act,NULL) < 0){
    perror("sigaction");
    return 1;
  }


/*  
   Read setup file on disk and load into shared memory
*/
  printf("Read conf file...\n");
  readConf();
  printf(" ... Conf file read \n");
/*
  Go out and read RTDs and get everything needed loaded into shared memory
*/

  for (ii=0; ii<20; ii++){
    if (lnptr->ge[ii].onoff == 1){
      lnptr->ge[ii].rtd = readRTD(lnptr->ge[ii].chanRTD);
      lnptr->ge[ii].oflo = readRTD(lnptr->ge[ii].chanOFLO);
      printf("%li ->  %lf   %lf\n", ii,lnptr->ge[ii].rtd,lnptr->ge[ii].oflo );
    }
  }
  lnptr->tank.rtd = readRTD(lnptr->tank.chanRTD);
  lnptr->tank.pressure = readRTD(lnptr->tank.chanPRES);

  printf("TANK: %lf\n", lnptr->tank.rtd);
/*  
  Setup time of next fill based on current time and configure file
*/
    curtime = time(NULL);
    time0=curtime;
    for (ii=0; ii<20; ii++){
      if (lnptr->ge[ii].onoff == 1){
	lnptr->ge[ii].next = time0 + lnptr->ge[ii].interval;
      }
    }

/*  
  Setup monitoring loop to look for changes and requests   
*/
/* 
  kicker timer stuff
*/
  setTimer();
  /*
  tout_val.it_interval.tv_sec = 0;
  tout_val.it_interval.tv_usec = 0;

  tout_val.it_value.tv_sec = INTERVAL; // set timer for "INTERVAL (10) seconds 
  tout_val.it_value.tv_usec = 0;

  setitimer(ITIMER_REAL, &tout_val,0);   // start the timer
  */
  lnptr->command = 0;
  while(lnptr->command != -1) {
    /*
    curtime = time(NULL);
    for (ii=0;ii<20;ii++){
      if (curtime > lnptr->ge[ii].next){
	if (lnptr->ge[ii].onoff == 1) { 

	  printf("start a fill !\n");
	}
      }  
    }
    */

    
    switch (lnptr->command) {
    case 0:
      signal(SIGTERM,alarm_wakeup);   // set the Alarm signal capture 
      sleep (1);                      // pause for time
      curtime = time(NULL);
      printf ("curtime = %li \n", curtime);

      break;
    case 1:
      printf ("1 selected\n");
	break;
    case 2:
      printf ("2 selected\n");
	break;
    default:
      printf ("default selected\n");
      lnptr->command=-1;
	break;
    }
    
  }

/*
  Alarm processing
*/

/*  
   
*/

/*

*/

/*  
   
*/

/*
   Print out some of shared memory to show it has been accessed

  ln.ge[0].rtd = 80.2;

  for (ii=0; ii< 20; ii++){
    ln.ge[ii].rtd = ln.ge[0].rtd + (double) ii;
    lnptr->ge[ii].rtd = ln.ge[ii].rtd;
    printf ("rtd[%li] = %.2lf = %.2lf\n", ii, ln.ge[ii].rtd,lnptr->ge[ii].rtd);

    sleep (1);
  }
*/
  shmdt(lnptr);                     // detach from shared memory segment
  printf("detached from SHM\n");

  shmctl(shmid, IPC_RMID, NULL);    // remove the shared memory segment hopefully forever
  printf("removed from SHM\n");

  closeUSBConnection(hU6);
  printf("USB closed\n");


  return 0;
}

/***********************************************************/
void handlerCommand(int sig, siginfo_t *siginfo, void *context){

  printf ("I caught a signal!\n");
  //  lnptr->command=0;

  /*
  switch (lnptr->command) {
  case 0:
    //    signal(SIGALRM,alarm_wakeup);   // set the Alarm signal capture 
    sleep (5);                      // pause for time
    curtime = time(NULL);
    printf ("curtime = %li, time0 = %li\n",curtime,time0);

    break;
  case 1:
    printf ("1 selected\n");
    break;
  case 2:
    printf ("2 selected\n");
    break;
  default:
    printf ("default selected\n");
    lnptr->command=-1;
    break;
  }
  */
  return;
}

/***********************************************************/
void setTimer () {
/*
  Set timer to check status every second
*/

  tout_val.it_interval.tv_sec = 0;
  tout_val.it_interval.tv_usec = 0;

  tout_val.it_value.tv_sec = INTERVAL; // set timer for "INTERVAL (1) seconds at top of file
  tout_val.it_value.tv_usec = 0;

  setitimer(ITIMER_REAL, &tout_val,0);   // start the timer

  //  signal(SIGALRM,alarm_wakeup);   // set the Alarm signal capture 

  return;
}
/***********************************************************/
void alarm_wakeup () {
  //  time_t curtime;
  //  GtkWidget *widget;
/*
   Routine where timer ends up on every interval
*/

  tout_val.it_value.tv_sec = INTERVAL; // set timer for "INTERVAL (10) seconds 

  setitimer(ITIMER_REAL, &tout_val,0);   // this starts the timer which will issue the alarm signal....needed to restart

    //  signal(SIGALRM,alarm_wakeup);   // set the Alarm signal capture ... may not be needed 

  lnptr->ge[0].rtd = readRTD(lnptr->ge[0].chanRTD);
  lnptr->ge[0].oflo = readRTD(lnptr->ge[0].chanOFLO);
  printf("1,1 ->  %lf   %lf\n", lnptr->ge[0].rtd,lnptr->ge[0].oflo );


  return;
 }


/**************************************************************/

void shmSetup() {

  printf("Setting up shared memory...\n");///Users/c4g/src/LNfill/include/lnfill.conf
  shmKey = ftok("/Users/c4g/src/LNfill/include/lnfill.conf",'b');                                    // key unique identifier for shared memory, other programs use 'LN' tag
  //  shmKey = ftok("SHM_PATH",'b');                                    // key unique identifier for shared memory, other programs use 'LN' tag
  shmid = shmget(shmKey, sizeof (struct lnfill), 0666 | IPC_CREAT); // gets ID of shared memory, size, permissions, create if necessary
  lnptr = shmat (shmid, (void *)0, 0);                              // now link to area so it can be used; struct lnfill pointer char *lnptr
  if (lnptr == (struct lnfill *)(-1)){                              // check for errors
    perror("shmat");
    exit;
  }
  printf ("shm size = %li\n",sizeof (struct lnfill) );

  printf("... set shared memory...\n");

  return;
}

/**************************************************************/

void readConf() {
  FILE *input_file;
  char line[200]="\0";
  char lnfill_conf[200]="/Users/c4g/src/LNfill/include/lnfill.conf";   //see define statement in lnfill.h 
  long int ii=0, mm=0;
  int onoff=0, chanRTD=0, chanOFLO=0;
  char name[10]="\0";
  double interval=0.0, max=0.0, min=0.0, limit=0.0;

  struct dewar ge;

/*
   Read configuration file
*/  

  if ( ( input_file = fopen (lnfill_conf,"r+") ) == NULL)
    {
      printf ("*** File on disk (%s) could not be opened: \n",lnfill_conf);
      printf ("===> %s \n",lnfill_conf);
      exit (EXIT_FAILURE);
    }

  fgets(line,150,input_file);    // reads column headers
  fgets(line,150,input_file);    // reads ----

/*
 Should be positioned to read file
*/
  while (1) {                   // 1 = true
    fgets(line,150,input_file);
    if (feof(input_file)) {
      mm = fclose(input_file);
      break;
    }
/*
   A line from the file is read above and processed below
*/
    mm = sscanf (line,"%li %s %i %lf %lf %lf %i %i %lf", &ii, name, &onoff, &interval, &max, &min, &chanRTD, &chanOFLO, &limit);
//    printf("\n %li %s %i %lf %lf %lf %i %i %lf \n", ii, name, onoff, interval, max, min, chanRTD, chanOFLO, limit);
/*
   Now load the data into the shared memory structure
*/

    if (ii < 21) {
      strcpy(lnptr->ge[ii].name,name);
      lnptr->ge[ii-1].onoff = onoff;
      lnptr->ge[ii-1].interval = interval;
      lnptr->ge[ii-1].max = max;
      lnptr->ge[ii-1].min = min;
      lnptr->ge[ii-1].chanRTD = chanRTD;
      lnptr->ge[ii-1].chanOFLO = chanOFLO;
      lnptr->ge[ii-1].limit = limit;
    } else {
      lnptr->tank.chanRTD = chanRTD;
      lnptr->tank.chanPRES = chanOFLO;
      lnptr->tank.timeout = max;
    }
  }
   
  for (ii=0; ii<20; ii++){
    printf("%li %s   %i   %lf    %lf    %lf    %lf\n",ii,lnptr->ge[ii].name,lnptr->ge[ii].onoff,lnptr->ge[ii].interval,lnptr->ge[ii].max,lnptr->ge[ii].min,lnptr->ge[ii].limit);
  }
 
  return;
}

/**************************************************************/
double readRTD (long int adc) {
  long int error=0;
  double involts=0.0, deg=0.0;
  double ohms=0.0, degRTD=273.15, amps=0.004167; //I=V/R=4.92/1080 = 4.555 mA ; 200 - 50 uA
/*
  Read the ADC from the U6
*/

  error = eAIN(hU6, &caliInfo, adc, 0, &involts, LJ_rgBIP1V, 8, 0, 0, 0, 0);
  //  error = eAIN(hU3, &caliInfo, 0, &DAC1Enable, adc, 31, &involts, 0, 0, 0, 0, 0, 0);
  if (error != 0){
    printf("%li - %s\n", error, errormsg[error]);
    closeUSBConnection(hU6);
    return 0;
  }
/*
  Convert the voltage read to Kelvin using a two point linear calibration
  close enough for our purposes .. google pt100 resistance vs temperatur table
*/

    ohms =  involts / amps;
    degRTD = 2.453687 * ohms + 27.781;
    deg = degRTD;  // voltage value in mV

    //    printf("%lf= %lf\n",involts,deg);

  return (deg);
}


/**************************************************************/
int labjackSetup(){
  long int localID=-1;
  long int count=0;
  long int error=0;

/*
  Open first found U3 over USB
*/
  printf("opening usb .... ");

  while (count < 5) {
    if( (hU6 = openUSBConnection(localID)) == NULL){
      count++;
      printf("Opening failed; reseting and attempting %li of 5 more times \n",count);
      printf ("....U6 device reset \n");
      resetU6(0x01);                       // 0x00 = soft reset; 0x01 = reboot
      sleep (2);
      resetU6(0x01);                       // 0x00 = soft reset; 0x01 = reboot
      sleep (2);
      if (count > 5) return 0;
    } 
    else {
      count = 5;
    }
  }

  printf("opened usb\n");
/*
  Get calibration information from U6
*/
  printf("getting calib .... ");
  error = getCalibrationInfo(hU6, &caliInfo);
  if(error != 0){
    printf("\n%li - %s\n",error, errormsg[error]);
    closeUSBConnection(hU6);
    return 0;
  } 
  printf("got calib \n");

  return;
}


/***********************************************************/
void resetU6(uint8 res){
/*
  Resets U3
*/
  uint8 resetIn[4], resetOut[4];
  long int ii=0, error=0;

  for (ii=0; ii<4; ii++){
    resetIn[ii]=0;
    resetOut[ii]=0;
  }
  resetIn[1] = 0x99;
  resetIn[2] = res;
  resetIn[3] = 0x00;
  resetIn[0] = normalChecksum8(resetIn,4);

  if( (error = LJUSB_BulkWrite(hU6, U6_PIPE_EP1_OUT, resetIn, 4)) < 4){
    LJUSB_BulkRead(hU6, U6_PIPE_EP2_IN, resetOut, 4); 
    printf("U6 Reset error: %s\n", errormsg[(int)resetOut[3]]);
    closeUSBConnection(hU6);
    return;
  }
  printf ("....U6 device reset \n");
  return;
}
/***********************************************************/