/*
  Program hvmon to monitor detector High Voltage
  Understands Wiener MPOD and CAEN x527

  To be compiled with:                                           
     gcc -Wall -lm -lcaenhvwrapper -o hvmon hvmon-v0.c

*/

#include "../include/hvmon.h"
#include "../include/CAENHVWrapper.h"

#define MPOD_MIB     "../include/WIENER-CRATE-MIB.txt"
#define CONFIGFILE   "../include/hvmon.conf"
#define INTERVAL 60

void readConf();                  // processes configuration file
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
void caenHV();
void caenCrate();

long int time0=0, time1=0, nread;
int indexMax=0;

// for CAEN crate mapping
char *model, *descModel;
unsigned short numOfSlots;
unsigned short *numOfChan, *serNumList;
unsigned char  *firmwareMin, *firmwareMax;
//char *model=NULL, *descModel=NULL;
//unsigned short numOfSlots=NULL;
//unsigned short *numOfChan=NULL, *serNumList=NULL;
//unsigned char  *firmwareMin=NULL, *firmwareMax=NULL;

/***********************************************************/
int main(int argc, char **argv){
/*

*/
  int ii=0, result=0;
  //  long int p0=0, p1=1, count=0, etime=0;
  int mapHVmon;
  pid_t pid;

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
  readConf();
/*  
  Setup time of next fill based on current time and configure file
*/
  time0 = time(NULL);            // record starting time of program
  time1 = time0 + INTERVAL;      // set up next read of voltages...usually every minute
/*  
  Setup monitoring loop to look for changes/minute and requests   
*/
  nread = INTERVAL;
  hvptr->com0 = 0;

  while(hvptr->com0 != -1) {
    //    for (ii=0; ii<indexMax; ii++){
    //      if (hvptr->xx[ii].type == 0) {   
	//	printf ("mon  MPOD  %i \n",ii);
    mpodHV();
/*    
    char	*m = model, *d = descModel;
    for (ii=0; ii<numOfSlots;ii++, m += strlen(m) + 1, d += strlen(d) + 1 ){
      if( *m == '\0' )
	printf("Board %2d: Not Present\n", ii);
      else             
	printf("Board %2d: %s %s  Nr. Ch: %d  Ser. %d   Rel. %d.%d \n",
	       ii, m, d, numOfChan[ii], serNumList[ii], firmwareMax[ii],firmwareMin[ii]);
			//		if( !loop ) con_getch(); 
      
    }
    free(serNumList), free(model), free(descModel), free(firmwareMin), free(firmwareMax), free(numOfChan);
*/
    caenHV();

    for (ii=0; ii<indexMax; ii++){
      printf("i = %i, VSet = %f,  Vred = %f, Vramp = %f, Ired = %f Imax = %f \n",ii, hvptr->xx[ii].vSet, hvptr->xx[ii].vMeas, hvptr->xx[ii].vRamp, hvptr->xx[ii].iMeas, hvptr->xx[ii].iSet);
    }
    hvptr->com0 = -1;
  }

  // CLOSE THE CAEN SYSTEMS HERE!!!!
  if (result != 0) printf("Error deattaching to CAEN system %s \n",hvptr->xx[indexMax].ip);
  else printf ("Successful logout!\n");
  
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
  int ii=0, mm=0, slot=0, chan=0;
  float volts=0.0;
  char ip[30]="\0";
  char ipsave[10][30];
  int kk=0,maxIP=0, new=0, result=0;
  // for CAEN crate mapping
  //  char **model=NULL, **descModel=NULL;
  //  unsigned short *numOfSlots=NULL;
  //  unsigned short **numOfChan=NULL, **serNumList=NULL;
  //  unsigned char	**firmwareMin=NULL, **firmwareMax=NULL;
  
  //  char lnfill_conf[200]="/Users/c4g/src/LNfill/include/lnfill.conf";   //see define statement in lnfill.h 
  /*
  long int ii=0;
  int onoff=0, chanRTD=0, chanOFLO=0, Ibar=0, mani=0;
  int u1=-1, u2=-1, u3=-1, u4=-1;
  int mm=0;
  char name[10]="\0";
  double interval=0.0, max=0.0, min=0.0, limit=0.0, olimit=0.0;
  */
  //  model = NULL;
  
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
    chan = -1;
    slot = -1;
    volts = 0.0;
/*
  Process MPOD and other SNMP data
*/    
    if (ii == 0) {
      mm = sscanf (line,"%i %s %u     %f", &ii, ip, &chan, &volts);  // MPOD data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].vSet = volts;
      hvptr->xx[indexMax].slot = slot;
      strcpy(hvptr->xx[indexMax].ip,ip);
      printf ("MPOD = %s %3u    %7.1f %i\n", hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan, hvptr->xx[indexMax].vSet,hvptr->xx[indexMax].type);
    } 
/*
  Process CAEN 1527, 2527, ... HV supplies
*/    
    if (ii == 1) {
      mm = sscanf (line,"%i %s %u %u %f", &ii, ip, &chan, &slot, &volts);  // CAEN data
      hvptr->xx[indexMax].type = ii;
      hvptr->xx[indexMax].chan = chan;
      hvptr->xx[indexMax].slot = slot;
      hvptr->xx[indexMax].vSet = volts;
      strcpy(hvptr->xx[indexMax].ip,ip);
      new = 0;
      for (kk=0; kk<maxIP; kk++) {                         // check if multiple CAEN units; new = a new IP found
	if (strcmp(ipsave[kk],ip) == 0) new = 1;              
      }
      if (new == 0) {
	strcpy(ipsave[maxIP],ip);      //...save and connect to get the handle: ceanH
	printf("new IP: maxIP = %i, IP = %s \n",maxIP,ipsave[maxIP]);
	maxIP++;
	caenCrate();
/*
	result = CAENHV_InitSystem(SY1527, 0, ip, "admin", "admin", &hvptr->xx[indexMax].caenH);   // only opens 1527 CAEN systems
	if (result != 0) printf("Error attaching to CAEN system %s \n",hvptr->xx[indexMax].ip);
	else {
	  printf ("Successful login!...now map it \n");
	  result = CAENHV_GetCrateMap(hvptr->xx[indexMax].caenH, &numOfSlots, &numOfChan, &model, &descModel, &serNumList, &firmwareMin, &firmwareMax);   // need to get the crate info in order to access
	  if (result != 0) printf("Error mapping CAEN system %s \n",hvptr->xx[indexMax].ip);
	  else printf ("Successful Map\n");
	}
*/
      }
      else hvptr->xx[indexMax].caenH = hvptr->xx[indexMax-1].caenH;   // copy previous caen Handle to this entry
    
      printf ("CAEN = %s %3u %3u %7.1f %i \n",hvptr->xx[indexMax].ip, hvptr->xx[indexMax].chan, hvptr->xx[indexMax].slot, hvptr->xx[indexMax].vSet,hvptr->xx[indexMax].type);
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
  
  printf ("%i HV entries found on MPODs and/or %i CAEN HV supplies\n",indexMax, maxIP);
  
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
*/
  if (setget == 1) {
    //    sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,MPOD_IP);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
    sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
  } 
  else if (setget == 0){
    sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,hvptr->xx[ii].ip);
    //    sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,MPOD_IP);
  }
  //  else if (setget == 2){              // if setget =2 then issue HV shutdown
  //}
  strcat(com,cmd);                             // add command to snmp system call
  printf("%s\n",com);

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
void caenHV() {
/*
   
*/
  int ii=0, result =0, one = 1;
  //  float val=0.0;
  //  unsigned long int tipo=0;
  //  char parName[30]="VMon\0";  //   V0Set  I0Set  IMon VMon
  char V0Set[30]="V0Set\0";   //   V0Set  I0Set  IMon VMon
  char IOSet[30]="IOSet\0";   //   V0Set  I0Set  IMon VMon
  char VMon[30]="VMon\0";     //   V0Set  I0Set  IMon VMon
  char IMon[30]="IMon\0";     //   V0Set  I0Set  IMon VMon
  char PW[30]="PW\0";         //   PW
  float	         *floatVal  = NULL;
  unsigned short *unsignVal = NULL;
  unsigned short *ChList;
  //  void * val;

  ChList =    malloc(sizeof(unsigned short));
  floatVal =  malloc(sizeof(float));               // if multiple channels use numChan instead of one
  unsignVal = malloc(sizeof(unsigned short));      // if multiple channels use numChan instead of one

  for (ii=0; ii<indexMax; ii++){
  //  for (ii=0; ii<5; ii++){
    if (hvptr->xx[ii].type == 1) {

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, VMon, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting VMON from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vMeas = floatVal[0];   // (float)val;           // measured voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, IMon, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting IMon from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iMeas = floatVal[0];   // (float)val;           // measured current
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, V0Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting V0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].vSet = floatVal[0];   // (float)val;           // set voltage
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, I0Set, one, &hvptr->xx[ii].chan, floatVal);
      if (result != 0) printf("Error getting I0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].iSet = floatVal[0];   // (float)val;           // set max current
      floatVal[0] = 0;

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, PW, one, &hvptr->xx[ii].chan, unsignVal);
      if (result != 0) printf("Error getting I0Set from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);
      else hvptr->xx[ii].onoff = unsignVal[0];   // (float)val;           // get power PW as onoff
      unsignVal[0] = 0;


    }
  }
  free(floatVal); free(ChList), free(unsignVal);
  return;
}

/******************************************************************************/
      //      printf (" %u   %u  %s  \n",  hvptr->xx[ii].slot, ChList[0], parName);
      //      result = CAENHV_GetChParamProp(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, ChList[0], parName, "Type", &tipo);
      //      if (result != 0) printf("Error getting Parameter property from CAEN system %s handle %i\n",hvptr->xx[ii].ip,hvptr->xx[ii].caenH);

      //      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, parName, one, &hvptr->xx[ii].chan, fParValList);//val);//fParValList);

      /*
      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot,"IMON", 1, &hvptr->xx[ii].chan, (void *)&val);
      if (result != 0) printf("Error getting IMON from CAEN system %s \n",hvptr->xx[indexMax].ip);
      else hvptr->xx[ii].iMeas = val;           // measured current

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot,"VSET", 1, &hvptr->xx[ii].chan, (void *)&val);
      if (result != 0) printf("Error getting VSET from CAEN system %s \n",hvptr->xx[indexMax].ip);
      else hvptr->xx[ii].vSet = val;            // set voltage

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot,"ISET", 1, &hvptr->xx[ii].chan, (void *)&val);
      if (result != 0) printf("Error getting ISET from CAEN system %s \n",hvptr->xx[indexMax].ip);
      else hvptr->xx[ii].iSet = val;            // set maximum current

      result = CAENHV_GetChParam(hvptr->xx[ii].caenH, hvptr->xx[ii].slot, "RAMP-UP", 1, &hvptr->xx[ii].chan, (void *)&val);
      if (result != 0) printf("Error getting RUP from CAEN system %s \n",hvptr->xx[indexMax].ip);
      else hvptr->xx[ii].vRamp = val;           // set voltage ramp up 
      */




/******************************************************************************/
void caenCrate() {
// for CAEN crate mapping
  char *model, *descModel;
  char *m = model, *d = descModel;
  unsigned short numOfSlots;
  unsigned short *numOfChan, *serNumList;
  unsigned char  *firmwareMin, *firmwareMax;

  result = CAENHV_InitSystem(SY1527, 0, hvptr->xx[indexMax].ip, "admin", "admin", &hvptr->xx[indexMax].caenH);   // only opens 1527 CAEN systems
  if (result != 0) printf("Error attaching to CAEN system %s \n",hvptr->xx[indexMax].ip);
  else {
    printf ("Successful login!...now map it \n");
    result = CAENHV_GetCrateMap(hvptr->xx[indexMax].caenH, &numOfSlots, &numOfChan, &model, &descModel, &serNumList, &firmwareMin, &firmwareMax);   // need to get the crate info in order to access
    if (result != 0) printf("Error mapping CAEN system %s \n",hvptr->xx[indexMax].ip);
    else {
      printf ("Successful Map\n");
      //  }
      for (ii=0; ii<numOfSlots;ii++, m += strlen(m) + 1, d += strlen(d) + 1 ){
	if( *m == '\0' ) printf("Board %2d: Not Present\n", ii);
	else printf("Board %2d: %s %s  Nr. Ch: %d  Ser. %d   Rel. %d.%d \n", ii, m, d, numOfChan[ii], serNumList[ii], firmwareMax[ii],firmwareMin[ii]);
      }
    }
  }
    free(serNumList), free(model), free(descModel), free(firmwareMin), free(firmwareMax), free(numOfChan);

  return;
}

/******************************************************************************/


/******************************************************************************/


/******************************************************************************/
