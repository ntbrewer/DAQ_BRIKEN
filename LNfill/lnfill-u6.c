/*
  Program lnfill-u6 to handle LN filling and monitoring of Ge
  when RMS system is not available.

  To be compiled with:                                           
     gcc -lm -llabjackusb u6.o -o lnfill-u6 lnfill-u6.c

  Will put relevant data into shared memory where other programs
  can access it and decide actions such as turning off HV.

  Program will be based on LabJack U3 USB daq, dataport probe
  power strip with remote relay control.
*/

#include "../include/lnfill.h"
#include "../include/u6.h"
#include "../include/labjackusb.h"
#include "../include/u3-err-msg.h"

#define IBOOTBAR_MIB "../include/ibootbar_v1.50.258.mib"
#define MPOD_MIB     "../include/WIENER-CRATE-MIB.txt"
#define CONFIGFILE   "../include/lnfill.conf"

char lnfill_conf[200]="include/lnfill.conf";   //see define statement in lnfill.h 

// mtas1vme (name pf mtas1 on vme network) - 00.01.2E.3B.2E.9B mac address
//#define IBOOTBAR_IP "192.168.0.254"      //  Default from iBootBar: mask 255.255.255.0 
//#define IBOOTBAR_IP "192.168.1.9"        // ORNL address: mask 255.255.255.0   Subnet 192.168.13.0; name:powerbar0 (no phy.ornl.gov)
//#define IBOOTBAR_IP "192.168.13.33"        // ORNL address: mask 255.255.255.0   Subnet 192.168.13.0; name:powerbar0 (no phy.ornl.gov)
//#define MPOD_IP "192.168.13.231"
//#define MPOD_IP "192.168.13.239"                     // used at ANL 2015
//
#define EMAIL_HZ 900                                    // number of seconds between emails when warning
#define INTERVAL 60

void readConf();                  // processes configuration file
void shmSetup();                  // sets up the shared memory
int mmapSetup();                 // sets up the memory map
int labjackSetup();               // sets up the LabJack U6
double readRTD(long int ii);      // reads RTDs for temperature
void resetU6(uint8 res);          // resets the U6

void setTimer();                  // sets the timer for signals
void alarm_wakeup();
struct itimerval tout_val;        // needed the timer for signals
void handlerCommand(int sig, siginfo_t *siginfo, void *context);  // main handler for signal notification and decisions
void signalBlock(int pp);
HANDLE hU6;                       // LabJack stuff
u6CalibrationInfo caliInfo;       // LabJack stuff

void updateRTD();                                    // read RTDs and store values into shared memory
void snmp(int setget, char *cmd, char *cmdResult);   // snmp get/set commands
void snmpShutdown(int index, char *cmdResult);       // snmp shutdown of MPOD HV        //need to edit the shutdown commands!
int readOnOff(char *cmdResult);                      // snmp get/set results (on/off)
int readInt(char *cmdResult);                        // snmp get/set results (integer)
float readFloat(char *cmdResult);                    // snmp get/set results (floats)
unsigned int readBits(char *cmdResult);              // snmp get/set results (bits)
 
void fillGe ();
void fillAllDelete ();
void openTank();
void closeTank();
void openMani();
void closeMani();
void checkMani();
void openDet(int ii);
void closeDet(int ii);
void checkDet(int ii);
void closeAllValves ();
void outletStatus();
void sendEmail(char subject[]);
void readEmail();
void updateStatus (int mm, int nn);

long int time0=0, time1=0, nread;

sigset_t mask;
sigset_t orig_mask;
struct sigaction act;

char         path [PATH_MAX+1];                  // PATH_MAX defined in a system libgen.h file
char     mpod_mib [PATH_MAX+100]="\0";
char ibootbar_mib [PATH_MAX+100]="\0";

/***********************************************************/
int main(int argc, char **argv){
/*
  key_t shmKey;
  int shmid;

  time_t curtime = -1;
  char xtime[40]="\0";
  long int size=4194304; //   = 4 MB // 131072;  //65536;
*/
  long int ii=0, p0=0, p1=1, count=0, etime=0;
  int mapLNfill;
  pid_t pid;

/*
  Set up LabJack U6  ***PUT NOTHING BEFORE THIS AND SHARED MEMORY
*/
//  path = getcwd(path,PATH_MAX+1); 
  printf("Working directory: %s\n",getcwd(path,PATH_MAX+1));
  strcpy(mpod_mib,path);                  // copy path to mpod mib file
  strcat(mpod_mib,"/");                   // add back the ending directory slash
  strcpy(ibootbar_mib,mpod_mib);          // copy path to ibootbar mib file
  strcat(mpod_mib,MPOD_MIB);              // tack on the mib file location
  strcat(ibootbar_mib,IBOOTBAR_MIB);      // tack on the mib file location
  printf("    mpod_mib file: %s\n",mpod_mib);
  printf("ibootbar_mib file: %s\n",ibootbar_mib);
  
/*
  Set up LabJack U6  ***PUT NOTHING BEFORE THIS AND SHARED MEMORY
*/
  labjackSetup();
/*
  Shared memory creation and attachment
  the segment number is stored in lnptr->pid
*/
//  shmSetup();
  mapLNfill = mmapSetup();
  if (mapLNfill == -1) return 0;
  /*  
   Set up the signal capture routine for when the reading
   program wants something changed
*/
  pid = getpid();    // this gets process number of this program
  lnptr->pid= pid;   // this is the pid number for the SIGALRM signals

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
  printf("Read conf and email file...\n");
  readConf();
  readEmail();
  printf(" ... Conf file read \n");
  //  if (lnptr->ge[19].u4 == -1) return(0);
/*
  Go out and read RTDs and get everything needed loaded into shared memory
*/

  updateRTD();
/*  
  Setup time of next fill based on current time and configure file
*/
  curtime = time(NULL);
  time0 = curtime;               // record starting time of program
  time1 = time0 + INTERVAL;      // set up next read of RTD...usually every minute

  for (ii=0; ii<20; ii++){
    if (lnptr->ge[ii].onoff == 1){
      lnptr->ge[ii].next = time0 + lnptr->ge[ii].interval;
    }
    else {
      lnptr->ge[ii].next = 0;
    }
  }

/*  
  Setup monitoring loop to look for changes/minute and requests   
*/
  nread = INTERVAL;
  lnptr->command = 0;

  while(lnptr->command != -1) {
    if (strcmp(lnptr->comStatus,"FILL ALL INTERRUPTED") == 0){
      if (++count > 10) strcpy(lnptr->comStatus,"NORMAL");
    }
    else {
      strcpy(lnptr->comStatus,"NORMAL");
      count=0;
    }

    updateRTD();                            // read and update the RTD information including lnptr->secRunning 
/*
   Check if an RTD is too high
*/
    for (ii=0;ii<20;ii++){                               // run thru each detector possibility
      if (lnptr->ge[ii].onoff == 1) {                    // run thru each detector that is ON
	if (lnptr->ge[ii].rtd > lnptr->ge[ii].limit){    // check rtd is within limit          !@#$%
	  if (curtime > etime + EMAIL_HZ && (strcmp(lnptr->ge[ii].status,"ALARM") == 0 ||strcmp(lnptr->ge[ii].status,"NO RTD") == 0)) { 
	    sendEmail("LN Temp/RTD Alarm");              // sending email on HIGH TEMP or LONG FILL TIME
	    etime = curtime;
	  }
	}
	if (curtime > lnptr->ge[ii].next){               // check if its time to fill a detector
	  printf("start a fill ...set command to 8 = Fill All! \n");
	  lnptr->command = 8;
	} 
      }
    }
/*
   AFTER THIS LOOP CHECK FOR COMMANDS COMING IN FROM CONTROL PROGRAM
*/
    if (lnptr->command > 20 &&   lnptr->command <= 40){
      closeDet(lnptr->command-21);                      // way to close individual valves
      lnptr->command=0;
    }
    else if (lnptr->command > 40 &&  lnptr->command <= 60) {
      openDet(lnptr->command-41);                       // way to open individual valves
      lnptr->command=0;
    }
    else {
      switch (lnptr->command) {
      case 0:
/*
      //      signal(SIGTERM,alarm_wakeup);   // set the Alarm signal capture 
      sleep (1);                      // pause for time
      curtime = time(NULL);
      printf ("curtime = %li %li \n", curtime, (time1-curtime));
      if (curtime >= time1) {
	time1 = time1+INTERVAL;
	updateRTD();
      }
*/
	break;
      case 1:
	printf ("1 selected\n");
	updateRTD();
	lnptr->command=0;
	break;
      case 2:
	printf ("2 selected\n");
	lnptr->command=0;
	break;
      case 3:
	printf ("3 selected\n");
	lnptr->command=0;
	break;

      case 7:
	printf ("7 selected\n");   // fill 1 detector
	signalBlock(p1);
	lnptr->command=8;  // set to 8 in order to get thru fill routine
	fillGe();
	signalBlock(p0);
	lnptr->command=0;
	break;
      case 8:
	printf ("8 selected\n");    // fill All
	signalBlock(p1);
	fillGe();
	signalBlock(p0);
	lnptr->command=0;
	break;
      case 9:
	printf ("9 selected\n");     // initial cooldown
	signalBlock(p1);
	fillGe();
	signalBlock(p0);
	lnptr->command=0;
	break;

      case 10:
	printf ("10 selected\n");
	closeAllValves();             // close all valves including tank but open manifold
	lnptr->command=0;
	break;
      case 11:
	printf ("11 selected\n");
	closeTank();                  // open tank
	lnptr->command=0;
	break;
      case 12:
	printf ("12 selected\n");
	openTank();                  // close Tank
	lnptr->command=0;
	break;
      case 13:
	printf ("13 selected\n");
	closeMani();                  // close Manifold
	lnptr->command=0;
	break;
      case 14:
	printf ("14 selected\n");
	openMani();                  // open Manifold
	lnptr->command=0;
	break;
	// case 15 and case 16 sets the command to 21-40 and 41-60
      case 17:
      if (lnptr->command == 8 || lnptr->command == 7 || lnptr->command == 18) break;  // don't force an update
	signalBlock(p1);
	printf ("17 selected\n");     // hardware status
	outletStatus ();
	lnptr->command=0;
	signalBlock(p0);

	break;
      case 18:
	printf ("18 selected - does this reset alarms?\n");     // reset alarms....but does it ?
	lnptr->command=0;
	break;

      case 61:
	printf ("Sending emails\n");     // emails
	sendEmail("Test LN emails");
	lnptr->command=0;
	break;

      default:
	printf ("default selected\n");
	lnptr->command=0;
	break;
      }
    }  

    sleep (nread);
  }


/*
   Release the shared memory and close the U6
*/
  if (munmap(lnptr, sizeof (struct lnfill*)) == -1) {
    perror("Error un-mmapping the file");
/* Decide here whether to close(fd) and exit() or not. Depends... */
  }
  close(mapLNfill);

/*
  shmdt(lnptr);                     // detach from shared memory segment
  printf("detached from SHM\n");

  shmctl(shmid, IPC_RMID, NULL);    // remove the shared memory segment hopefully forever
  printf("removed from SHM\n");
*/
  closeUSBConnection(hU6);
  printf("USB closed\n");


  return 0;
}

/***********************************************************/
void signalBlock(int pp){
/*
    Block (p=1) and unblock (p=0) signal SIGALRM
*/

  sigemptyset(&mask);
  sigaddset (&mask,SIGALRM);

  if (pp == 1) {                        
       printf("Block signals attempt\n");
    //    sigemptyset(&mask);
    //   sigaddset (&mask,SIGALRM);
    
    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
      perror ("sigprocmask");
      return;
    }  
  }
  else {    
       printf("release block signals ....\n");
    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
      perror ("sigprocmask");
      return;
    }
  }
  return;

}
/***********************************************************/
void updateRTD(){
  long int ii=0;
  char cmdResult[140];
/*
  read only active RTDs and tank RTD and Pressure
  should only be 4 status conditions
 - OK
 - OK-FILL
 - NO RTD
 - ALARM
*/
  curtime = time(NULL);
  lnptr->time1 = curtime;             // record time check rtd's for data acquisition
  lnptr->secRunning = curtime - time0; 

  
  for (ii=0; ii<20; ii++){
    if (lnptr->ge[ii].onoff == 1){
      printf ("Det: %s RTD : ", lnptr->ge[ii].name);
      lnptr->ge[ii].rtd = readRTD(lnptr->ge[ii].chanRTD);
      printf ("Det: %s MAN : ", lnptr->ge[ii].name);
      lnptr->ge[ii].oflo = readRTD(lnptr->ge[ii].chanOFLO);
      //      printf("Active det: %li ->  %lf   %lf\n", ii,lnptr->ge[ii].rtd,lnptr->ge[ii].oflo );

      if (lnptr->ge[ii].rtd >= lnptr->ge[ii].limit) {
	strcpy(lnptr->ge[ii].status,"ALARM");                                    // over limit issue HV shutdown command to MPOD
	if (lnptr->com2 == 1){
	  if (lnptr->ge[ii].u1 >= 0) snmpShutdown(lnptr->ge[ii].u1,cmdResult);         //!@#$% 
	  if (lnptr->ge[ii].u2 >= 0) snmpShutdown(lnptr->ge[ii].u2,cmdResult);         //!@#$% issue HV shutdown command
	  if (lnptr->ge[ii].u3 >= 0) snmpShutdown(lnptr->ge[ii].u3,cmdResult);         //!@#$% issue HV shutdown command
	  if (lnptr->ge[ii].u4 >= 0) snmpShutdown(lnptr->ge[ii].u4,cmdResult);         //!@#$% issue HV shutdown command
	  printf ("Shutting down HV\n");
	  sendEmail("Shutting down HV");
/*
	  if (ii == 0) {   // shut down A1
	    snmpShutdown(0,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(1,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(2,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(3,cmdResult);         //!@#$% issue HV shutdown commands
	  }
	  if (ii == 5) {    // shut down F6
	    snmpShutdown(4,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(5,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(6,cmdResult);         //!@#$% issue HV shutdown commands
	    snmpShutdown(7,cmdResult);         //!@#$% issue HV shutdown commands
	  }
*/
	}
      }
      else if (lnptr->ge[ii].rtd <= 70 || lnptr->ge[ii].rtd >= 500) strcpy(lnptr->ge[ii].status,"NO RTD");     // broken RTD
      else if (strcmp(lnptr->ge[ii].status,"LONG") == 0 || strcmp(lnptr->ge[ii].status,"ALARM") == 0) break;                          // don't write over LONG fill or ALARM
      else {
	strcpy(lnptr->ge[ii].status,"OK");                                                                     // temp is OK
	if (lnptr->command == 7 || lnptr->command == 8) strcpy(lnptr->ge[ii].status,"FILL");                   // FILLING detector
      }
    }
  }
  printf ("Tank Manifold : ");
  lnptr->tank.rtd = readRTD(lnptr->tank.chanRTD);
  printf ("Tank Pressure : ");
  lnptr->tank.pressure = readRTD(lnptr->tank.chanPRES);

  //  printf("TANK: %lf\n", lnptr->tank.rtd);
  return;
}

/***********************************************************/
void handlerCommand(int sig, siginfo_t *siginfo, void *context){
/*
  Handlers are delicate and don't have all protections the rest
  of the user code has, so get in and out quickly...RLV recommendation!
*/

  printf ("I caught signal number %i ! \n",lnptr->command);

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

  printf ("In set timer!\n");
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
  printf("Alarm_wakeup: 1,1 ->  %lf   %lf\n", lnptr->ge[0].rtd,lnptr->ge[0].oflo );

  return;
 }
/**************************************************************/
int mmapSetup() {
  //#define FILEPATH "data/mmapped.bin"
  //#define FILESIZE sizeof(struct thermometer)
  int fd=0, result=0, ii=0;
  /* Open a file for writing.
    *  - Creating the file if it doesn't exist.
    *  - Truncating it to 0 size if it already exists. (not really needed)
    *
    * Note: "O_WRONLY" mode is not sufficient when mmaping.
    */
  //   fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0644);
   fd = open(LNFILLDATAPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0644);
   if (fd == -1) {
	perror("Error opening kelvin file for writing");
	exit(EXIT_FAILURE);
   }

  /* Open a file for writing.
    *  - Creating the file if it doesn't exist.
    *  - Truncating it to 0 size if it already exists. (not really needed)
    *
    * Note: "O_WRONLY" mode is not sufficient when mmaping.
    
   fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
   if (fd == -1) {
	perror("Error opening file for writing");
	exit(EXIT_FAILURE);
   }
*/
 /* Stretch the file size to the size of the (mmapped) array of ints
    */
   //   for (ii=0; ii<sizeof (struct thermometer); ii++){
   for (ii=0; ii<LNFILLDATASIZE; ii++){
     result = write(fd, "D", 1);
     if (result != 1) {
       close(fd);
       perror("Error writing last byte of the file");
       exit(EXIT_FAILURE);
     }
   };
   /*
   result = lseek(fd, FILESIZE-1, SEEK_SET);
   if (result == -1) {
	close(fd);
	perror("Error calling lseek() to 'stretch' the file");
	exit(EXIT_FAILURE);
   }
   */
   /* Something needs to be written at the end of the file to
    * have the file actually have the new size.
    * Just writing an empty string at the current file position will do.
    *
    * Note:
    *  - The current position in the file is at the end of the stretched 
    *    file due to the call to lseek().
    *  - An empty string is actually a single '\0' character, so a zero-byte
    *    will be written at the last byte of the file.
    */
   /*
   result = write(fd, "", 1);
   if (result != 1) {
	close(fd);
	perror("Error writing last byte of the file");
	exit(EXIT_FAILURE);
   }
   */

   /* Now the file is ready to be mmapped.
    */
   lnptr = (struct lnfill*) mmap(0, LNFILLDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (lnptr == MAP_FAILED) {
	close(fd);
	perror("Error mmapping the lnfill file");
	exit(EXIT_FAILURE);
   }
   /* Don't forget to free the mmapped memory
    */
   /*
  //   if (munmap(degptr, FILESIZE) == -1) {
  if (munmap(degptr, sizeof (struct thermometer*)) == -1) {
    perror("Error un-mmapping the file");
    close(fd);
    fd = -1;
  }
   */
   return (fd);
}


/**************************************************************/
/*
void shmSetup() {

  printf("Setting up shared memory...\n");///Users/c4g/src/LNfill/include/lnfill.conf
  //  shmKey = ftok("/Users/c4g/src/LNfill/include/lnfill.conf",'b');       // key unique identifier for shared memory, other programs use 'LN' tag
  shmKey = ftok("include/lnfill.conf",'b');       // key unique identifier for shared memory, other programs use 'LN' tag
  //  shmKey = ftok("SHM_PATH",'b');                                    // key unique identifier for shared memory, other programs use 'LN' tag
  shmid = shmget(shmKey, sizeof (struct lnfill), 0666 | IPC_CREAT);     // gets ID of shared memory, size, permissions, create if necessary
  lnptr = shmat (shmid, (void *)0, 0);                                  // now link to area so it can be used; struct lnfill pointer char *lnptr
  if (lnptr == (struct lnfill *)(-1)){                                  // check for errors
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  lnptr->shm = shmid;   // this is the number of the shared memory segment

  //  pid = getpid();    // this gets process number of this program
  //  lnptr->pid= pid;   // this is the pid number for the SIGALRM signals


  printf("pid = %li \n",(long int)lnptr->pid);
  printf ("shm size = %li\n",sizeof (struct lnfill) );
  printf("... set shared memory...\n");

  return;
}
*/
/**************************************************************/

void readConf() {
  FILE *ifile;
  char line[200]="\0";
  //  char lnfill_conf[200]="/Users/c4g/src/LNfill/include/lnfill.conf";   //see define statement in lnfill.h 
  long int ii=0;
  int onoff=0, chanRTD=0, chanOFLO=0, Ibar=0, mani=0;
  int u1=-1, u2=-1, u3=-1, u4=-1;
  int mm=0;
  char name[10]="\0";
  double interval=0.0, max=0.0, min=0.0, limit=0.0, olimit=0.0;

  strcpy(lnfill_conf,path);
  strcat(lnfill_conf,"/");
  strcat(lnfill_conf,CONFIGFILE);
  for (ii=0; ii<20; ii++){
    strcpy(lnptr->ge[ii].name,name);
    lnptr->ge[ii].onoff = onoff;
    lnptr->ge[ii].interval = interval;
    lnptr->ge[ii].max = max;
    lnptr->ge[ii].min = min;
    lnptr->ge[ii].chanRTD = chanRTD;
    lnptr->ge[ii].chanOFLO = chanOFLO;
    lnptr->ge[ii].limit = limit;
    lnptr->ge[ii].olimit = olimit;
    lnptr->ge[ii].chanIbar = Ibar;
    //    for (mm=0;mm<4;mm++)strcpy(lnptr->ge[ii].shutdown[mm],"\0"); 
    lnptr->ge[ii].u1 = u1;
    lnptr->ge[ii].u2 = u2;
    lnptr->ge[ii].u3 = u3;
    lnptr->ge[ii].u4 = u4;
    
  }
  lnptr->tank.chanRTD = chanRTD;
  lnptr->tank.chanPRES = chanOFLO;
  lnptr->tank.timeout = max;
  lnptr->tank.limit = limit;
  lnptr->tank.olimit = olimit;
  lnptr->tank.chanIbar = Ibar;
  lnptr->tank.chanMani = mani;

/*
   Read configuration file
*/  
  
  if ( ( ifile = fopen (lnfill_conf,"r+") ) == NULL)
    {
      printf ("*** File on disk (%s) could not be opened: \n",lnfill_conf);
      printf ("===> %s \n",lnfill_conf);
      exit (EXIT_FAILURE);
    }

  fgets(line,150,ifile);    // reads column headers
  fgets(line,150,ifile);    // reads ----
/*
 Should be positioned to read file
*/
  while (1) {                   // 1 = true
    fgets(line,150,ifile);
    if (feof(ifile)) {
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
/*
   A line from the file is read above and processed below
*/
    mm = sscanf (line,"%li %s %i %lf %lf %lf %i %i %lf %lf %i %i %i %i %i", &ii, name, &onoff, &interval, &max, &min, &chanRTD, &chanOFLO, &limit, &olimit, &Ibar, &u1, &u2, &u3, &u4);
    //    printf("\n mm=%li %li %s %i %lf %lf %lf %i %i %lf %lf %i \n",mm, ii, name, onoff, interval, max, min, chanRTD, chanOFLO, limit,olimit,Ibar );
    if (ii == 21) {
	mm = sscanf (line,"%li %s %i %lf %lf %lf %i %i %lf %lf %i %i", &ii, name, &onoff, &interval, &max, &min, &chanRTD, &chanOFLO, &limit, &olimit, &Ibar,&mani);
    }
    
    if (ii == 999) {
      fgets(line,150,ifile);    // reads column headers
      mm = sscanf(line,"%s",lnptr->ibootbar_IP);
      fgets(line,150,ifile);    // reads ----
      mm = sscanf(line,"%s",lnptr->mpod_IP);
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
   
/*
   Now load the data into the shared memory structure
*/

    if (ii < 21) {
      strcpy(lnptr->ge[ii-1].name,name);
      lnptr->ge[ii-1].onoff = onoff;
      lnptr->ge[ii-1].interval = interval;
      lnptr->ge[ii-1].max = max;
      lnptr->ge[ii-1].min = min;
      lnptr->ge[ii-1].chanRTD = chanRTD;
      lnptr->ge[ii-1].chanOFLO = chanOFLO;
      lnptr->ge[ii-1].limit = limit;
      lnptr->ge[ii-1].olimit = olimit;
      lnptr->ge[ii-1].chanIbar = Ibar;
      lnptr->ge[ii-1].u1 = u1;
      lnptr->ge[ii-1].u2 = u2;
      lnptr->ge[ii-1].u3 = u3;
      lnptr->ge[ii-1].u4 = u4;
    } else {
      lnptr->tank.chanRTD = chanRTD;
      lnptr->tank.chanPRES = chanOFLO;
      lnptr->tank.timeout = max;
      lnptr->tank.limit = limit;
      lnptr->tank.olimit = olimit;
      lnptr->tank.chanIbar = Ibar;
      lnptr->tank.chanMani = mani;
    }
  }   // end of while statement
   
  for (ii=0; ii<20; ii++){
    printf("%2li %3s   %i   %0.lf    %0.lf    %0.lf    %4.lf    %4.lf   %i  %5i %5i %5i %5i \n",ii,lnptr->ge[ii].name,lnptr->ge[ii].onoff,lnptr->ge[ii].interval,lnptr->ge[ii].max,lnptr->ge[ii].min,lnptr->ge[ii].limit,lnptr->ge[ii].olimit,lnptr->ge[ii].chanIbar, lnptr->ge[ii].u1,lnptr->ge[ii].u2,lnptr->ge[ii].u3,lnptr->ge[ii].u4);
  }

  printf("21    TANK     1       0      %3.0lf       0     %2i      %2i      %3.0lf     %3.0lf      %i      %i \n", 
	  lnptr->tank.timeout,    lnptr->tank.chanRTD, lnptr->tank.chanPRES, lnptr->tank.limit, lnptr->tank.olimit, lnptr->tank.chanIbar, lnptr->tank.chanMani);
  //	  lnptr->tank.cooltime,    lnptr->tank.chanRTD, lnptr->tank.chanPRES, lnptr->tank.limit, lnptr->tank.olimit, lnptr->tank.chanIbar, lnptr->tank.chanMani);

  printf ("iBootBar IP = %s \n",lnptr->ibootbar_IP);
  printf ("mpod IP     = %s \n",lnptr->mpod_IP);

  
  return;
}

/**************************************************************/
double readRTD (long int adc) {
  long int error=0;
  double involts=0.0, deg=0.0;
  double ohms=0.0, degRTD=273.15, amps=0.004167; //I=V/R=4.92/1080 = 4.555 mA ; 200 - 50 uA
  long int DAC1Enable=1;
/*
  Read the ADC from the U6
*/

  error = eAIN(hU6, &caliInfo, adc, 0, &involts, LJ_rgBIP1V, 8, 0, 0, 0, 0);   // LJ_rgBIP1V data range   U6 command
  //  error = eAIN(hU3, &caliInfo, 0, &DAC1Enable, adc, 31, &involts, 0, 0, 0, 0, 0, 0);   //U3 command
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


    printf("%li deg= %.1lf \n",adc,deg);

  return (deg);
}


/**************************************************************/
int labjackSetup(){
  long int localID=-1;
  long int count=0;
  long int error=0;

/*
  Open first found U6 over USB
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

  return 0;
}


/***********************************************************/
void resetU6(uint8 res){
/*
  Resets U6
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

  //  if( (error = LJUSB_BulkWrite(hU3, U3_PIPE_EP1_OUT, resetIn, 4)) < 4){
  if( (error = LJUSB_BulkWrite(hU6, U6_PIPE_EP1_OUT, resetIn, 4)) < 4){
    LJUSB_BulkRead(hU6, U6_PIPE_EP2_IN, resetOut, 4); 
    //    LJUSB_BulkRead(hU3, U3_PIPE_EP2_IN, resetOut, 4); 
    printf("U6 Reset error: %s\n", errormsg[(int)resetOut[3]]);
    closeUSBConnection(hU6);
    return;
  }
  printf ("....U6 device reset \n");
  return;
}
/***********************************************************/

void snmpShutdown(int index, char *cmdResult){
//void snmp(int setget, char *cmd, char *cmdResult) {
  //  pid_t wait(int *stat_loc);
  FILE *fp;
  int status;
  char com[150];
  char res[200];
  res[0] = '\0';

  printf("Shutting down HV u%i \n ",index);
  sprintf (com,"snmpset -OqvU -v 2c -m %s -c guru %s outputVoltage.u%i F 0.0",mpod_mib,lnptr->mpod_IP,index);     //!@#$%^& -r retry num (default=5); -t num time of timeout (default=1 sec)
  //  sprintf (com,"snmpset -r 15 -t 5 -v 2c -L o -m %s -c guru %s outputSwitch.u%i i 3",MPOD_MIB,MPOD_IP,index);     //!@#$%^& -r retry num (default=5); -t num time of timeout (default=1 sec)
  //  strcat(com,cmd);                             // add command to snmp system call
  printf("%s\n",com);

  fp = popen(com, "r");   // open child process - call to system; choosing this way

  if (fp == NULL){                             // so that we get the immediate results
    printf("<br/>Could not open to shell!\n");
    strncat(cmdResult,"popen error",39);
    return;
  }

  /* process the results from the child process  */  
    while (fgets(res,200,fp) != NULL){
      fflush(fp);  
    } 

  /* close the child process  */  
  status = pclose(fp);

  if (status == -1) {
    printf("<p>Could not issue snmp command to shell or to close shell!</p>\n");
    strncat(cmdResult,"pclose error",139);
    return;
  } 
  else {
    //     wait();
     strncpy(cmdResult,res,139);   // cutoff results so no array overflows
  }

  return;
}


/***********************************************************/
void snmp(int setget, char *cmd, char *cmdResult) {
  //  pid_t wait(int *stat_loc);
  FILE *fp;
  int status;
  char com[150];
  char res[200];
  res[0] = '\0';

/*
   setget flag chooses if this is a 0=read (snmpget) or 1=write (snmpset) MPOD shutdown done in other routine
*/
  if (setget == 1) {
    sprintf (com,"snmpset -r 15 -t 5 -v 1 -L o -m %s -c private %s ",ibootbar_mib,lnptr->ibootbar_IP);
  } 
  else if (setget == 0){
    sprintf (com,"snmpget -r 15 -t 5 -v 1 -L o -m %s -c public  %s ",ibootbar_mib,lnptr->ibootbar_IP);
  }   //-r retry num (default=5); -t num time of timeout (default=1 sec)
  //  else if (setget == 2){              // if setget =2 then issue HV shutdown
  //    sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s outputSwitch.u%i i 3",MPOD_MIB,MPOD_IP);     //!@#$%^& 
    //  }
    strcat(com,cmd);                             // add command to snmp system call
  //  printf("%s\n",com);

  fp = popen(com, "r");   // open child process - call to system; choosing this way

  if (fp == NULL){                             // so that we get the immediate results
    printf("<br/>Could not open to shell!\n");
    strncat(cmdResult,"popen error",39);
    return;
  }

  /* process the results from the child process  */  
    while (fgets(res,200,fp) != NULL){
      fflush(fp);  
    } 

  /* close the child process  */  
  status = pclose(fp);

  if (status == -1) {
    printf("<p>Could not issue snmp command to shell or to close shell!</p>\n");
    strncat(cmdResult,"pclose error",139);
    return;
  } 
  else {
    //     wait();
     strncpy(cmdResult,res,139);   // cutoff results so no array overflows
  }

  return;
}

/*************************************************************/
void updateStatus (int mm, int nn){
  int ii=0;
  char *xxxx;

  xxxx=lnptr->bitstatus;
  ii = nn;
  ii = 6+2*ii;                      // find correct field
  if (mm == 1) *(xxxx+ii) = '1';
  else *(xxxx+ii) = '0';

}
/*************************************************************/
void openTank (){
/*  
  Open tank valve
*/
  int mm=0, set=1, get=0;
  char cmd[150], cmdResult[140];

  while (mm == 0){
    sprintf(cmd,"outletCommand.%i i 1",lnptr->tank.chanIbar);
    snmp(set, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);
    sleep(5);
    
    sprintf(cmd,"outletStatus.%i",lnptr->tank.chanIbar);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);       // when mm=1 valve opened
    updateStatus(mm,lnptr->tank.chanIbar);
  }

  strcpy(lnptr->tank.status,"OPEN");   

  return;
}

/*************************************************************/
void closeTank (){
/*  
  Close tank valve
*/
  int mm=1, set=1, get=0;
  char cmd[150], cmdResult[140];

  printf (" Tank valve closing \n");   
  while (mm == 1){
    sprintf(cmd,"outletCommand.%i i 0",lnptr->tank.chanIbar);
    snmp(set, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);
    sleep(5);
    
    sprintf(cmd,"outletStatus.%i",lnptr->tank.chanIbar);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);       // when mm=0 valve closed
    updateStatus(mm,lnptr->tank.chanIbar);

    }
  strcpy(lnptr->tank.status,"CLOSED");   

  //    printf (" Tank valve closed  : mm = %i\n", mm);
    
    return;
}

/*************************************************************/
void openMani (){
/*  
  Open manifold valve
*/
  int mm=0, set=1, get=0;
  char cmd[150], cmdResult[140];
  printf (" Tank mani opening \n");       
  while (mm == 0){
    sprintf(cmd,"outletCommand.%i i 1",lnptr->tank.chanMani);
    snmp(set, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);
    sleep(5);
        
    sprintf(cmd,"outletStatus.%i",lnptr->tank.chanMani);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);       // when mm=1 manifold valve open
    updateStatus(mm,lnptr->tank.chanMani);
    //        printf (" mm = %i\n", mm);
  }
    //    strcpy(lnptr->tank.status,"Cool/Vent");   
    //    printf (" Manifold valve opened  : mm = %i\n", mm);
    
  return;
}
/*************************************************************/
void closeMani (){
/*  
  Close manifold valve
*/

    int mm=1, set=1, get=0;
    char cmd[150], cmdResult[140];

    printf (" Tank mani closing \n");       
    while (mm == 1){
        sprintf(cmd,"outletCommand.%i i 0",lnptr->tank.chanMani);
        snmp(set, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
        mm = readOnOff(cmdResult);
        sleep(5);
        
        sprintf(cmd,"outletStatus.%i",lnptr->tank.chanMani);
        snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
        mm = readOnOff(cmdResult);       // when mm=0 manifold valve closed
	updateStatus(mm,lnptr->tank.chanMani);
	//        printf (" mm = %i\n", mm);
    }
    //    strcpy(lnptr->tank.status,"Vented");   
    //    printf (" Manifold valve closed  : mm = %i\n", mm);
    
    return;
}
/*************************************************************/
void openDet (int ii){
/*  
  Open detetctor valve
*/

  int mm=0, set=1, get=0;
  char cmd[150], cmdResult[140];
  
  printf (" Detectors opening %s \n", lnptr->ge[ii].name);      
  while (mm == 0){
    sprintf(cmd,"outletCommand.%i i 1",lnptr->ge[ii].chanIbar);
    snmp(set, cmd, cmdResult);       // set the outlet on
    mm = readOnOff(cmdResult);
    sleep(5);

    sprintf(cmd,"outletStatus.%i",lnptr->ge[ii].chanIbar);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);
    //        printf (" mm = %i\n", mm);
    updateStatus(mm,lnptr->ge[ii].chanIbar);
  }
  strcpy(lnptr->ge[ii].status,"FILL");
  
    //    printf (" Detector valve opened  : mm = %i\n", mm);
    
  return;
}
/*************************************************************/
void closeDet (int ii){
/*  
  Close detetctor valve
*/

  int mm=1, set=1, get=0;
  char cmd[150], cmdResult[140];
    
  printf (" Detectors closing %s \n",lnptr->ge[ii].name);  
  while (mm == 1){
    sprintf(cmd,"outletCommand.%i i 0",lnptr->ge[ii].chanIbar);
    snmp(set, cmd, cmdResult);       // set the outlet on
    mm = readOnOff(cmdResult);
    sleep(5);

    sprintf(cmd,"outletStatus.%i",lnptr->ge[ii].chanIbar);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);
    updateStatus(mm,lnptr->ge[ii].chanIbar);
	//        printf (" mm = %i\n", mm);
  }
  //  strcpy(lnptr->ge[ii].status,"OK");
  updateRTD();  
    //    printf (" Detector valve %i closed  : mm = %i\n", ii,mm);
    
  return;
}

/*************************************************************/
void fillGe (){
/*
     fill all the detctors
*/
  int ii=0, jj=0, kk=0, mm=0, init=0;
  int active[6], activemax=0, actkk[6];
  time_t xtime=0;

    /*
    printf("Block signals attempt\n");
    sigemptyset(&mask);
    sigaddset (&mask,SIGALRM);

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
      perror ("sigprocmask");
      return;
    }
    */
  if (lnptr->command == 9) {   // initial fill set next fill to 60 minutes
    init=1;
    lnptr->command = 8;         // change to fillGe command
  }
  for (ii=0;ii<6;ii++){
    active[ii]=0;
    actkk[ii]=0;    
    lnptr->ge[ii].last = 0;     // zero the last fill to ensure an old reading is taken as new
  }
  activemax = 0;
  if (lnptr->com1 == -1){
    strcpy(lnptr->comStatus,"FILLING ALL");              // all detector fill
    jj = 0;
    for (ii=0; ii<20; ii++){
      if (lnptr->ge[ii].onoff == 1){
	active[jj++] = ii;
	activemax++;
      }
    }
  }
  else {
    sprintf(lnptr->comStatus,"FILLING Ge %i",lnptr->com1);     // single detector fill
    activemax = 1;
    active[0] = lnptr->com1;
    //    lnptr->com1-1;
  }
  
  //    printf("Activemax = %i\n",activemax);
  if (activemax==0) return;
  sendEmail("Fill started");
  openTank();
  sleep(5);
  openMani();
  updateRTD();                                    // read RTDs
  
  kk = 0;
  xtime = time(NULL) + lnptr->tank.timeout;
  while (kk < 4){                               // while counter of number of times checked            
    updateRTD();                                // read RTDs
    if(lnptr->tank.rtd < lnptr->tank.olimit){   // see if below limit
      kk++;                                     // increment counter
      if (kk > 3){                              // if cold on 4 consecutive reads its filled 
	closeMani();                            // close detector
	break;                       // break out of while loop?
      }
    }
    else {
      kk=0;                                     // reset counter to 0...false readings occur from splashes of LN
    }
    printf("Mani below olimit kk = %i\n",kk);
    sleep(5);
    if (lnptr->command != 8) {
      strcpy(lnptr->comStatus,"FILL INTERRUPTED");
      return;     // another interupt signal was sent ...go to users needs to manual get things right
    }
  }
  sleep(5);    // sleep to allow time for network response to closing mani 
/*
   Fill detector(s)
*/

  for (ii=0; ii<activemax; ii++){
    openDet(active[ii]);
    if (lnptr->command != 8) {
      strcpy(lnptr->comStatus,"FILL INTERRUPTED");
      return;     // another interupt signal was sent ...go to users needs to manual get things right
    }  
  }
  
  //    updateRTD();
  
  xtime = time(NULL);
  mm = activemax;
  for (ii=0; ii<activemax; ii++){
    actkk[ii] = 0;                               // set up counter for times manifold is cold enough (suppresses splashes)
  }
  
  while (mm > 0){
    updateRTD();                                 // read RTDs
    /*
      lnptr->ge[active[0]].oflo = 80;              // fake rtd values
      lnptr->ge[active[1]].oflo = 80;
      lnptr->ge[active[2]].oflo = 80;
      lnptr->ge[active[3]].oflo = 80;
      lnptr->ge[active[4]].oflo = 80;
      lnptr->ge[active[5]].oflo = 80;
    */
    for (ii=0; ii<activemax; ii++){               // go thru active detectors
      curtime = time(NULL);
      if (actkk[ii] != 9){
	if (actkk[ii] <= 4) {                     // set to 9 when detector finished filling
	  if(lnptr->ge[active[ii]].oflo < lnptr->ge[active[ii]].olimit){   // check if oflo cold
	    actkk[ii] = actkk[ii] + 1;            // record that oflo was colder
	    if (actkk[ii] > 3) {                  // read overflo values 4 times to be sure they are cold
	      mm--;                               // reduce the while count by 1
	      closeDet(active[ii]);               // close the detector that is filled
	      actkk[ii] = 9;                      // set the counter high so it doesn't get reset to 0
	      strcpy(lnptr->ge[active[ii]].status, "OK");                              // Set detector status OK
	      lnptr->ge[active[ii]].last = curtime - xtime;                            // record fill duration
	      if (init == 0) lnptr->ge[active[ii]].next = curtime + lnptr->ge[active[ii]].interval; // set next filling time to conf setting
	      else lnptr->ge[active[ii]].next = curtime + 3600;   // set next filling time to 1 hour
	    }                                     // closeDet has a sleep period - one path thru loop
	    else {
	      sleep (5);                          // sleep until next rtd check - one path thru loop
	    }
	  }
	  else {                                
	    actkk[ii] = 0;                        // reset the counter -- not a full LN stream
	    sleep (5);                            // sleep until next rtd check - one path thru loop
	  }
	}
	if (curtime > (xtime + lnptr->ge[ii].max)) {               // check for TOO LONG fill
	  mm--;                                                    // reduce the while count by 1
	  closeDet(active[ii]);                                    // close the detector that is filled
	  actkk[ii] = 9;                                           // set the counter high so it doesn't get reset to 0
	  if (init == 0) lnptr->ge[active[ii]].next = curtime + lnptr->ge[active[ii]].interval; // set next filling time to conf setting
	  else lnptr->ge[active[ii]].next = curtime + 3600;        // set next filling time to 1 hour
	  strcpy(lnptr->ge[active[ii]].status, "LONG");            // this also has a sleep so 
	  sendEmail("Long LN fill");
	}
      }
    }
/*
    for (ii=0; ii<activemax; ii++){                             // go thru active detectors send email if any took too long
      if (strcmp(lnptr->ge[ii].status,"LONG") == 0) sendEmail;
    }

    Filling has finished....
*/    
    if (lnptr->command != 8) {
      strcpy(lnptr->comStatus,"FILL ALL INTERRUPTED");
      return;     // another interupt signal was sent ...go to users needs to manual get things right
    }
  }  // end of while statement
  
  for (ii=0; ii<activemax; ii++){   // reset all the variables
    actkk[ii] =0;
  }
  mm=0;
  activemax=0;
  init = 0;

  sleep(5);                          // add sleep to give time for network to respond
  closeTank();
  sleep(5);                          // add sleep to give time for network to respond
  printf ("Manifold open to release pressure....");
  openMani();                        // leave mani closed or else solenoid will burn up
  sleep(5);                          // add sleep to give time for network to respond
  updateRTD();
  printf ("SLEEPING 60 SECONDS TO CLOSE MANIFOLD so it doesn't overheat!\n");
  sleep (60);
  printf ("Closing manifold...!\n");
  closeMani();
  sendEmail("Fill completed");
/*

*/
  return;
}


/*************************************************************/
void closeAllValves (){
  int ii=0;

  closeTank();
  sleep(5);
  printf ("Manifold open to release pressure....");
  openMani();    // leave mani closed or else solenoid will burn 

  for (ii=0; ii<20; ii++){
    if (lnptr->ge[ii].onoff == 1) closeDet(ii); 
  }

  printf ("SLEEPING 60 SECONDS TO CLOSE MANIFOLD so it doesn't overheat!\n");
  sleep (60);
  printf ("Closing manifold...!\n");
  closeMani();

  return;
}

/*************************************************************/
void outletStatus (){
  int ii=0, mm=0, get=0;
  char cmd[150], cmdResult[140];
  char cc[24]="\0";


  sprintf(cc,"outl: ");

  for (ii=0; ii<8; ii++){
    sprintf(cmd,"outletStatus.%i",ii);
    snmp(get, cmd, cmdResult);       // set the outlet on tank overflow and tank valve
    mm = readOnOff(cmdResult);       // when mm=0 manifold valve closed

    if (mm == 0) strcat(cc,"0 ");
    else strcat(cc,"1 ");
  }

  strcpy(lnptr->bitstatus,cc);

  return;
}

/******************************************************************************/

float readFloat(char *cmdResult) {
  int kk;
  char *jj;             //pointer 
  float zz;
  char ss[140];

  jj = strstr(cmdResult,"Float:"); 
  strcpy(ss,(jj+7));
  kk = strlen (ss);
  if (kk > 2) zz=strtod(ss,&jj);            //mm = sscanf(ss,"%f %s", zz, uu);

  return (zz);
}

/******************************************************************************/

unsigned int readBits(char *cmdResult) {
  int a, b, c, d;
  char *jj;             //pointer 
  char ss[140];
  unsigned int ff;
  char sa[2]="\0", sb[2]="\0", sc[2]="\0", sd[2]="\0";

  jj = strstr(cmdResult,"BITS:"); 
  strcpy(ss,(jj+6));

  snprintf (sa,1,"%c",ss[0]); 
  snprintf (sb,1,"%c",ss[1]); 
  snprintf (sc,1,"%c",ss[3]); 
  snprintf (sd,1,"%c",ss[4]); 
  
  a = atoi(sa);
  b = atoi(sb);
  c = atoi(sc);
  d = atoi(sd);
  /*
  */
  ff = a*4096 + b*256 + c*16 + d;

  return (ff);
}

/******************************************************************************/

int readInt(char *cmdResult) {
  char *jj;             //pointer 
  char ss[140];
  int nn;

  jj = strstr(cmdResult,"INTEGER:"); 
  strcpy(ss,(jj+9));
  nn=strtol(ss,&jj,10);            //mm = sscanf(ss,"%f %s", zz, uu);

  return (nn);
}
/******************************************************************************/

int readOnOff(char *cmdResult) {
  char *jj, *gg;             //pointer 
  char ss[140];
  int nn;

  jj = strstr(cmdResult,"("); 
  gg = strstr(cmdResult,")"); 
  strcpy(ss,(jj+1));

  ss[gg-jj-1]='\0';     // 0, 1 on/off; 10 = clear flags

  nn=atoi(ss);

  return (nn);
}
/******************************************************************************/
void sendEmail(char subject[]){
  FILE *fp;
  int status;
  char com[150];
  int ii=0,jj=0;
/*
    Modify below by commenting in or out the appropriate email commands
*/
  printf("%i\n",lnptr->maxAddress);
  for (jj=0; jj < lnptr->maxAddress; jj++){
    printf("loop is: %i %i\n",jj,(int)lnptr->maxAddress);
    //    sprintf (com,"mail -s \"Liquid nitrogen filling failure\" %s < include/fill-failed.txt",lnptr->ge[ii].email);
// ORNL     
    sprintf (com,"mail -s \"LN\" %s",lnptr->ge[jj].email);
// ANL
//    sprintf (com,"mail -s \"%s\" -r \"torben@anl.gov\" %s",subject,lnptr->ge[jj].email);

    //                     1         2         3         4         5         6         7         8
    //            12345678901234567890123456789012345678901234567890123456789012345678901234567890 + 40 for email addresses

    printf("%s\n",com);

    //    fp = popen(com, "r+");   // open child process - call to system; choosing this way..works on APPLE
    fp = popen(com, "w");   // open child process - call to system; choosing this way
    if (fp == NULL){                             // so that we get the immediate results
      printf("<br/>Could not open to shell!\n");
    //    strncat(cmdResult,"popen error",39);
      return;
    }
    fprintf(fp,"Valves:%s\n",lnptr->bitstatus);
    for (ii=0; ii< 20; ii++){
      if (lnptr->ge[ii].onoff == 1){
     	fprintf(fp,"%i-%s-%s T/L=%.1f/%.0f duration/next=%.0lf/%0.lf\n",ii,lnptr->ge[ii].name,lnptr->ge[ii].status,lnptr->ge[ii].rtd,lnptr->ge[ii].limit,lnptr->ge[ii].last,(lnptr->ge[ii].next-((double)time(NULL))));
	printf("%i-%s-%s T/L=%.1f/%.0f duration/next=%.0lf/%0.lf\n",ii,lnptr->ge[ii].name,lnptr->ge[ii].status,lnptr->ge[ii].rtd,lnptr->ge[ii].limit,lnptr->ge[ii].last,(lnptr->ge[ii].next-time(NULL)));
      }
    }
        fprintf(fp,".");
    fflush(fp);  
  /* process the results from the child process    
    while (fgets(res,200,fp) != NULL){
      printf("sendEmail: %s\n");
      fflush(fp);  
    } 
 */  
  /* close the child process  */  
    status = pclose(fp);
    printf("status = %i\n",status);
  }

  return;
}

/******************************************************************************/

void readEmail() {
  FILE *ifile;
  char line[200]="\0";
  //  char lnfill_mail[200]="/Users/c4g/src/LNfill/include/lnfill-email.txt";   //see define statement in lnfill.h 
  char lnfill_mail[200]="include/lnfill-email.txt";   //see define statement in lnfill.h 
  int mm=0, jj=0;
  char ans[40];
/*
   Read configuration file
*/  
  if ( ( ifile = fopen (lnfill_mail,"r+") ) == NULL) {
    printf ("*** File on disk (%s) could not be opened: \n",lnfill_mail);
    printf ("===> %s \n",lnfill_mail);
    exit (EXIT_FAILURE);
  }
/*
 Should be positioned to read file
*/
  lnptr->maxAddress=0;
  while (1) {                   // 1 = true
    fgets(line,150,ifile);
    printf("%s",line);

    if (feof(ifile)) {
      mm = fclose(ifile);
      if (mm !=0) printf ("File close error\n");
      break;
    }
/*
   A line from the file is read above and processed below
*/
//    sprintf(lnptr->email[0],"Hi Carl");
    mm = sscanf (line,"%s",ans);
    jj = strlen(ans);

    if (jj > 3 && strchr(ans,'#') == NULL) {
      strcpy(lnptr->ge[lnptr->maxAddress].email, ans);
      lnptr->maxAddress++;
      printf("%i\n",(int)lnptr->maxAddress);
    }
  }
  return;
}


/******************************************************************************/
//  ii = strlen(cmdResult);
//  jj = strstr(cmdResult,"INTEGER:"); 

