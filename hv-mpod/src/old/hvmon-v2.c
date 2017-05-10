/*
  Program hvmon to monitor detector High Voltage
  Understands Wiener MPOD and CAEN x527

  To be compiled with:                                           
     gcc -Wall -lcaenhvwrapper -lpthread -ldl -lm -o hvmon hvmon-v1.c
*/

#include "../include/hvmon.h"
#include "../include/CAENHVWrapper.h"

#define MPOD_MIB     "../include/WIENER-CRATE-MIB.txt"
#define CONFIGFILE   "../include/hvmon.conf"
#define INTERVAL 60  //wake up every 15 seconds 

void readConf();                  // processes configuration file
void readConfNew();                  // processes configuration file
int mmapSetup();                  // sets up the memory map

//  MPOD SNMP related commands

void snmp(int setget, int ii, char *cmd, char *cmdResult);   // snmp get/set commands
int readOnOff(char *cmdResult);                              // snmp get/set results (on/off)
int readInt(char *cmdResult);                                // snmp get/set results (integer)
float readFloat(char *cmdResult);                            // snmp get/set results (floats)
unsigned int readBits(char *cmdResult);                      // snmp get/set results (bits)

//  CAENHVWrapper related commands

void setTimer();                  // sets the timer for signals
void alarm_wakeup();
struct itimerval tout_val;        // needed the timer for signals
void handlerCommand(int sig, siginfo_t *siginfo, void *context);  // main handler for signal notification and decisions
void signalBlock(int pp);
sigset_t mask;
sigset_t orig_mask;
struct sigaction act;

char         path [PATH_MAX+1];                  // PATH_MAX defined in a system libgen.h file
char     mpod_mib [PATH_MAX+100]="\0";
char ibootbar_mib [PATH_MAX+100]="\0";

void mpodHV();
void caenGet();
void caenSet();
void caenCrate();
void caenGetBySlot();
void hvOnAll();
void hvOffAll();
void recoverCAEN();
void getTemp();
void changeParam();
void getVMon(int ii);
void getIMon(int ii);
void getV0Set(int ii);
void getI0Set(int ii);
void getPw(int ii);
void getRUp(int ii);
void getV1Set(int ii);
void getI1Set(int ii);
void getSVMax(int ii);
void getRDWn(int ii);
void getTrip(int ii);
void getTripInt(int ii);
void getTripExt(int ii);
void getName(int ii);
void setV0Set(int ii);
void setI0Set(int ii);
void setPw(int ii);
void setRUp(int ii);
void setV1Set(int ii);
void setI1Set(int ii);
void setSVMax(int ii);
void setRDWn(int ii);
void setTrip(int ii);
void setTripInt(int ii);
void setTripExt(int ii);

time_t time0=0, time1=0, nread;
int indexMax=0;

// for CAEN crate mapping
//char *model, *descModel;
//unsigned short numOfSlots;
//unsigned short *numOfChan, *serNumList;
//unsigned char  *firmwareMin, *firmwareMax;

/***********************************************************/
int main(int argc, char **argv){
/*

*/
  int ii=0, result=0;
  //  long int p0=0, p1=1, count=0, etime=0;
  int mapHVmon;
  pid_t pid;
  long int p0=0,p1=1;
  
  printf("Working directory: %s\n",getcwd(path,PATH_MAX+1));
  strcpy(mpod_mib,path);                  // copy path to mpod mib file
  strcat(mpod_mib,"/");                   // add back the ending directory slash
  strcat(mpod_mib,MPOD_MIB);              // tack on the mib file location
  printf("    mpod_mib file: %s\n",mpod_mib);
/*
  Memory map creation and attachment
  the segment number is stored in hvptr->pid
*/
  mapHVmon = mmapSetup();
  if (mapHVmon == -1) return 0;
  /*  
   Set up the signal capture routine for when the reading
   program wants something changed
*/
  pid = getpid();    // this gets process number of this program
  hvptr->pid= pid;   // this is the pid number for the SIGALRM signals

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
//  readConf();
  readConfNew();

/*  
  Setup time of next fill based on current time and configure file
*/
  time0 = time(NULL);            // record starting time of program
  hvptr->time0=time0;
  //  time1 = time0 + INTERVAL;      // set up next read of voltages...usually every minute
/*  
  Get the first read of the HV data
*/
  hvptr->com0 = 2;
   // mpodHV();
  //  caenGet();
/*  
  Setup monitoring loop to look for changes/minute and requests   
*/
  nread = INTERVAL;
  hvptr->com0 = 2;

  while(hvptr->com0 != -1) {
    //    for (ii=0; ii<indexMax; ii++){
    //      if (hvptr->xx[ii].type == 0) {   
	//	printf ("mon  MPOD  %i \n",ii);

    switch ( hvptr->com0 ) { 
    
    case  1:              // do nothing as calling program will use what is in shared memory
      hvptr->com0 = 0;    // set comand back to regular reading
      break;

    case  2:                   // do a regular or forced read...curtime is reset
      //      hvptr->time1=time(NULL);
      hvptr->secRunning = time(NULL) - hvptr->time0;
      signalBlock(p1);
      mpodHV(); 
      //    caenSet();
      caenGet();
//      getTemp();

      signalBlock(p0);
      hvptr->com0 = 0;    // set comand to regular reading or else we lose touch with the CAEN module
/*
      for (ii=0; ii<indexMax; ii++){
	printf("i = %i, VSet = %f,  Vred = %f, Vramp = %f, Ired = %f Imax = %f OnOFF = %i\n",ii,
	       hvptr->xx[ii].vSet, hvptr->xx[ii].vMeas, hvptr->xx[ii].vRamp, hvptr->xx[ii].iMeas,
	       hvptr->xx[ii].iSet,hvptr->xx[ii].onoff);
    }
*/
      break;
    case 3:
      hvptr->com0 = 2;    // set comand to regular reading or else we lose touch with the CAEN module
      break;
    case 4:                 // HV On all
      signalBlock(p1);
      hvOnAll();
      signalBlock(p0);
      //      caenGetBySlot();
      hvptr->com0 = 2;    // things by slots...may not need
      break;
    case 5:                // HV Off All
      signalBlock(p1);
      caenGetBySlot();
      hvOffAll();
      signalBlock(p0);
   
      hvptr->com0 = 2;    // things by 
      break;
      
    case 7:                 // Recover from hardware...
      printf(" Recovering data from hardware....\n");
      signalBlock(p1);
      //      caenGetBySlot();
      recoverCAEN();
      signalBlock(p0);
      hvptr->com0 = 2;    // set comand to regular reading or else we lose touch with the CAEN module
      //      sleep(10);
      break;
    case 16:
      printf(" Changing parameters of one detector....\n");
      signalBlock(p1);
      changeParam();
      signalBlock(p0);
      hvptr->com0 = 2;    // set comand to regular reading or else we lose touch with the CAEN module
     break;
      
    default:
      hvptr->com0 = 2;
      sleep (INTERVAL);
      //      hvptr->com0 = 2;
      break;
    }  // end of switch statement    hvptr->com0 = -1;
  }  // end of while statement

  // CLOSE THE CAEN SYSTEMS HERE!!!!
  for (ii=0; ii<hvptr->maxchan; ii++){
    if (hvptr->xx[ii].type == 1) {
      result = CAENHV_DeinitSystem(hvptr->xx[ii].caenH);
      if (result != 0) printf("Error deattaching to CAEN system %s \n",hvptr->xx[indexMax].ip);
      else {
	printf ("Successful logout of CAEN crate!\n");
	break;
      }
    }
  }
  
/*
   Release the shared memory and close the U3

*/
  if (munmap(hvptr, sizeof (struct hvmon*)) == -1) {
    perror("Error un-mmapping the file");
/* Decide here whether to close(fd) and exit() or not. Depends... */
  }
  close(mapHVmon);

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
void handlerCommand(int sig, siginfo_t *siginfo, void *context){
/*
  Handlers are delicate and don't have all protections the rest
  of the user code has, so get in and out quickly...RLV recommendation!
*/

  printf ("I caught signal number %i ! \n",hvptr->com0);

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
   fd = open(HVMONDATAPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0644);
   if (fd == -1) {
        perror("Error opening hvmon file for writing");
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
   for (ii=0; ii<HVMONDATASIZE; ii++){
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
   hvptr = (struct hvmon*) mmap(0, HVMONDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (hvptr == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the hvmon file");
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

/******************************************************************************/
void readConf() {
  FILE *ifile;
  char line[200]="\0";
  char hvmon_conf[200] ="\0";
  int ii=0, mm=0, slot=0, chan=0, onoff=0;
  float volts=0.0, current=0.0, ramp=0.0;
  char ip[30]="\0", name[15]="\0";
  char ipsave[10][30];
  int kk=0,new=0, maxIP=0;

  strcpy(hvmon_conf,path);
  strcat(hvmon_conf,"/");
  strcat(hvmon_conf,CONFIGFILE);
  printf("    mpod_mib file: %s\n",hvmon_conf);

/*
   Read configuration file
*/  
  if ( ( ifile = fopen (hvmon_conf,"r+") ) == NULL) {
    printf ("*** File on disk (%s) could not be opened: \n",hvmon_conf);
    printf ("===> %s \n",hvmon_conf);
    exit (EXIT_FAILURE);
  }

  fgets(line,200,ifile);    // reads column headers
  fgets(line,200,ifile);    // reads ----
/*
 Should be positioned to read file
*/
  indexMax = 0;
  while (1) {                   // 1 = true
    fgets(line,200,ifile);
    if (feof(ifile)) {
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
/*
   A line from the file is read above and processed below
*/
    mm = sscanf (line,"%i", &ii);
    onoff = -1;
    chan = -1;
    slot = -1;
    volts = 0.0;
    ramp = 0.0;
    current = 0.0;
    strcpy (name,"\0");
/*
  Process MPOD and other SNMP data
*/    
    if (ii == 0) {
      mm = sscanf (line,"%i %s %u %u %f %f %f %14s", &ii, ip, &chan, &slot, &volts, &ramp, &current, name);  // CAEN data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].slot = slot;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].vRamp = ramp;
      hvptr->xx[indexMax].iSet = current;
      strcpy(hvptr->xx[indexMax].name,name);
      strcpy(hvptr->xx[indexMax].ip,ip);


      /*
      mm = sscanf (line,"%i %s %u     %f", &ii, ip, &chan, &volts);  // MPOD data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].slot = slot;
      strcpy(hvptr->xx[indexMax].ip,ip);
      */
      printf ("MPOD = %s %3u    %7.1f %i\n", hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan, hvptr->xx[indexMax].vSet,hvptr->xx[indexMax].type);
    }
    
/*
  Process CAEN 1527, 2527, ... HV supplies
*/    
    if (ii == 1) {
      mm = sscanf (line,"%i %s %u %u %f %f %f %14s", &ii, ip, &chan, &slot, &volts, &ramp, &current, name);  // CAEN data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].slot = slot;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].vRamp = ramp;
      hvptr->xx[indexMax].iSet = current;
      strcpy(hvptr->xx[indexMax].name,name);
      strcpy(hvptr->xx[indexMax].ip,ip);

      new = 0;
      for (kk=0; kk<maxIP; kk++) {                         // check if multiple CAEN units; new = a new IP found
	if (strcmp(ipsave[kk],ip) == 0) new = 1;              
      }
      if (new == 0) {
	strcpy(ipsave[maxIP],ip);      //...save and connect to get the handle: ceanH
      	printf("new IP: maxcaen = %i, IP = %s \n",maxIP,ipsave[maxIP]);
	maxIP++;
	//	hvptr->maxcaen++;
	caenCrate();
      }
      else hvptr->xx[indexMax].caenH = hvptr->xx[indexMax-1].caenH;   // copy previous caen Handle to this entry
    
      printf ("CAEN = %s %3u %3u %7.1f %7.1f %7.1f %i %s \n",hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan,
	      hvptr->xx[indexMax].slot, hvptr->xx[indexMax].vSet, hvptr->xx[indexMax].vRamp, hvptr->xx[indexMax].iSet,
	      hvptr->xx[indexMax].type, hvptr->xx[indexMax].name);
    }
/*
    Reached the end of the configuration file data
*/
    if (ii == -1) {
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
    
    indexMax++;            // increment indexes 
  }   // end of while statement
  hvptr->maxchan = indexMax;
  printf ("%i HV entries found on MPODs and/or %i CAEN HV supplies\n",hvptr->maxchan, maxIP);
  
  return;
}
/******************************************************************************/
void readConfNew() {
  FILE *ifile;
  char line[200]="\0";
  char hvmon_conf[200] ="\0";
  int ii=0, mm=0, slot=0, chan=0, onoff=0, itrip=0, etrip=0;
  float volts=0.0, current=0.0, ramp=0.0, trip=0.0, dramp=0.0, svmax=0.0, v1set=0.0, i1set=0.0;
  char ip[30]="\0", name[15]="\0";
  char ipsave[10][30];
  int kk=0,new=0, maxIP=0;
    
  strcpy(hvmon_conf,path);
  strcat(hvmon_conf,"/");
  strcat(hvmon_conf,CONFIGFILE);
  printf("    mpod_mib file: %s\n",hvmon_conf);

/*
   Read configuration file
*/  
  if ( ( ifile = fopen (hvmon_conf,"r+") ) == NULL) {
    printf ("*** File on disk (%s) could not be opened: \n",hvmon_conf);
    printf ("===> %s \n",hvmon_conf);
    exit (EXIT_FAILURE);
  }

  fgets(line,200,ifile);    // reads column headers
  fgets(line,200,ifile);    // reads column headers units
  fgets(line,200,ifile);    // reads ----
/*
 Should be positioned to read file
*/
  indexMax = 0;
  while (1) {                   // 1 = true
    fgets(line,200,ifile);
    if (feof(ifile)) {
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
/*
   A line from the file is read above and processed below
*/
    mm = sscanf (line,"%s %i", ip, &ii);  // read ip and type to determine CAEN or MPOD
    printf (" ip=%s , type = %i\n", ip, ii);
    onoff = -1;
    chan = -1;
    slot = -1;
    volts = 0.0;
    ramp = 0.0;
    current = 0.0;
    strcpy (name,"\0");
    dramp = 0.0;
    svmax = 0.0;
    v1set = 0.0;
    i1set = 0.0;
    trip = 0.0;
    itrip = 0;
    etrip = 0;
/*
  Process MPOD and other SNMP data
*/    
    if (ii == 0) {
      mm = sscanf (line,"%s %i %u %u %14s %f %f %f ", ip, &ii, &slot, &chan, name, &volts, &current, &ramp);  // MPOD data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].slot = slot;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].vRamp = ramp;
      hvptr->xx[indexMax].iSet = current;
      strcpy(hvptr->xx[indexMax].name,name);
      strcpy(hvptr->xx[indexMax].ip,ip);

      /*
      mm = sscanf (line,"%i %s %u     %f", &ii, ip, &chan, &volts);  // MPOD data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].slot = slot;
      strcpy(hvptr->xx[indexMax].ip,ip);
      */
      printf ("MPOD = %s %3u    %7.1f %i\n", hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan, hvptr->xx[indexMax].vSet,hvptr->xx[indexMax].type);
    }
    
/*
  Process CAEN 1527, 2527, ... HV supplies
*/    
    if (ii == 1) {
      mm = sscanf (line,"%s %i %u %u %14s %f %f %f %f %f %f %f %f %i %i", ip, &ii, &slot, &chan, name, &volts, &current, &ramp,
		   &dramp, &svmax, &v1set, &i1set, &trip, &itrip, &etrip);  // CAEN data

      //      mm = sscanf (line,"%i %s %u %u %f %f %f %14s", &ii, ip, &chan, &slot, &volts, &ramp, &current, name);  // CAEN data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].slot = slot;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].vRamp = ramp;
      hvptr->xx[indexMax].iSet = current;
      strcpy(hvptr->xx[indexMax].name,name);
      strcpy(hvptr->xx[indexMax].ip,ip);
      hvptr->xx[indexMax].downRamp = dramp;
      hvptr->xx[indexMax].vMax = svmax;
      hvptr->xx[indexMax].v1Set = v1set;
      hvptr->xx[indexMax].i1Set = i1set;
      hvptr->xx[indexMax].trip = trip;
      hvptr->xx[indexMax].intTrip = itrip;
      hvptr->xx[indexMax].extTrip = etrip;

      new = 0;
      for (kk=0; kk<maxIP; kk++) {                         // check if multiple CAEN units; new = a new IP found
	if (strcmp(ipsave[kk],ip) == 0) new = 1;              
      }
      if (new == 0) {
	strcpy(ipsave[maxIP],ip);      //...save and connect to get the handle: ceanH
      	printf("new IP: maxcaen = %i, IP = %s \n",maxIP,ipsave[maxIP]);
	maxIP++;
	//	hvptr->maxcaen++;
	caenCrate();
      }
      else hvptr->xx[indexMax].caenH = hvptr->xx[indexMax-1].caenH;   // copy previous caen Handle to this entry
    
      hvptr->caenCrate[hvptr->xx[indexMax].slot] += pow(2,hvptr->xx[indexMax].chan);    // record active chan in bit pattern

    printf ("CAEN = %s %3u %3u %7.1f %7.1f %7.1f %i %s \n",hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan,
	      hvptr->xx[indexMax].slot, hvptr->xx[indexMax].vSet, hvptr->xx[indexMax].vRamp, hvptr->xx[indexMax].iSet,
	      hvptr->xx[indexMax].type, hvptr->xx[indexMax].name);
    }
/*
    Reached the end of the configuration file data
*/
    if (ii == -1) {
      mm = fclose(ifile);
      if (mm != 0) printf("File not closed\n");
      break;
    }
    
    indexMax++;            // increment indexes 
  }   // end of while statement
  hvptr->maxchan = indexMax;
  printf ("%i HV entries found on MPODs and/or %i CAEN HV supplies\n",hvptr->maxchan, maxIP);
/*
  for (ii=0;ii<16;ii++){
    printf ("chan pattern by slot %i => %x => %i \n",ii,hvptr->caenCrate[ii],hvptr->caenCrate[ii] );
  }
  
  for (ii=0;ii<24;ii++){
    if (((hvptr->caenCrate[2] & (int)pow(2,ii)) >> ii)== 1) printf(" %i occupied\n",ii);
  }
*/  
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
void mpodHV() {
  int setget=0, ii=0;
  char cmd[150]="\0", cmdResult[140]="\0";

  printf (" Getting MPOD data ....");  
  for (ii=0; ii<indexMax; ii++){

    if (hvptr->xx[ii].type == 0) {
      sprintf(cmd,"outputVoltage.u%i",hvptr->xx[ii].chan); 
      snmp(setget,ii,cmd,cmdResult);     //mpodGETguru(cmd, cmdResult);     // read the set voltage
      hvptr->xx[ii].vSet = readFloat(cmdResult);

      sprintf(cmd,"outputMeasurementSenseVoltage.u%i",hvptr->xx[ii].chan);
      snmp(setget,ii,cmd,cmdResult);     //mpodGETguru(cmd, cmdResult);     // read the measured voltage
      hvptr->xx[ii].vMeas = readFloat(cmdResult);

      sprintf(cmd,"outputVoltageRiseRate.u%i",hvptr->xx[ii].chan);
      snmp(setget,ii,cmd,cmdResult);    //mpodGETguru(cmd, cmdResult);     // read the voltage ramp rate
      hvptr->xx[ii].vRamp = readFloat(cmdResult);

      sprintf(cmd,"outputMeasurementCurrent.u%i",hvptr->xx[ii].chan);
      snmp(setget,ii,cmd,cmdResult);   //mpodGETguru(cmd, cmdResult);     // read the measured current
      hvptr->xx[ii].iMeas = readFloat(cmdResult);

      sprintf(cmd,"outputCurrent.u%i",hvptr->xx[ii].chan);
      snmp(setget,ii,cmd,cmdResult);   //mpodGETguru(cmd, cmdResult);     // read the max current
      hvptr->xx[ii].iSet = readFloat(cmdResult);
    }
  }
  printf (".... finished \n");  
  return;
}

/******************************************************************************/
void snmp(int setget, int ii, char *cmd, char *cmdResult) {
  //  pid_t wait(int *stat_loc);
  FILE *fp;
  int status;
  char com[150];
  char res[200];
  res[0] = '\0';

/*
   setget flag chooses if this is a 
    0=read (snmpget) or
    1=write (snmpset) or 
    2-shutdown with MPOD MIB and IP from HV control
    
    snmpset -OqvU -v 2c -M /usr/share/snmp/mibs -m +WIENER-CRATE-MIB -c guru 192.168.13.231 outputSwitch.u7 i 0
*/
  if (setget == 1) {
     printf("ok.");
    //    sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,MPOD_IP);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
    //sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
    sprintf (com,"snmpset -OqvU -v 2c -M /usr/share/snmp/mibs -m +%s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
  } 
  else if (setget == 0){
    sprintf (com,"snmpget -v 2c -M /usr/share/snmp/mibs -m %s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);
    //sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,hvptr->xx[ii].ip);
    //    sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,MPOD_IP);
  }
  //  else if (setget == 2){              // if setget =2 then issue HV shutdown
  //}
  strcat(com,cmd);                             // add command to snmp system call
  //  printf("%s\n",com);

  fp = popen(com, "r");   // open child process - call to system; choosing this way

  if (fp == NULL){                             // so that we get the immediate results
    printf("<br/>Could not open to shell!\n");
    strncat(cmdResult,"popen error",39);
    return;
  }

  // process the results from the child process    
    while (fgets(res,200,fp) != NULL){
      fflush(fp);  
    } 

  // close the child process    
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

/******************************************************************************/
void caenCrate() {
  char *model, *descModel, *m, *d;
  unsigned short numOfSlots;
  unsigned short *numOfChan, *serNumList;
  unsigned char  *firmwareMin, *firmwareMax;
  int result=0, ii=0;

  result = CAENHV_InitSystem(SY1527, 0, hvptr->xx[indexMax].ip, "admin", "admin", &hvptr->xx[indexMax].caenH);   // only opens 1527 CAEN systems
  if (result != 0) printf("Error attaching to CAEN system %s \n",hvptr->xx[indexMax].ip);
  else {
    printf ("Successful login!...now map it \n");
    result = CAENHV_GetCrateMap(hvptr->xx[indexMax].caenH, &numOfSlots, &numOfChan, &model, &descModel, &serNumList, &firmwareMin, &firmwareMax);   // need to get the crate info in order to access
    if (result != 0) printf("Error mapping CAEN system %s \n",hvptr->xx[indexMax].ip);
    else {
      printf ("Successful Map\n\n");
      m = model;
      d = descModel;
      for (ii=0; ii<numOfSlots; ii++, m += strlen(m) + 1, d += strlen(d) + 1 ){
	if( *m == '\0' ){
	  printf("Board %2d: Not Present\n", ii);
	  hvptr->caenCrate[ii]=-1;    //  bits above 24 are also set so -1 is OK
	  hvptr->caenSlot[ii]=0;      //  number of channels
	}
	else {
	  printf("Board %2d: %s %s  Nr. Ch: %d  Ser. %d   Rel. %d.%d \n",
		    ii, m, d, numOfChan[ii], serNumList[ii], firmwareMax[ii],firmwareMin[ii]);
	  hvptr->caenCrate[ii]=0;           //  bits = 0 above 24 are also set so -1 is OK
	  hvptr->caenSlot[ii]=(unsigned int) numOfChan[ii];    //  number of channels
	  printf ("number of channels in slot %i = %i\n",ii,hvptr->caenSlot[ii]);
	}
      }
    }
    printf ("\n");
  }
  free(serNumList), free(model), free(descModel), free(firmwareMin), free(firmwareMax), free(numOfChan);

  return;
}

/******************************************************************************/
void caenGetBySlot() {
/*
   Get parameters from CAEN crates   
*/
  int ii=0, jj=0, result =0;
  unsigned short c24 = 24, slot=0;
  char V0Set[30]="V0Set\0";   //   V0Set  I0Set  IMon VMon
  char I0Set[30]="I0Set\0";   //   V0Set  I0Set  IMon VMon
  char VMon[30]="VMon\0";     //   V0Set  I0Set  IMon VMon
  char IMon[30]="IMon\0";     //   V0Set  I0Set  IMon VMon
  char Pw[30]="Pw\0";         //   PW
  float	         *floatVal  = NULL;
  unsigned int   *unsignVal = NULL;
  unsigned short *ChList;

  ChList =    malloc(c24*sizeof(unsigned short));
  floatVal =  malloc(c24*sizeof(float));               // if multiple channels use numChan instead of one
  unsignVal = malloc(c24*sizeof(unsigned int));      // if multiple channels use numChan instead of one

  //  printf (" Getting CAEN data ....");  
  for (ii=0; ii< c24; ii++){
    ChList[ii]=ii;
  }
  slot=5;
  ii = 10;
  //  for (ii=0; ii<indexMax; ii++){
  //    if (hvptr->xx[ii].type == 1) {
      /**/
  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, slot, VMon, c24, ChList, floatVal);
  if (result != 0) printf("Error getting VMON from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else {
    //      floatVal[0] = 0;
    for (ii=0; ii<c24; ii++){
      printf ("chan %u = %7.1f volts\n", ChList[ii],floatVal[ii]);
    }
  }
  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, slot, Pw, c24, ChList, unsignVal);
  if (result != 0) printf("Error getting VMON from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else {
    //      floatVal[0] = 0;
    for (ii=0; ii<c24; ii++){
      printf ("chan %u = %u volts\n", ChList[ii],unsignVal[ii]);
    }
  }

  for (ii=0; ii <hvptr->maxchan; ii++){
    if ( (hvptr->xx[ii].caenH == hvptr->xx[ii].caenH) && (hvptr->xx[ii].slot == slot)) {
      for (jj=0;jj<c24;jj++){
	if ((hvptr->xx[ii].chan == ChList[jj]) && (hvptr->xx[ii].onoff == 1)){
	  printf ("chan %u = %7.1f volts\n", ChList[jj],floatVal[jj]);
	  hvptr->xx[ii].vMeas = floatVal[jj];
	}
      }
    }
  }

  free(floatVal);  free(ChList);  free(unsignVal);
  printf (".... finished\n");  
  return;
}

/******************************************************************************/
void recoverCAEN() {
/*
   Get parameters from CAEN crates   
*/
  int ii=0, jj=0, kk=0, nn=0, type1=0, result=0, pp=0;
  int handle=0;
  char ipaddr[30];
  
  char V0Set[30]="V0Set\0";       //   V0Set  
  char I0Set[30]="I0Set\0";       //   I0Set  
  char VMon[30]="VMon\0";         //   VMon
  char IMon[30]="IMon\0";         //   IMon 
  char Pw[30]="Pw\0";             //   PW
  char V1Set[30]="V1Set\0";       //   V1Set
  char I1Set[30]="I1Set\0";       //   I1Set
  char SVMax[30]="SVMax\0";       //   V0Set  I0Set  IMon VMon
  char Trip[30]="Trip\0";         //   SVMax
  char RUp[30]="RUp\0";           //   RUp
  char RDWn[30]="RDWn\0";         //   RDWn
  char TripInt[30]="TripInt\0";   // TripInt
  char TripExt[30]="TripExt\0";   // TripExt
  struct hvchan yy[1000];

  float	         *floatVal  = NULL;
  unsigned int   *unsignVal = NULL;
  unsigned short *ChList;
  char  (*charVal)[MAX_CH_NAME];
  /*
  ChList =    malloc(c24*sizeof(unsigned short));
  floatVal =  malloc(c24*sizeof(float));               // if multiple channels use numChan instead of one
  unsignVal = malloc(c24*sizeof(unsigned long int));      // if multiple channels use numChan instead of one
*/
  printf (" Getting CAEN data ....\n ");  

  ii=0;
  while (++ii < 1000) {
    if (hvptr->xx[ii].type == 1) {
      //     printf (" Found start of CAEN data at ii=%i .... ",ii);  
      handle = hvptr->xx[ii].caenH;
      strcpy(ipaddr, hvptr->xx[ii].ip);
      type1=ii;
      break;
    }
  }

  hvptr->maxchan =0;             // 0 maxchan to reload it....but what about MPOD?
  nn=0;
  for (kk=0; kk<16; kk++){              // loop over slots getting data by module...ie., get info from all channels
    //    printf ("processing slot %i\n",kk);
    if (hvptr->caenSlot[kk] != 0) {     // will sort at end based on those that have onoff = 1.
      //     printf ("processing loaded slot %i\n",kk);
      ChList =    malloc(hvptr->caenSlot[kk]*sizeof(unsigned short));
      floatVal =  malloc(hvptr->caenSlot[kk]*sizeof(float));               // if multiple channels use numChan instead of one
      unsignVal = malloc(hvptr->caenSlot[kk]*sizeof(unsigned int));      // if multiple channels use numChan instead of one
      charVal =   malloc(hvptr->caenSlot[kk]*sizeof(MAX_CH_NAME));
  
      hvptr->caenCrate[kk] =0;                      // zero the channel bit pattern....reset based on Pw =1 below
      for (jj=0; jj< hvptr->caenSlot[kk]; jj++){    // zero the return data
	ChList[jj]=jj;
      }
      result = CAENHV_GetChParam(handle, kk, VMon, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting VMON from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].vMeas = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, V0Set, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting V0Set from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].vSet = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, I0Set, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting I0Set from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].iSet = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, I1Set, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting I1Set from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].i1Set = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, V1Set, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting V1Set from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].v1Set = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, SVMax, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting SVMax from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].vMax = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, RUp, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting vRamp from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].vRamp = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, RDWn, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting RDWn from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].downRamp = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, Trip, hvptr->caenSlot[kk], ChList, floatVal);
      if (result != 0) printf("Error getting Trip from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].trip = floatVal[ii-nn];
	}
      }
      result = CAENHV_GetChParam(handle, kk, Pw, hvptr->caenSlot[kk], ChList, unsignVal);
      if (result != 0) printf("Error getting PW (onoff) from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].onoff = (int) unsignVal[ii-nn];   // (float)val;           // get power PW as onoff
	  if ( yy[ii].onoff == 1) {
	    yy[ii].type = 1;                           // set type to caen crate
	    hvptr->caenCrate[kk] += pow(2,ii-nn);      // load channel bit pattern
	    yy[ii].chan=ii-nn;                         // load crannel
	    yy[ii].slot=kk;                            // load slot
	    yy[ii].caenH=handle;                       // load handle
	    strcpy(yy[ii].ip,ipaddr);                  // load ip address
	    pp++;
	    //	    printf("last pp = %i\n",pp);
	  }
	}
      }
      result = CAENHV_GetChParam(handle, kk, TripInt, hvptr->caenSlot[kk], ChList, unsignVal);
      if (result != 0) printf("Error getting TripInt from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].intTrip = (int) unsignVal[ii-nn];   // (float)val;           // get power PW as onoff
	}
      }
      result = CAENHV_GetChParam(handle, kk, TripExt, hvptr->caenSlot[kk], ChList, unsignVal);
      if (result != 0) printf("Error getting TripExt from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  yy[ii].extTrip = (int) unsignVal[ii-nn];   // (float)val;           // get power PW as onoff
	}
      }
      /*
      result = CAENHV_GetChName(handle, kk, hvptr->caenSlot[kk], ChList, charVal);
      if (result != 0) printf("Error getting Name from CAEN system %s handle %i\n",ipaddr,handle);
      else {
	printf("Name string length %li\n",strlen(charVal[ii-nn]));
	for (ii=nn; ii < nn+hvptr->caenSlot[kk]; ii++){
	  //	  strcpy(hvptr->xx[ii].name,charVal[ii]);   // (float)val;           // get power PW as onoff
	  strcpy(yy[ii].name,charVal[ii-nn]);   // (float)val;           // get power PW as onoff
	}
      }
      */
      nn += hvptr->caenSlot[kk];
      //      printf("nn = %i  pp = %i\n",nn,pp);
      free(floatVal);  free(ChList);  free(unsignVal); free(charVal);
    }
  }
/*
  Have loaded all information into the arrays; now consolodate to maxchan.
*/
  kk = type1;                  // first CAEN module
  //  printf("last nn = %i\n",nn);
  for (ii=0; ii<nn; ii++) {
    if (yy[ii].onoff == 1) {                         // scroll through those found with HV on
      hvptr->xx[kk++] = yy[ii];                      // copy into the CAEN data AFTER the MPOD
      //      printf("processing channel with HV on %i\n",kk);
    }
  }
  hvptr->maxchan = kk;     // record maxchan
  indexMax = hvptr->maxchan;
  
  printf (".... finished...found %i CAEN channels that are on\n", hvptr->maxchan-type1);  
  return;
}
	
/******************************************************************************/
void caenGet() {
/*
   Get parameters from CAEN crates   
*/
  int ii=0, result =0, one = 1;
  char V0Set[30]="V0Set\0";   //   V0Set  I0Set  IMon VMon
  char I0Set[30]="I0Set\0";   //   V0Set  I0Set  IMon VMon
  char VMon[30]="VMon\0";     //   V0Set  I0Set  IMon VMon
  char IMon[30]="IMon\0";     //   V0Set  I0Set  IMon VMon
  char Pw[30]="Pw\0";         //   PW
  char V1Set[30]="V1Set\0";   //   V0Set  I0Set  IMon VMon
  char I1Set[30]="I1Set\0";   //   V0Set  I0Set  IMon VMon
  char SVMax[30]="SVMax\0";   //   V0Set  I0Set  IMon VMon
  char Trip[30]="Trip\0";   //   V0Set  I0Set  IMon VMon
  char RUp[30]="RUp\0";   //   V0Set  I0Set  IMon VMon
  char RDWn[30]="RDWn\0";   //   V0Set  I0Set  IMon VMon
  char TripInt[30]="TripInt\0";   //   V0Set  I0Set  IMon VMon
  char TripExt[30]="TripExt\0";   //   V0Set  I0Set  IMon VMon

  float	         *floatVal  = NULL;
  unsigned int   *unsignVal = NULL;
  unsigned short *ChList;
  char  (*charVal)[MAX_CH_NAME];

  
  ChList =    malloc(sizeof(unsigned short));
  floatVal =  malloc(sizeof(float));               // if multiple channels use numChan instead of one
  unsignVal = malloc(sizeof(unsigned int));      // if multiple channels use numChan instead of one
  charVal =   malloc(sizeof(MAX_CH_NAME));
  
  printf (" Getting CAEN data ....");  

  for (ii=0; ii<indexMax; ii++){
    if (hvptr->xx[ii].type == 1) {
      /**/

      getVMon(ii);
      getIMon(ii);
/*     
      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, VMon, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting VMON from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vMeas = floatVal[0];   // (float)val;           // measured voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, IMon, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting IMon from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iMeas = floatVal[0];   // (float)val;           // measured current
      floatVal[0] = 0;
*/
      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V0Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting V0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vSet = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I0Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting I0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iSet = floatVal[0];   // (float)val;           // set max current
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I1Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting I1Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].i1Set = floatVal[0];   // (float)val;           // set max current
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V1Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting V1Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].v1Set = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, SVMax, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting SVMax from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vMax = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RUp, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting vRamp from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vRamp = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RDWn, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting RDWn from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].downRamp = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Trip, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting Trip from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].trip = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      /*      */
      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, unsignVal);
      if (result != 0) printf("Error getting PW (onoff) from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].onoff = (int) unsignVal[0];   // (float)val;           // get power PW as onoff
      unsignVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripInt, one, &hvptr->xx[ii].chan, unsignVal);
      if (result != 0) printf("Error getting TripInt from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].intTrip = (int) unsignVal[0];   // (float)val;           // get power PW as onoff
      unsignVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripExt, one, &hvptr->xx[ii].chan, unsignVal);
      if (result != 0) printf("Error getting TripExt from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].extTrip = (int) unsignVal[0];   // (float)val;           // get power PW as onoff
      unsignVal[0] = 0;

      result = CAENHV_GetChName(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, one, &hvptr->xx[ii].chan, charVal);
      if (result != 0) printf("Error getting TripExt from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else strcpy(hvptr->xx[ii].name,charVal[0]);   // (float)val;           // get power PW as onoff
      strcpy(charVal[0],"\0");

    }
  }
  free(floatVal); free(ChList); free(unsignVal); free(charVal);
  printf (".... finished\n");  
  return;
}
/******************************************************************************/
void caenSet() {
/*
   Get parameters from CAEN crates   
*/
  int ii=0, result =0, one = 1;
  char V0Set[30]="V0Set\0";   //   V0Set  I0Set  IMon VMon
  char I0Set[30]="I0Set\0";   //   V0Set  I0Set  IMon VMon
  char VMon[30]="VMon\0";     //   V0Set  I0Set  IMon VMon
  char IMon[30]="IMon\0";     //   V0Set  I0Set  IMon VMon
  char Pw[30]="Pw\0";         //   PW
  float	         *floatVal  = NULL;
  unsigned short *unsignVal = NULL;
  unsigned short *ChList;

  ChList =    malloc(sizeof(unsigned short));
  floatVal =  malloc(sizeof(float));               // if multiple channels use numChan instead of one
  unsignVal = malloc(sizeof(unsigned long int));      // if multiple channels use numChan instead of one

  //    for (ii=0; ii<indexMax; ii++){
      //for (ii=0; ii<5; ii++){
  if (hvptr->xx[15].type == 1) {
    /*
      result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, VMon, one, &hvptr->xx[ii].chan, &floatVal);
      if (result != 0) printf("Error setting VMON from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vMeas = floatVal[0];   // (float)val;           // measured voltage
      floatVal[0] = 0;

      result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, IMon, one, &hvptr->xx[ii].chan, &floatVal);
      if (result != 0) printf("Error setting IMon from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iMeas = floatVal[0];   // (float)val;           // measured current
      floatVal[0] = 0;

      result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V0Set, one, &hvptr->xx[ii].chan, &floatVal);
      if (result != 0) printf("Error setting V0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vSet = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I0Set, one, &hvptr->xx[ii].chan, &floatVal);
      if (result != 0) printf("Error setting I0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iSet = floatVal[0];   // (float)val;           // set max current
      floatVal[0] = 0;

	unsignVal[0] = hvptr->xx[ii].onoff;
      result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, &unsignVal);
      if (result != 0) printf("Error setting PW (onoff)for %s from CAEN system %s handle %i\n",hvptr->xx[ii].name,hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else printf("Error setting PW\n"); 
      unsignVal[0] = 0;
    */
      
      unsignVal[0] = hvptr->xx[15].onoff;
      result = CAENHV_SetChParam(hvptr->xx[15].caenH, hvptr->xx[15].slot, Pw, one, &hvptr->xx[15].chan, &unsignVal);
      if (result != 0) printf("Error = 0x%x setting PW (onoff) for %s from CAEN system %s handle %i\n",
			      result,hvptr->xx[15].name,hvptr->xx[15].ip,hvptr->xx[15].caenH);
      else printf("Successful setting PW\n"); 
      unsignVal[0] = 0;

    }
 
  free(floatVal); free(ChList), free(unsignVal);
  return;
}

/******************************************************************************/
void hvOnAll(){
  int ii=0, result =0, one = 1;
  char Pw[30]="Pw\0";                 //   PW
  unsigned int *unsignVal = NULL;

  unsignVal = malloc(sizeof(unsigned int));      // if multiple channels use numChan instead of one

  for (ii=0; ii<hvptr->maxchan; ii++){
    unsignVal[0] = 1;
    result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, &unsignVal);
    if (result != 0) printf("Error = 0x%x setting PW (onoff) for %s from CAEN system %s handle %i\n",
			    result,hvptr->xx[ii].name,hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
    else {
      printf("Successful setting PW %i \n", ii); 
      hvptr->xx[ii].onoff = 1;
    }
  }
  
  free(unsignVal);
  return;
}

/******************************************************************************/
void hvOffAll(){
  int ii=0, result =0, one = 1;
  char Pw[30]="Pw\0";                 //   PW
  unsigned int *unsignVal = NULL;

  unsignVal = malloc(sizeof(unsigned int));      // if multiple channels use numChan instead of one

  for (ii=0; ii<hvptr->maxchan; ii++){
    unsignVal[0] = 0;
    result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, &unsignVal);
    if (result != 0) printf("Error = 0x%x setting PW (onoff) for %s from CAEN system %s handle %i\n",
			    result,hvptr->xx[ii].name,hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
    else {
      printf("Successful setting PW %i \n", ii); 
      hvptr->xx[ii].onoff = 0;
    }
  }
  
  free(unsignVal);
  return;
}

/******************************************************************************/
void getTemp(){
  int result=0, one=1;
  int handle=0;
  short unsigned int ii=0;
  char Temp[30]="Temp\0";                 //   Temperature
  unsigned int   *unsignVal = NULL;
  unsigned short *slotList;
  float	         *floatVal  = NULL;
  
  slotList  = malloc(sizeof(unsigned short));
  unsignVal = malloc(sizeof(unsigned int));      // if multiple channels use numChan instead of one
  floatVal = malloc(sizeof(float));      // if multiple channels use numChan instead of one
  ii=-1;
  while (++ii < 1000) {
    if (hvptr->xx[ii].type == 1) {
      handle = hvptr->xx[ii].caenH;
      break;
    }
  }
  
  for (ii=0; ii<16; ii++){
    if (hvptr->caenSlot[ii] > 0){
      result = CAENHV_GetBdParam(handle, one, &ii, Temp, floatVal);
      if (result != 0) printf("Error = %i (0x%x) getting temperature \n", result,result);
      else {
	//	printf("Successful getting temp %i ...", ii);
	hvptr->caenTemp[ii] = floatVal[0];
	//	printf("   %2.1f  \n", hvptr->caenTemp[ii]);
      }
    }
  }
  free(unsignVal); free(slotList); free(floatVal);
  
  return;
}

/******************************************************************************/
void changeParam(){
  int ii = 0;
  /*
 control variables:
  com0 = 16        selects this function
  com1 = detector  selects detector to change
  com2 = parameter selects the parameter to change.....
  hvptr->xx[hvptr->com1].parameter contains the new parameter
   */

  ii = hvptr->com1;
  switch (hvptr->com2){

  case 1:
    setV0Set(ii);
    break;

  case 2:
    setI0Set(ii);
    break;

  case 3:
    setPw(ii);
    break;

  case 4:
    setRUp(ii);
    break;

  case 5:
    setV1Set(ii);
    break;

  case 6:
    setI1Set(ii);
    break;

  case 7:
    setSVMax(ii);
    break;

  case 8:
    setRDWn(ii);
    break;

  case 9:
    setTrip(ii);
    break;

  case 10:
    setTripInt(ii);
    break;

   case 11:
    setTripExt(ii);
    break;
  
  default:
    break;
  }
 

  return;
}
/******************************************************************************/
void getVMon(int ii){
  char VMon[30]="VMon\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, VMon, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting VMon for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].vMeas = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void getIMon(int ii){
  char IMon[30]="IMon\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, IMon, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting IMon for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].iMeas = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void getV0Set(int ii){
  char V0Set[30]="V0Set\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V0Set, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting V0Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].vSet = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setV0Set(int ii){
  char V0Set[30]="V0Set\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].vSet == hvptr->xcom3) return;
  
  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V0Set, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting V0Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set voltage
  free (floatVal);

  return;
}

/******************************************************************************/
void getI0Set(int ii){
  char I0Set[30]="I0Set\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I0Set, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting I0Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].iSet = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setI0Set(int ii){
  char I0Set[30]="I0Set\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].iSet == hvptr->xcom3) return;
  
  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I0Set, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting I0Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}

/******************************************************************************/
/******************************************************************************/
/*
char V0Set[30]="V0Set\0";       //   V0Set  
  char I0Set[30]="I0Set\0";       //   I0Set  
  char VMon[30]="VMon\0";         //   VMon
  char IMon[30]="IMon\0";         //   IMon 
  char Pw[30]="Pw\0";             //   PW
  char V1Set[30]="V1Set\0";       //   V1Set
  char I1Set[30]="I1Set\0";       //   I1Set
  char SVMax[30]="SVMax\0";       //   V0Set  I0Set  IMon VMon
  char Trip[30]="Trip\0";         //   SVMax
  char RUp[30]="RUp\0";           //   RUp
  char RDWn[30]="RDWn\0";         //   RDWn
  char TripInt[30]="TripInt\0";   // TripInt
  char TripExt[30]="TripExt\0";   // TripExt
*/

/******************************************************************************/
/******************************************************************************/
void getV1Set(int ii){
  char V1Set[30]="V1Set\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V1Set, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting V1Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].v1Set = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setV1Set(int ii){
  char V1Set[30]="V1Set\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].v1Set == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V1Set, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting V1Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}
/******************************************************************************/
void getI1Set(int ii){
  char I1Set[30]="I1Set\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I1Set, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting I1Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].i1Set = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setI1Set(int ii){
  char I1Set[30]="I1Set\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].i1Set == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I1Set, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting I1Set for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}
/******************************************************************************/
void getSVMax(int ii){
  char SVMax[30]="SVMax\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, SVMax, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting SVMax for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].vMax = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setSVMax(int ii){
  char SVMax[30]="SVMax\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].vMax == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, SVMax, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting SVMax for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}

/******************************************************************************/
void getRUp(int ii){
  char RUp[30]="RUp\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RUp, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting RUp for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].vRamp = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setRUp(int ii){
  char RUp[30]="RUp\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].vRamp == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RUp, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting RUp for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}

/******************************************************************************/
void getRDWn(int ii){
  char RDWn[30]="RDWn\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RDWn, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting RDWn for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].downRamp = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setRDWn(int ii){
  char RDWn[30]="RDWn\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].downRamp == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, RDWn, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting RDWn for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}

/******************************************************************************/
void getTrip(int ii){
  char Trip[30]="Trip\0";       //   V0Set  
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Trip, one, &hvptr->xx[ii].chan, floatVal);
  if (result != 0) printf("Error getting Trip for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].trip = floatVal[0];   // (float)val;           // set voltage

  free (floatVal);

  return;
}

/******************************************************************************/
void setTrip(int ii){
  char Trip[30]="Trip\0";       //   I0Set
  int one=1;
  int result=0;
  float	         *floatVal  = NULL;

  if (hvptr->xx[ii].trip == hvptr->xcom3) return;

  floatVal =  malloc(one * sizeof(float));               // if multiple channels use numChan instead of one
  floatVal[0] = hvptr->xcom3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Trip, one, &hvptr->xx[ii].chan, &floatVal);
  if (result != 0) printf("Error setting Trip for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set current
  free (floatVal);

  return;
}

/******************************************************************************/
void getTripInt(int ii){
  char TripInt[30]="TripInt\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripInt, one, &hvptr->xx[ii].chan, unsignVal);
  if (result != 0) printf("Error setting TripInt for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].intTrip = unsignVal[0];   // (float)val;           // set voltage
  free (unsignVal);

  return;
}

/******************************************************************************/
void setTripInt(int ii){
  char TripInt[30]="TripInt\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  if (hvptr->xx[ii].intTrip == hvptr->com3) return;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one
  unsignVal[0] = hvptr->com3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripInt, one, &hvptr->xx[ii].chan, &unsignVal);
  if (result != 0) printf("Error setting TripInt for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set internal trip
  free (unsignVal);

  return;
}

/******************************************************************************/
void getTripExt(int ii){
  char TripExt[30]="TripExt\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripExt, one, &hvptr->xx[ii].chan, unsignVal);
  if (result != 0) printf("Error setting TripExt for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].extTrip = unsignVal[0];   // (float)val;           // set voltage
  free (unsignVal);

  return;
}

/******************************************************************************/
void setTripExt(int ii){
  char TripExt[30]="TripExt\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  if (hvptr->xx[ii].extTrip == hvptr->com3) return;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one
  unsignVal[0] = hvptr->com3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, TripExt, one, &hvptr->xx[ii].chan, &unsignVal);
  if (result != 0) printf("Error setting TripExt for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set internal trip
  free (unsignVal);

  return;
}

/******************************************************************************/
void getPw(int ii){
  char Pw[30]="Pw\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one

  result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, unsignVal);
  if (result != 0) printf("Error setting Pw for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else hvptr->xx[ii].onoff = unsignVal[0];   // (float)val;           // set voltage
  free (unsignVal);

  return;
}

/******************************************************************************/
void setPw(int ii){
  char Pw[30]="Pw\0";       //   I0Set
  int one=1;
  int result=0;
  unsigned int   *unsignVal = NULL;

  if (hvptr->xx[ii].onoff == hvptr->com3) return;

  unsignVal =  malloc(one * sizeof(unsigned int));               // if multiple channels use numChan instead of one
  unsignVal[0] = hvptr->com3;
  result = CAENHV_SetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, Pw, one, &hvptr->xx[ii].chan, &unsignVal);
  if (result != 0) printf("Error setting HV ON/OFF for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else printf("Success\n");            // set internal trip
  free (unsignVal);

  return;
}

/******************************************************************************/
void getName(int ii){
  int one=1;
  int result=0;
  char  (*charVal)[MAX_CH_NAME];

  charVal =   malloc(sizeof(MAX_CH_NAME));

  result = CAENHV_GetChName(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, one, &hvptr->xx[ii].chan, charVal);
  if (result != 0) printf("Error getting NAME for det %i from CAEN system %s handle %i\n",ii, hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
  else strcpy(hvptr->xx[ii].name,charVal[0]);   // (float)val;           // get power PW as onoff

  free (charVal);

  return;
}

/******************************************************************************/
