/*********************************************************************/
/* Program deg-u3-read is used to control the LabJack U3-HV or U3-LV */
/*    to measure the temperature near the experiment                 */
/*                                                                   */
/* To be compiled with:                                              */
/*    gcc -o deg-u3-read deg-u3-read.c                               */
/*                                                                   */
/* C. J. Gross, February 2014                                        */
/*********************************************************************/
#include <unistd.h>  /* UNIX standard function definitions */
#include <time.h>    /* Time definitions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <ctype.h>
#include <stdbool.h>
#include <math.h>    /* Math definitions */
#include <signal.h>  /* Signal interrupt definitions */
#include <fcntl.h>
#include <sys/mman.h>

#include "../include/usrdata.h"

//#include "include/u3.h"     // location may also be in: /usr/local/include
//#include "include/kelvin.h" 

int  mmapSetup();
void menu();
int toggle(int jj);
void closeUserData(int mapUser);
int openUserData();
int scan2int ();

/***********************************************************/
int main(int argc, char **argv){
  int ans=0, ii=0;
  int mapUser = 0;
/*
  Shared/mapped memory creation and attachment
  the segment number is stored in lnptr->pid
*/
  //  mapCom = openDaqCom();
  mapUser = openUserData();
  while (ans != 100){
    menu();
    //    scanf ("%i", &ans);          // read in ans (pointer is indicated)     
    ans = scan2int ();
    
    switch (ans){
    case 0:                    // end program but not kelvin
      ans = 100;
      break;
    case 1:                      // display temps and limits do not communicate to kelvin
      //      printout();
      break;
    case 3:                      // toggle usrData on/off
      usrptr->onoff = toggle(usrptr->onoff);
      break;
    case 4:                      // toggle kelvin data on/off
      usrptr->kelvin = toggle(usrptr->kelvin);
      break;
    case 5:                      // toggle geln data on/off
      usrptr->geLN = toggle(usrptr->geLN);
      break;
    case 6:                      // toggle caenHV on/off
      usrptr->caenHV = toggle(usrptr->caenHV);
      break;
    case 7:                      // toggle mpodHV on/off
      usrptr->mpodHV = toggle(usrptr->mpodHV);
      break;
    case 2:                      // toggle kelvin data on/off
      if (usrptr->onoff && usrptr->data[0] > 0) {
	printf("User data contents:\n");
	printf(" Index   Value\n");
	printf(" -----  --------\n");
	for (ii=0; ii<usrptr->data[0]; ii++){
	  printf (" %i  %i \n",ii, usrptr->data[ii]);
	};
      }
      else if (usrptr->onoff) printf("Length of user data is 0\n");
      else printf("User data must be off\n");
	
      break;
    case 100:                          // End usr2xia programs by breaking out of while statement
      usrptr->com0 = 2;                // 
      break;

    default:                  // Do nothing and go back to the list of options
      ans = 0;
      break;      
    }  
  }



/*
   Wrap up the program ending the program
*/

  if (mapUser != -1) closeUserData(mapUser);

  return (0);
}



/**************************************************************/
int toggle(int jj) {
  if (jj == 0) jj = 1;
  else jj = 0;
  return (jj);
}

/**************************************************************/


/********************************************************************************************************************************************************************************/

/***************************************************** User Data Section Below **************************************************************************************************/
int openUserData() {
    int fd=0;                // mapped file descriptor
    //    char fileData[200] = "data/usrdata.bin";
    //    int zero=0;
    //    ssize_t result=0;
    /*
     Open a file for writing.
     - Creating the file if it doesn't exist.
     - read/write/create/fail if exists/truncate to 0 size      u:rw, g:r o:r
     - Truncating it to 0 size if it already exists. (not really needed)
     - include O_RDWR | O_CREAT | O_EXCL | O_TRUNC if you don't want to overwrite existing data
     Note: "O_WRONLY" mode is not sufficient when mmaping.
     */
    
    printf("Setting up memory mapped file: %s\n", USRDATAPATH);
    fd = open(USRDATAPATH, O_RDWR, (mode_t)0644);
    if (fd == -1) {
        perror("Error opening usrdata for writing ");
        printf (" %s \n",USRDATAPATH);
        exit(EXIT_FAILURE);
    }
    /*
     Now the file is ready to be mmapped.
     */
     //     usrptr = (int*) mmap(0, sizeData, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  // might be uint32!
     usrptr = (struct usrdat*) mmap(0, USRDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  // might be uint32!
    if (usrptr == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    
    /*
     Don't forget to free the mmapped memory usually at end of main
     */
    return (fd);
}

/**************************************************************/
void closeUserData(int mapUser){
    
  if (mapUser != -1 ) {                                  // if file is active
    if (munmap(usrptr, sizeof(struct usrdat)) == -1) {      // unmap memory
      perror("Error un-mmapping the usrData file");
    }
    close(mapUser);                                    // close usrptr->usrData file
  }
  
  return;
}


/**************************************************************/
void menu(){
  time_t curtime=0;

  curtime = time(NULL);
  printf ("----------------------------------------------------------------------------\n");
  printf ("    Toggle ON/OFF     | Current settings   |  Time since last check (s)    \n");
  printf ("----------------------------------------------------------------------------\n");
  printf ("1 - status            |                    |   \n");  //,usrptr->usrDataTime);
  printf ("2 - show data         |                    |   \n");  //,usrptr->usrDataTime);
  if (usrptr->onoff)
    printf ("3 - User Data         | ON                 |   %i  \n",(int) (curtime - usrptr->time));
  else
    printf ("3 - User Data         | OFF                |     \n");
  if (usrptr->kelvin)
    printf ("4 - Kelvin Data       | ON                 |   %i  \n",(int) (curtime - usrptr->kelvinTime));
  else
    printf ("4 - Kelvin Data       | OFF                |     \n");
  if (usrptr->geLN)
    printf ("5 - geLN Data         | ON                 |   %i  \n",(int) (curtime - usrptr->geLNTime));
  else
    printf ("5 - geLN Data         | OFF                |     \n");
  if (usrptr->caenHV)
    printf ("6 - caenHV Data       | ON                 |   %i  \n",(int) (curtime - usrptr->caenHVTime));
  else
    printf ("6 - caenHV Data       | OFF                |     \n");
  if (usrptr->mpodHV)
    printf ("7 - mpodHV Data       | ON                 |   %i  \n",(int) (curtime - usrptr->mpodHVTime));
  else
    printf ("7 - mpodHV Data       | OFF                |     \n");

  printf ("----------------------------------------------------------------------------\n");
  printf ("0 - end this program | 100 - End all programs  \n");
  printf ("----------------------------------------------------------------------------\n");

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

/******************************************************************/
