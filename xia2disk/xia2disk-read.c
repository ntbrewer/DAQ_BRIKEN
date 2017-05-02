#include <unistd.h>  /* UNIX standard function definitions */
#include <time.h>    /* Time definitions */
#include <stdio.h>   /* file input/output definitions */
#include <stdlib.h>   /* standard c commands */
#include <string.h>  /* String function definitions */
#include <fcntl.h>   /* File control definitions */
#include <sys/types.h> /* for shared/mapped memory stuff */
#include <sys/mman.h> /* for memory mapping  */
#include <stdbool.h>   /* boolean h file available with C99 */

#include "../include/xia2disk.h"

int menu();
int toggle(int jj);
int scan2int ();
void status();
void zero();
void options();
void foptions();
time_t time0=0;

int main (int argc, char **argv){
  int run=0;
  
  time0 = time(NULL);
  mapCom = openDaqCom();

  printf ("daq file: %s\n",daqptr->daqfile);
  printf ("daq: %i\n",daqptr->daq);
  printf ("usr2daq: %i\n",daqptr->usr2daq);
  printf ("kelvin: %i\n",daqptr->kelvin);

  while (daqptr->daq != 3 && run == 0) run = menu();

  printf("DAQ IS ENDING - CLOSING FILE AND STOPPING DATA TAKING FUNCTIONS \n");
  if (daqptr->mtas_hv != 2) daqptr->mtas_hv = 0;
  if (daqptr->geln != 2)    daqptr->geln    = 0;
  if (daqptr->kelvin != 2)  daqptr->kelvin  = 0;
  printf("GO TO (USER DATA)-READ PROGRAMS IF MONITORING SUCH AS GELN SHOULD BE ENDED \n");
  closeDaqCom(mapCom);

  return (0);
}


/**************************************************************/
int openDaqCom() {
  //#define comDataSize sizeof(struct comData)

  int fd=0;                // mapped file descriptor
  ssize_t result=0;
    /*
     Open a file for writing.
     - Creating the file if it doesn't exist.
     - read/write/create/fail if exists/truncate to 0 size      u:rw, g:r o:r
     - Truncating it to 0 size if it already exists. (not really needed)
     - include O_RDWR | O_CREAT | O_EXCL | O_TRUNC if you don't want to overwrite existing data
     Note: "O_WRONLY" mode is not sufficient when mmaping.
     */
    
    printf("Setting up memory mapped usr2xia file:\n");
    fd = open(COMDATAPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0644);
    if (fd == -1) {
        perror("Error opening usr2xia for writing ");
        exit(EXIT_FAILURE);
    }
    /*
     Stretch the file size to the size of the (mmapped) array/structure/etc.
     We can choose to write to the entire file or seek the end and writing 1 word
     */
    printf ("going to end of file\n");
    result = lseek(fd, COMDATASIZE-1, SEEK_SET);       //268435455 - effectively zeros the array according to man lseek
    if (result == -1) {
        close(fd);
        perror("Error calling lseek() to 'stretch' the usr2xia file");
        exit(EXIT_FAILURE);
    }
    
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
    
    result = write(fd, "", 1);
    if (result != 1) {
      close(fd);
      perror("Error writing last byte of the file");
      exit(EXIT_FAILURE);
    }
    
    /*
     Now the file is ready to be mmapped.
     */
    daqptr = (struct comData*) mmap(0, COMDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  // might be uint32!
    if (daqptr == MAP_FAILED) {
      close(fd);
      perror("Error mmapping the usr2xia file");
      exit(EXIT_FAILURE);
    }
    
    /*
     Don't forget to free the mmapped memory usually at end of main
     */
    return (fd);
}

/**************************************************************/
void closeDaqCom(int mapCom){
    
  if (mapCom != -1 ) {                                  // if file is active
    if (munmap(daqptr, sizeof (*daqptr)) == -1) {      // unmap memory
      perror("Error un-mmapping the usr2xia file");
    }
    close(mapCom);                                    // close usrData file
  }  
  return;
}

/**************************************************************/
int toggle(int jj) {
  if (jj == 0) jj = 1;
  else jj = 0;
  return (jj);
}

/**************************************************************/
void status(){
  int fg_black = 30;        // colors may not be standard across all platforms....
  int bg_green = 42;        // but probably good enough...not 47 = grey and 48 = white
  int bg_white = 48;        // though website said 7 was white...saw other differences as well
  int attr = 0, attr1 = 0;  // 5 = blinking, 1 = bold, 0 = normal (resets attributes),3 = underline , 

  printf ("Status: ");
// daq
  if (daqptr->daq == 1) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" daq: ON ");
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  if (daqptr->daq != 1) printf (" daq:OFF  ");

// data2disk
  if (daqptr->file) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" disk: ON [%s] ",daqptr->daqfile);
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  else  printf (" disk:OFF  ");
  //  if (!daqptr->file) printf (" disk:OFF  ");

// output file
  if (daqptr->file) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" output file: ON ");
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  else printf (" output file:OFF  ");
// usr2daq
  if (daqptr->usr2daq) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" user data: ON ");
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  else printf (" user data:OFF  ");
// statistics
  if (daqptr->statsYES) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" statistics: ON ");
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  else printf (" statistics:OFF  ");
// histograms
  if (daqptr->histoYES) {
    printf ("%c[%d;%d;%dm",0x1B, attr1, fg_black, bg_green);
    printf (" histogram: ON ");
    printf ("%c[%d;%d;%dm  ",0x1B, attr, fg_black, bg_white);
  }
  else printf (" histogram:OFF  ");

  printf("\n");
  return;
}
/**************************************************************/
int menu() {
  char yes[4] = "y", yn[4] = "n";
  time_t mm=0;
  char ans='a';
  
  mm = time(NULL);
  printf ("time = %li    \n",mm);
  status();
  printf ("       s-top    g-o     f-ile     o-ptions     e-X-it   \n");   
  scanf("%s",&ans);
  
  switch (ans){
  case 'X':
    daqptr->daq = 0;                            // stop daq
    daqptr->response = false;                   // wait for a response
    while (!daqptr->response) usleep(1000);     // waiting for daq to respond that this has changed
    daqptr->response = false;                   // wait for a response
    printf(" DAQ has stopped.  Do you really want to end? y/n\n");
    scanf("%s",yn);
    if (strcmp(yn,yes) == 0) daqptr->daq = 2;   // was 2 , now make 3, try back to 2
    else break;
    daqptr->response = false;                   // wait for a response
    while (!daqptr->response) usleep(1000);     // waiting for daq to respond that this has changed
    daqptr->response = false;                   // wait for a response
    printf(" DAQ should be ending - End this program too? \n");
    scanf("%s",yn);
    if (strcmp(yn,yes) == 0) return(1);         // return 1 to end this program
    break;
  case 's':
    daqptr->response = false;                   // wait for a response
    if (daqptr->daq != 0) {
      daqptr->daq = 0;
      while (!daqptr->response) usleep(1000);   // waiting for daq to respond that this has changed
      daqptr->response = false;                 // wait for a response
    }
    printf(" DAQ should be stopped.\n");
    break;
  case 'g':
    daqptr->response = false;                   // wait for a response
    if (daqptr->daq != 1) daqptr->daq = 1;
    while (!daqptr->response) usleep(1000);     // waiting for daq to respond that this has changed
    daqptr->response = false;                   // wait for a response
    printf(" DAQ should be running\n");
    break;
  case 'f':
    foptions();
    break;
  case 'o':
    options();
    break;
  default:
    break;
  }
 
  return(0);
}

/**************************************************************
void zero(){

  daqptr->file=0;
  daqptr->daq=0;
  daqptr->usr2daq=0;
  daqptr->kelvin=0;
  daqptr->geln=0;
  daqptr->mtas_hv=0;
  strcpy(daqptr->daqfile,"test.xxx");

  return;
}
*/
/**************************************************************/
int scan2int () {
  char sss[200];
  long int ii=0;
  
  scanf("%s",sss);
  ii=strtol(sss,NULL,10);                            // convert to base 10
  if ((ii == 0) && (strcmp(sss,"0") != 0)) ii = -1;  // check if 0 result is 0 or because input is not number
  return ((int) ii);
}

/******************************************************************/
void options(){
  char ans='a';
  int nn=0;
  
  printf (" Boot pixies  :  b-oot               f-astboot                               \n");
  printf (" Toggle on/off:  u-serdata           s-tatistics         h-istogram          \n");
  printf ("                 q-uiet              m-odulerates        z-eroClocks         \n");
  printf (" Change values:  H-istoInterval=%i   S-tatsInterval=%i   t-hreshPercent =%i  \n",daqptr->histoInterval,daqptr->statsInterval,daqptr->threshPercent);

  scanf("%s",&ans);
  switch(ans) {
  case 'u':
    daqptr->usr2daq = toggle((daqptr->usr2daq));
    break;
  case 's':
    daqptr->statsYES = toggle((daqptr->statsYES));
    break;
  case 'h':
    daqptr->histoYES = toggle((daqptr->histoYES));
    break;
  case 'b':
    daqptr->fastBoot = false;
    daqptr->com0 = 3;                                     // com0 = 3 = boot
    daqptr->daq = 0;
    daqptr->response = false;                             // wait for a response
    while (!daqptr->response) usleep(1000);               // waiting for daq to respond that this has changed
    daqptr->response = false;                             // wait for a response
    break;      
  case 'f':
    daqptr->fastBoot = true;
    daqptr->com0 = 3;                                     // com0 = 3 = boot
    daqptr->daq = 0;
    daqptr->response = false;                             // wait for a response
    while (!daqptr->response) usleep(1000);               // waiting for daq to respond that this has changed
    daqptr->response = false;                             // wait for a response
    break;
  case 'q':
    daqptr->quiet = toggle((daqptr->quiet));              // can be changed at any time
    break;
  case 'm':                                               // can be changed at any time (prints text to screen)
    daqptr->showModuleRates = toggle((daqptr->showModuleRates));
    break;
  case 'z':
    printf(" stopping DAQ and clocks will zero on restart \n");  
    daqptr->zeroClocks = true;
    daqptr->com0 = 1;                                     // indicate something changed..in this case zeroclocks
    daqptr->daq = 0;                                      // stop DAQ
    daqptr->response = false;                             // wait for a response
    while (!daqptr->response) usleep(1000);               // waiting for daq to respond that this has changed
    daqptr->response = false;
    break;
  case 'H':                                               // can be changed at any time
    printf ("Interval to write histograms ? -1 = OFF (currently %i seconds)\n",daqptr->histoInterval);
    nn = scan2int ();
    daqptr->histoInterval = nn;
    break;
  case 'S':                                               // can be changed at any time
    printf ("Interval to write statistics ? -1 = OFF (currently %i seconds)\n",daqptr->statsInterval);
    nn = scan2int ();
    daqptr->statsInterval = nn;
    break;
  case 't':                                               // can be changed at any time
    while (nn > 0 && nn <= 100){
      printf ("Percent of FIFO declared full ? (currently %i percent)\n",daqptr->threshPercent);
      nn = scan2int ();
      daqptr->threshPercent = nn;
      daqptr->com0 = 1;                                   // indicate something changed..in this case zeroclocks
      daqptr->response = false;                          
      while (!daqptr->response) usleep(1000);             // waiting for daq to respond that this has changed
      daqptr->response = false;                          
    }
    break;
  default:
    break;
  }
 
  return;
}

/******************************************************************/
void foptions() {
  char ans='a';
  char yes[4] = "y", yn[4] = "n";

  printf ("     Type n or f for :   n-ew file   or   t-oggle on/off  \n ");  // anything else does nothing
  scanf("%s",&ans);
  switch(ans) {
  case 't':
    daqptr->file = toggle((daqptr->file));
    break;
  case 'n':
    if (daqptr->file) {
      printf(" DAQ taking data to disk - Shall it stop to change file name? y/n\n");
      scanf("%s",yn);
      if (strcmp(yn,yes) != 0) daqptr->file = toggle((daqptr->file)); //break;
    }
    printf ("        Name ? old file = %s\n",daqptr->daqfile);   // anything else does nothing
    scanf("%s",daqptr->daqfile);                                 //  daqprt->daqfile);
    daqptr->file = 0;                                            // set to 2 to indicate file name has changed
    daqptr->com0 = 2;
    daqptr->daq = 0;
    daqptr->response = false;                             // wait for a response
    while (!daqptr->response) usleep(1000);               // waiting for daq to respond that this has changed daqptr->file should now be 0
    daqptr->response = false;                             // wait for a response
    printf ("Start writing to file? y/n \n");
    scanf("%s",yn);
    if (strcmp(yn,yes) == 0) daqptr->file = 1;
    break;
  default:
    break;
  }
  
  return;
}
/******************************************************************/
