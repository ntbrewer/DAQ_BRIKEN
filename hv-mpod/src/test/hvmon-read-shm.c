/*
  Program hvmon to monitor detector High Voltage
  Understands Wiener MPOD

  To be compiled with:                                           
     gcc -Wall -lpthread -ldl -lm -o hvmon hvmon.mpod.c
     gcc -Wall -lm -o hvmon-read hvmon-read.c
*/
#include "../include/u3.h"
//#include "../include/labjackusb.h"
#include "../include/hvmon-shm.h"
#include "../include/kelvin-shm.h"

//int
// mmapSetup();                  // sets up the memory map
void shmSetup();
void menu();
void menu1();
int scan2int (); 
void printOut();
void fprintOut();
void printActive();
void printInactive();
void printAvailable();
void clone();
void snmp(int setget, int ii, char *cmd, char *cmdResult);
int readOnOff(char *cmdResult);
void mpodOnOff(int nf);
void setOnOff(int ii);
void ResetChan(int ii);
void ResetAllChan();
//void setVolt();
//void hvOffAll();
void saveSetup();
void openSaveFile();
char *GetDate();
void detParam();

FILE *fileSave;
 
/***********************************************************/
int main(int argc, char **argv){
/*

*/
  //int ii=0;// jj=0;
  int ans=0;
//  int mapHVmon;
  //pid_t pid;
  /*
  printf("Working directory: %s\n",getcwd(path,PATH_MAX+1));
  strcpy(mpod_mib,path);                  // copy path to mpod mib file
  strcat(mpod_mib,"/");                   // add back the ending directory slash
  strcat(mpod_mib,MPOD_MIB);              // tack on the mib file location
  printf("    mpod_mib file: %s\n",mpod_mib);
  */
/*
  Memory map creation and attachment
  the segment number is stored in hvptr->pid
*/
//  mapHVmon = mmapSetup();
//  if (mapHVmon == -1) return 0;
  shmSetup();
/*
  Scroll through options
*/
  while (ans != 100){
    menu1();
    //    scanf ("%i", &ans);          // read in ans (pointer is indicated)     
    ans = scan2int ();
    
    switch (ans){
    case 0:                    // end program but not kelvin
      ans = 100;
      break;
    case 1:                      // display temps and limits do not communicate to kelvin
      printOut();
      break;

    case 2:                      // display temps and limits
      printOut();
      hvptr->com0 = 2;           // command (3) stored in SHM so  hvmon can do something
      printOut();
      break;

    case 3:                      // display temps and limits
      printOut();
      hvptr->com0 = 3;            // command (3) stored in SHM so hvmon can do something
      printOut();
      break;

    case 4:                      // display temps and limits
      printf("HV on...\n");
      hvptr->com0 = 4;
      kill(hvptr->pid,SIGALRM); //send to hvmon for action
      /*char cmdResult[140]="\0";
      int onOff=100;
      snmp(0,0,"outputSwitch.u300",cmdResult);
      onOff = readOnOff(cmdResult);
      printf("%s :cmd, %i : onOFF", cmdResult, onOff);
      */
      //mpodOnOff(1);

      break;

    case 5:                      // display temps and limits
      printf("HV off...\n");
      //mpodOnOff(0);
      hvptr->com0 = 5;
      kill(hvptr->pid,SIGALRM); //send to hvmon for action
      break;
    case 6:                      // Save Config File
      printf("Saving file...\n");
      openSaveFile();
      fprintOut();
      fclose(fileSave);
      printf("file is saved ...\n");
      break;

    case 7:
       printf("N/A\n");
      //hvptr->com0 = 7;           // command (3) stored in SHM so kelvin can do something
      //kill(hvptr->pid,SIGALRM);  // send an alarm to let kelvin know it needs to do command (1)
      //printf("Recover setup from hardware ...\n");
      //sleep(1);
      break;

    case 8:                      // display temps and limits
      printActive();
      break;
    case 9:
      printInactive();
      break;
    case 11:                      // display temps and limits
      printf("Clone channel parameters...\n");
      clone();
      break;

/*    case 14:                      // display temps and limits
      printf("Channel HV on...\n");
      printInactive();
      printf("which channel to turn on?");
      ii = scan2int();
      hvptr->xx[ii].onoff = 1;
      hvptr->com0=14;
      kill(hvptr->pid,SIGALRM); //send to hvmon for action
      break;

    case 15:                      // display temps and limits
      printf("Channel HV off...\n");
      printActive();
      printf("which channel to turn off?");
      ii = scan2int();
      hvptr->xx[ii].onoff = 0;
      hvptr->com0=15;
      kill(hvptr->pid,SIGALRM); //send to hvmon for action
      break;
*/
    case 16:                      // display temps and limits
      printf ("Alter parameters.\n");
      printAvailable();
      detParam();
      hvptr->com0=16;
      kill(hvptr->pid,SIGALRM);   // send an alarm to let hvmon know it needs to do command (1)
      break;

    case 17:
      printf("N/A Load config from file...\n");
      break;

    case 18:
      printf("Reseting.....\n");
      ResetAllChan();
      break;
    case 19:
      printf("Configuration:\n");
      printOut();
      break;

      
    case 100:                 // End ALL kelvin programs by breaking out of while statement
      hvptr->com0=-1;                   // send command to hvmon to break out of its while command
      hvptr->com1=-1;                   // send command to hvmon-log to break out of its while command
      hvptr->com2=-1;                   // send command to hvmon-daq to break out of its while command
      kill(hvptr->pid,SIGALRM);
      break;

    default:                  // Do nothing and go back to the list of options
      ans = 0;
      break;      
    }  
  }
  
/*
   Release the shared memory and close the U3
*/

  kill(hvptr->pid,SIGALRM);

  shmdt(hvptr);                      // detach from shared memory segment
  shmctl(hvshmid, IPC_RMID, NULL);     // remove the shared memory segment hopefully forever

//  if (munmap(hvptr, sizeof (struct hvmon*)) == -1) {
//    perror("Error un-mmapping the file");
/* Decide here whether to close(fd) and exit() or not. Depends... */
//  }
//  close(mapHVmon);

  return 0;
}
/******************************************************************************/
void ResetAllChan() {
  int ii=0;
  for (ii=0; ii<hvptr->maxchan; ii++){
    if (hvptr->xx[ii].type == 0) {
      ResetChan(ii);    //mpodGETguru(cmd, cmdResult);     // read the set voltage
    }
  }
  printf (" finished.\n");
  return;
}
/**************************************************************/
void ResetChan(int ii) {
  char cmd[140]="\0", cmdRes[140]="\0";
  if ( hvptr->xx[ii].slot ==0 ) 
  {
//    sprintf(cmd, "outputSwitch.u%i i %i", hvptr->xx[ii].chan,2);
    sprintf(cmd, "outputSwitch.u%i i %i", hvptr->xx[ii].chan,10);
  } else 
  {
//    sprintf(cmd, "outputSwitch.u%i0%i i %i", hvptr->xx[ii].slot, hvptr->xx[ii].chan,2);
    sprintf(cmd, "outputSwitch.u%i0%i i %i", hvptr->xx[ii].slot, hvptr->xx[ii].chan,10);
  }

  snmp(1,ii,cmd,cmdRes);
  return;
}
/**************************************************************/
void setOnOff(int ii) {
  char cmd[140]="\0", cmdRes[140]="\0";
  if ( hvptr->xx[ii].slot ==0 ) 
  {
    sprintf(cmd, "outputSwitch.u%i i %i", hvptr->xx[ii].chan,hvptr->xx[ii].onoff);
  } else 
  {
    sprintf(cmd, "outputSwitch.u%i0%i i %i", hvptr->xx[ii].slot, hvptr->xx[ii].chan,hvptr->xx[ii].onoff);
  }

  snmp(1,ii,cmd,cmdRes);
  return;
}
/******************************************************************************/
void mpodOnOff(int nf) {
  int ii=0;
  //char cmd[150]="\0", cmdResult[140]="\0";

  printf (" Getting MPOD data ....");  
  for (ii=0; ii<hvptr->maxchan; ii++){

    if (hvptr->xx[ii].type == 0) {
      hvptr->xx[ii].onoff = nf;      
      setOnOff(ii);    //mpodGETguru(cmd, cmdResult);     // read the set voltage
    }
  }
  printf (".... finished \n");  
  return;
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
  //printf("%s :: %s ",jj ,gg);
  return (nn);
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
     //printf("ok.");
    //    sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,MPOD_IP);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
    //sprintf (com,"snmpset -v 2c -L o -m %s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
    sprintf (com,"snmpset -v 2c -M /usr/share/snmp/mibs -m +%s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);  //guru (MPOD)-private-public (iBOOTBAR); versions 1 (iBOOTBAR) and 2c (MPOD)
  } 
  else if (setget == 0){
  //  printf("0ok");
    sprintf (com,"snmpget -Ov -v 2c -M /usr/share/snmp/mibs -m %s -c guru %s ",MPOD_MIB,hvptr->xx[ii].ip);
    //sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,hvptr->xx[ii].ip);
    //    sprintf (com,"snmpget -v 2c -L o -m %s -c guru  %s ",MPOD_MIB,MPOD_IP);
  }
  else if (setget == 2){              // if setget =2 then issue HV shutdown
    sprintf (com,"snmpset -OqvU -v 2c -M /usr/share/snmp/mibs -m %s -c guru %s i 0",MPOD_MIB,hvptr->xx[ii].ip);
  }
  strcat(com,cmd);                             // add command to snmp system call
  //printf("%s\n",com);

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

/***********************************************************/
/*int mmapSetup() {
  //#define FILEPATH "data/mmapped.bin"
  //#define FILESIZE sizeof(struct thermometer)
  int fd=0;//, result=0, ii=0;
  * Open a file for writing.
    *  - Creating the file if it doesn't exist.
    *  - Truncating it to 0 size if it already exists. (not really needed)
    *
    * Note: "O_WRONLY" mode is not sufficient when mmaping.
    *
  //   fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0644);
   fd = open(HVMONDATAPATH, O_RDWR, (mode_t)0644);
   if (fd == -1) {
        perror("Error opening hvmon file for writing");
        exit(EXIT_FAILURE);
   }

   * Now the file is ready to be mmapped.
    *
   hvptr = (struct hvmon*) mmap(0, HVMONDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (hvptr == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the hvmon file");
        exit(EXIT_FAILURE);
   }

   return (fd);
}
*/

/**************************************************************/

void shmSetup() {

  printf("Setting up shared memory...\n");///Users/c4g/src/LNfill/include/lnfill.conf
  hvshmKey = ftok(CONFIGFILE,'b');       // key unique identifier for shared memory based on CONF file name, other programs use 'LN' tag
  //  shmKey = ftok("SHM_PATH",'b');                                    // key unique identifier for shared memory, other programs use 'LN' tag
  hvshmid = shmget(hvshmKey, sizeof (struct hvmon), 0666 | IPC_CREAT);     // gets ID of shared memory, size, permissions, create if necessary
  hvptr = shmat (hvshmid, (void *)0, 0);                                  // now link to area so it can be used; struct lnfill pointer char *lnptr
  if (hvptr == (struct hvmon *)(-1)){                                  // check for errors
    perror("hv shmat");
  }

  hvptr->shm = hvshmid;   // this is the number of the shared memory segment
  shmKey = ftok(KELVINCONF,'b'); 
  shmid = shmget(shmKey, KELVINDATASIZE, 0666 | IPC_CREAT);
  degptr = (struct thermometer*) shmat (shmid, (void *)0, 0);  // now link to area so it can be used; struct lnfill pointer char 

  if (degptr == (struct thermometer *)(-1)){                                  // check for errors
    perror("kelvin shmat");
  }

  printf("ok...%li" ,hvshmid);
  return;
}
/******************************************************************/
int scan2int () {
  char sss[200];
  long int ii=0;
  
  scanf("%s",sss);
  ii=strtol(sss,NULL,10);                            // convert to base 10
  if ((ii == 0) && (strcmp(sss,"0") != 0)) ii = -1;  // check if 0 result is 0 or because input is not number
  return ((int) ii);
}

/**************************************************************/
float scan2float () {
  char sss[200];
  float xx=0;
  
  scanf("%s",sss);
  xx=atof(sss);                            // convert to base 10
  return (xx);
}

/**************************************************************/
void printOut() {
  int ii=0;
  //  time_t curtime;
/*
  union int2int4 {
    time_t ct;
    unsigned short int ust[4];
  } x;

  x.ct = degptr->tim.time1;
*/
  printf ("-----------------------------------------------------------------------------------------------------------\n");
  printf ("Time since start: %li s\n", hvptr->secRunning);
  printf ("-----------------------------------------------------------------------------------------------------------\n");
  //       12345678901234  123456  123456  123456  123456  123456
  /*printf ("     Name        vSet    vMeas   iSet    iMeas  On/Off   uRamp  dRamp  SVMax  V1Set  I1Set   Trp  ITrp eTrp \n");
  printf ("                  (V)     (V)    (uA)     (uA)  (bool)   (V/s)  (V/s)   (V)    (V)    (uA)   (s)            \n");
  printf ("--------------  ------  ------  ------  ------  ------  ------ ------ ------ ------ ------  ----  ---- ---- \n");
*/
  printf (" Type      IP address       Slot Chan      Name      vSet    iSet   uRamp   dRamp   Switch On/Off \n");
  printf ("                                                     (V)      (uA)  (V/s)   (V/s)       1/0       \n");
  printf ("------  --------------     ---- ---- -------------- ------  ------  ------  ------  ------------- \n");
    for (ii=0; ii<hvptr->maxchan; ii++){
    if (hvptr->xx[ii].type == 0) {
      printf ("%3i %20s  %3i  %3i ",hvptr->xx[ii].type,hvptr->xx[ii].ip,hvptr->xx[ii].slot,hvptr->xx[ii].chan);
      printf ("%14s  %6.1lf %6.0lf ",hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].iSet*1e6);
      printf ("%6.1lf  %6.1lf   %i \n",hvptr->xx[ii].vRamp,hvptr->xx[ii].downRamp, hvptr->xx[ii].onoff);
    }
    if (hvptr->xx[ii].onoff == 1 && hvptr->xx[ii].type == 1) {
      printf ("%20s %3i  %3i  %3i ",hvptr->xx[ii].ip,hvptr->xx[ii].type,hvptr->xx[ii].slot,hvptr->xx[ii].chan);
      printf ("%14s  %6.1lf  %6.1lf  ",hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].iSet);
      printf ("%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
      printf ("%6.1lf %6.1lf %6.1lf  ",hvptr->xx[ii].vMax, hvptr->xx[ii].v1Set,hvptr->xx[ii].i1Set);
      printf ("%4.1lf %3i  %3i ",hvptr->xx[ii].trip, hvptr->xx[ii].intTrip,hvptr->xx[ii].extTrip);
      printf("\n");
    }
  }
  printf ("--------------------------------------------------------------------------------------------------\n");
/*for (ii=0; ii<hvptr->maxchan; ii++){
    if (hvptr->xx[ii].type == 0) {
      printf ("%14s  %6.1lf  %6.1lf %6.6lf %6.6lf %3i ",
	      hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].vMeas,hvptr->xx[ii].iSet,hvptr->xx[ii].iMeas,hvptr->xx[ii].onoff);
      //      printf ("%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
      printf ("%6.1lf  ",hvptr->xx[ii].vRamp);
      printf("\n");
    }
    if (hvptr->xx[ii].onoff == 1 && hvptr->xx[ii].type == 1) {
      printf ("%14s  %6.1lf  %6.1lf  %6.1lf  %6.1lf     %i   ",
	      hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].vMeas,hvptr->xx[ii].iSet,hvptr->xx[ii].iMeas,hvptr->xx[ii].onoff);
      printf ("%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
      printf ("%6.1lf %6.1lf %6.1lf  ",hvptr->xx[ii].vMax, hvptr->xx[ii].v1Set,hvptr->xx[ii].i1Set);
      printf ("%4.1lf  %3i  %3i ",hvptr->xx[ii].trip, hvptr->xx[ii].intTrip,hvptr->xx[ii].extTrip);
      printf("\n");
    }
  }
  printf ("-----------------------------------------------------------------------------------------------------------\n");
  */
  return;
}

/******************************************************************/
void fprintOut() {
  int ii=0;
  //  time_t curtime;
/*
  union int2int4 {
    time_t ct;
    unsigned short int ust[4];
  } x;

  x.ct = degptr->tim.time1;
*/
//  curtime = time(NULL);
  fprintf (fileSave," Type      IP address       Slot Chan      Name      vSet    iSet   uRamp   dRamp   Switch On/Off \n");
  fprintf (fileSave,"                                                     (V)      (uA)  (V/s)   (V/s)       1/0       \n");
  fprintf (fileSave,"------  --------------     ---- ---- -------------- ------  ------  ------  ------  ------------- \n");
  for (ii=0; ii<hvptr->maxchan; ii++){
    if (hvptr->xx[ii].type == 0) {
      fprintf (fileSave,"%3i %20s  %3i  %3i ",hvptr->xx[ii].type,hvptr->xx[ii].ip,hvptr->xx[ii].slot,hvptr->xx[ii].chan);
      fprintf (fileSave,"%14s  %6.1lf %6.0lf ",hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].iSet*1e6);
      //      fprintf (fileSave,"%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
      fprintf (fileSave,"%6.1lf  %6.1lf   %i ",hvptr->xx[ii].vRamp,hvptr->xx[ii].downRamp, hvptr->xx[ii].onoff);
      //      fprintf (fileSave,"%6.1lf %6.1lf %6.1lf  ",hvptr->xx[ii].vMax, hvptr->xx[ii].v1Set,hvptr->xx[ii].i1Set);
      //      fprintf (fileSave,"%4.1lf %3i  %3i \n",hvptr->xx[ii].trip, hvptr->xx[ii].intTrip,hvptr->xx[ii].extTrip);
      fprintf(fileSave,"\n");
    }
    if (hvptr->xx[ii].onoff == 1 && hvptr->xx[ii].type == 1) {
      fprintf (fileSave,"%20s %3i  %3i  %3i ",hvptr->xx[ii].ip,hvptr->xx[ii].type,hvptr->xx[ii].slot,hvptr->xx[ii].chan);
      fprintf (fileSave,"%14s  %6.1lf  %6.1lf  ",hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].iSet);
      fprintf (fileSave,"%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
      fprintf (fileSave,"%6.1lf %6.1lf %6.1lf  ",hvptr->xx[ii].vMax, hvptr->xx[ii].v1Set,hvptr->xx[ii].i1Set);
      fprintf (fileSave,"%4.1lf %3i  %3i ",hvptr->xx[ii].trip, hvptr->xx[ii].intTrip,hvptr->xx[ii].extTrip);
      fprintf(fileSave,"\n");
    }
  }
  fprintf (fileSave,"-1\n");
  fprintf (fileSave,"-----------------------------------------------------------------------------------------------------------\n");
  
  return;
}

/**************************************************************/
void printActive() {
  int ii=0, tt=0;

  for (ii=0; ii < hvptr->maxchan; ii++){
    if (hvptr->xx[ii].onoff == 1) 
    {
      printf("%4i - %10s  ",ii,hvptr->xx[ii].name);
      tt++;
      if (tt%4 == 0) printf("\n");
    }
 }
  printf("\n");
  return;
}

/**************************************************************/
void printInactive() {
  int ii=0, tt=0;

  for (ii=0; ii < hvptr->maxchan; ii++){
    if (hvptr->xx[ii].onoff == 0) 
    {
      printf("%4i - %10s  ",ii,hvptr->xx[ii].name);
      tt++;
      if (tt%4 == 0) printf("\n");
    }
 }
  printf("\n");
  return;
}/**************************************************************/
void printAvailable() {
  int ii=0, tt=0;
  for (ii=0; ii < hvptr->maxchan; ii++){
      printf("%4i - %10s  ",ii,hvptr->xx[ii].name);
      tt++;
      if (tt%4 == 0) printf("\n");
  }
  printf("\n");
  return;
}
/******************************************************************/
void menu1(){
  int ii=0, temp=0;

  // ii = 0;
  printf ("---------------------------------------------------------------------------------------\n");
  printf ("     Global          |        Channels          |  Status   \n");
  printf ("---------------------------------------------------------------------------------------\n");
  printf ("1 - Status           | 11 - Clone channel       |  \n");
  printf ("2 - Force read       | 12 -                     |  \n");
  printf ("3 - Force read Temps | 13 -                     |  \n");
  printf ("4 - All HV on        | 14 -                     |  \n");
  printf ("5 - All HV off       | 15 -                     |  \n");
  printf ("6 - Save conf file   | 16 - Alter parameters    |  \n");
  printf ("7 -                  | 17 - Read from file      |  \n");
  printf ("8 - On list          | 18 - Reset All Status'   |  \n");
  printf ("9 - Off list         | 19 - Print Configure     |  \n");
  printf ("---------------------------------------------------------------------------------------\n");
  printf (" Time up - %li s     id: %li      \n",hvptr->secRunning, hvptr->shm);
  printf ("---------------------------------------------------------------------------------------\n");
  printf (" Temps (0,1,2) =(C,K,F): ");
  //for (ii=0;ii<16;ii++) if (hvptr->caenSlot[ii] > 0) printf ("%2.f  ",hvptr->caenTemp[ii]);
  if (shmKey != 0 )
  {
    for (ii=0;ii<degptr->maxchan; ii++)
//    for (ii=0;ii<2; ii++) 
      if (degptr->temps[ii].degree > -500)
      {
        //temp = hvptr->mpodTemp[ii];
        temp = degptr->temps[ii].degree;
        printf ("T: %i CKF: %i ", temp , degptr->temps[ii].unit);
      }
  }
  printf("\n");
  printf ("---------------------------------------------------------------------------------------\n");
  printf ("0 - end this program | 100 - end all HVmon     | \n");
  printf ("---------------------------------------------------------------------------------------\n");

return;
}

/***********************************************************/
void openSaveFile(){

  long int ii=0, jj=0, kk=0;
  char file_name[200]="hvmon-", tt[50]="\0";

/*
    Build file name out of date and time
*/
  sprintf(tt,"%s%c",GetDate(),'\0');
  jj=0;
  jj = strlen(tt);
  ii=0;
  kk=0;
  while (ii < jj) {
    if (isspace(tt[ii]) == 0) tt[kk++] = tt[ii];
    ii++;
  }
  tt[kk]='\0';
  if (jj > 0) strcat(file_name,tt);
  strcat(file_name,".conf\0");
/*
    Open file
*/
 if (( fileSave = fopen (file_name,"a+") ) == NULL){
   printf ("*** File on disk could not be opened \n");
   exit (EXIT_FAILURE);
 }
  printf("hvmon new config file opened: %s\n",file_name);             

  return;
}

/***********************************************************/
char *GetDate() {
  
  struct tm *timer;                                     // found in <time.h>
  time_t    myClock;                                    // found in <time.h>
  time((time_t *) &myClock);                            // get the current time
  tzset();                                              // put time in our timezone
  timer = (struct tm *) localtime((time_t *) &myClock); // convert time to local time

  return (char *) asctime(timer);                       // return it formatted
}


/******************************************************************************/
/***********************************************************/
void clone(){
  int ii=0, jj=0, kk=0, mm=0;

  printAvailable();
  printf("Clone from detector number ? e.g. 89 ( < 0 to quit)\n");
  ii = scan2int ();
  if (ii < 0) return;
  printInactive();
  printf("to slot ? e.g. 2 ( < 0 to quit) \n");
  jj = scan2int ();
  if (jj < 0) return;
  printf("to channel ? e.g. 21 ( < 0 to quit) \n");
  kk = scan2int ();
  if (kk < 0) return;

  printf("Will you want detector %i removed from the set-up (HV off) ? 1 = yes 0 = no \n",ii);
  mm = scan2int ();

  if (mm == 1) {
    printf("Changing slot and channel assignment to detector %i \n",ii);
    //    hvOff(ii);
    //hvptr->caenCrate[hvptr->xx[ii].slot] -= pow(2,hvptr->xx[ii].chan);  // remove old occupied channel bit
    hvptr->xx[ii].slot = jj;                                            // set new slot
    hvptr->xx[ii].chan = kk;                                            // set new channel
    //hvptr->caenCrate[jj] += pow(2,kk);                                  // add new occupied channel bit
    printf("Go to channel mode to turn on HV \n");                      // maxchan does not change
    }
  else if (mm == 0) {
    hvptr->xx[hvptr->maxchan] = hvptr->xx[ii];                          // transfer the data
    hvptr->xx[hvptr->maxchan].slot = jj;                                // correct to new slot
    hvptr->xx[hvptr->maxchan].chan = kk;                                // correct new channel
    //hvptr->caenCrate[jj] += pow(2,kk);                                  // add new occupied channel bit
    hvptr->maxchan++;                                                   // increment maxchan
    printf("Go to channel mode to turn on HV and change appropriate values such as detector name \n");
  }
  else
    printf("Neither 0 nor 1...doing nothing.. \n");
  
  return;
}

/******************************************************************************/
void detParam() {
  int ii=0, jj=0, kk=0, ll=0;
  float yy=0.0;

  printf ("Which detector number to change ?\n");
  ii = scan2int ();
      
  //       12345678901234  123456  123456  123456  123456  123456
  printf ("     Name        vSet    vMeas   iSet    iMeas  On/Off   uRamp  dRamp   VMax  \n");
  printf ("                  (V)     (V)    (uA)     (uA)  (bool)   (V/s)  (V/s)   (V)   \n");
  printf ("--------------  ------  ------  ------  ------  ------  ------ ------ ------- \n");
  printf ("%14s  %6.1lf  %6.1lf  %6.1lf  %6.1lf     %i   ",
	  hvptr->xx[ii].name,hvptr->xx[ii].vSet,hvptr->xx[ii].vMeas,hvptr->xx[ii].iSet,hvptr->xx[ii].iMeas,hvptr->xx[ii].onoff);
  printf ("%6.1lf %6.1lf  ",hvptr->xx[ii].vRamp, hvptr->xx[ii].downRamp);
  printf ("%6.1lf  ",hvptr->xx[ii].vMax);//, hvptr->xx[ii].v1Set,hvptr->xx[ii].i1Set);
//  printf ("%4.1lf  %3i  %3i ",hvptr->xx[ii].trip, hvptr->xx[ii].intTrip,hvptr->xx[ii].extTrip);
  printf("\n");
  printf ("-----------------------------------------------------------------------------------------------------------\n");
  while ( ll != 1 )
  {
    printf ("Type number of quantity to change:  0 = nothing   \n");
    printf ("  1 - Voltage setting  |  5 - Reset Status        |           \n");
    printf ("  2 - Current limit    |  6 - N/A                 |           \n");
    printf ("  3 - HV on/off        |  7 - HV Maximum setting  |           \n"); 
    printf ("  4 - Voltage ramp up  |  8 - Voltage ramp down   |           \n");

    jj = scan2int ();
    if (jj < 1 || jj > 8) return;

    switch (jj){
    
    case 1:
      printf ("What voltage setting? < %6.1lf V \n",hvptr->xx[ii].vMax);
      yy = scan2float();
      if (yy <= hvptr->xx[ii].vMax) hvptr->xx[ii].vSet = yy;
      break;
    case 2:
      printf ("What current limit setting?  (A) (5 A limit)\n");
      yy = scan2float();
      if (yy <= 5) hvptr->xx[ii].iSet = yy;
      break;
    case 3:
      if (hvptr->xx[ii].onoff == 1) 
      {
        printf ("Turn HV off ? 1 = yes \n");
        kk = scan2int();
        if (kk == 1) hvptr->xx[ii].onoff = 0;
      }
      else if (hvptr->xx[ii].onoff == 0) 
      {
        printf ("Turn HV on ? 1 = yes \n");
        kk = scan2int();
        if (kk == 1) hvptr->xx[ii].onoff = 1;
      }
      break;
    case 4:
      printf ("What voltage ramp-up setting? < 100 V/s \n");
      yy = scan2float();
      if (yy <= 100.0) hvptr->xx[ii].vRamp = yy;
      break;
    case 5:
      hvptr->xx[ii].reset = 1;
      break;
    case 6:
      printf ("Not used. \n");
      break;
    case 7:
      printf ("What maximum voltage setting? < 2000 V \n");
      yy = scan2float();
      if (yy <= 2000.0) hvptr->xx[ii].vMax = yy;
      break;
    case 8:
      printf ("What voltage ramp-down setting? < 50 V/s \n");
      yy = scan2float();
      if (yy < 50.0) hvptr->xx[ii].downRamp = yy;
      break;
    
    default:
      break;
    }
    printf("finished? 1/0 for yes/no.");
    ll= scan2int();
  }
  hvptr->com1 = ii;  // send detector to control program
  hvptr->com2 = jj;  // function to change by control program
  //   com3 and xcom3 are new values returned to the control program

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
      
  return;
}

/******************************************************************************/


/******************************************************************************/


/******************************************************************************/


/******************************************************************************/


/******************************************************************************/

