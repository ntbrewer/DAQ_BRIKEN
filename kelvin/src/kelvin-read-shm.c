/*********************************************************************/
/* Program deg-u3-read is used to control the LabJack U3-HV or U3-LV */
/*    to measure the temperature near the experiment                 */
/*                                                                   */
/* To be compiled with:                                              */
/*    gcc -o kelvin-read kelvin-read.c                               */
/*                                                                   */
/* C. J. Gross, February 2014                                        */
/*********************************************************************/

#include "../../include/u3.h"         // location may also be in: /usr/local/include
#include "../../include/kelvin-shm.h" 
//#include "kelvin-shm.h"
//int  mmapSetup();
void shmSetup();
void menu();
void printout();
int scan2int ();

/***********************************************************/
int main(int argc, char **argv){

  int ans=0, mapKelvin=0, kk=0;
/*
  Shared/mapped memory creation and attachment
  the segment number is stored in lnptr->pid
*/
  //mapKelvin = mmapSetup();
/*
  Scroll through options
*/
  shmSetup();
  while (ans != 100){
    menu();
    //    scanf ("%i", &ans);          // read in ans (pointer is indicated)     
    ans = scan2int ();
    
    switch (ans){
    case 0:                    // end program but not kelvin
      shmdt(degptr);           // detach from shared memory segment
      ans = 100;
      break;
    case 1:                      // display temps and limits do not communicate to kelvin
      printout();
      break;

    case 2:
      printout();
      degptr->com1 = 2;           // command (3) stored in SHM so kelvin can do something
      kill(degptr->pid,SIGALRM);  // send an alarm to let kelvin know it needs to do command (1)
      sleep(1);                   // sleep for a second to allow kelvin to read new values
      printout();
  
      break;

    case 3:                      // display temps and limits
      printf ("What interval in integer seconds ?\n");
      kk = scan2int ();
      //      scanf ("%li", &kk);          // read in ans (pointer is indicated)     
      degptr->interval = kk;
      degptr->com1 = 3;            // command (3) stored in SHM so kelvin can do something
      kill(degptr->pid,SIGALRM);   // send an alarm to let kelvin know it needs to do command (1)
      break;

    case 4:
      degptr->com2 = 0;            // change com2 in SHM so kelvin-log can stop
      break;
/*
    case 5:
      degptr->com3 = 0;            // change com3 in SHM so kelvin-daq can stop
      break;
*/
    case 100:                 // End ALL kelvin programs by breaking out of while statement
      degptr->com1=-1;                   // send command to kelvin to break out of its while command
      degptr->com2=-1;                   // send command to kelvin-log to break out of its while command
      degptr->com3=-1;                   // send command to kelvin-daq to break out of its while command
      kill(degptr->pid,SIGALRM);
      break;

    default:                  // Do nothing and go back to the list of options
      ans = 0;
      break;      
    }  
  }

/*
   Wrap up the program ending the kelvin detaching the mapped memory segment
   shmctl(shmid, IPC_RMID, NULL);  // remove the shared memory segment hopefully forever (needs to be in the main program)
*/  

//  if (munmap(degptr, sizeof (struct thermometer*)) == -1) {
//    perror("Error un-mmapping the file");
    /* Decide here whether to close(fd) and exit() or not. Depends... */
//  }
//  close(mapKelvin);          // close file

  shmdt(degptr);                     // detach from shared memory segment
  printf("detached from SHM\n");

  return 0;
}

/***********************************************************/
void menu(){

  printf ("\nOptions ? \n");
  printf ("    1 - display current temperature data (does not read or change interval) \n");
  printf ("    2 - force a read of current temperature data \n");
  printf ("    3 - change measurement interval (currently %i s)\n",degptr->interval);
  printf (" \n");
  printf ("    4 - End log file \n");
  //  printf ("    5 - End DAQ logging \n");
  printf (" \n");
  printf ("    0 - end this program only \n");
  printf (" \n");
  printf ("  100 - end all temperature programs \n");

return;
}

/**************************************************************/
void printout() {
  int ii=0;
/*
  union int2int4 {
    time_t ct;
    unsigned short int ust[4];
  } x;

  x.ct = degptr->tim.time1;
*/ 
  printf ("--------------------------------------\n");
  printf ("Time since start: %li s\n", (degptr->tim.time1 - degptr->time0) );
  printf ("--------------------------------------\n");
  printf ("Therm   Deg     Para    Data\n");
  printf ("----- -------  ------  -----\n");
  for (ii=0; ii<degptr->maxchan; ii++){
    printf (" %s    %.1lf     %i      %i \n",degptr->temps[ii].name, degptr->temps[ii].degree,degptr->temps[ii].para,degptr->temps[ii].data);
  }
  printf ("--------------------------------------\n");
  
  return;
}


/**************************************************************/
/*int mmapSetup() {
  int fd=0;     // mapped file descriptor

  * Open a file for writing.
   *  - Creating the file if it doesn't exist.
   *  - Truncating it to 0 size if it already exists. (not really needed)
   *
   * Note: "O_WRONLY" mode is not sufficient when mmaping.
   *
  fd = open(KELVINDATAPATH, O_RDWR, (mode_t)0600);
  if (fd == -1) {
    perror("Error opening file path for writing");
    exit(EXIT_FAILURE);
  }
        
  * Now the file is ready to be mmapped.
   *
  degptr = (struct thermometer*) mmap(0, KELVINDATASIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (degptr == MAP_FAILED) {
    close(fd);
    perror("Error mmapping the file");
    exit(EXIT_FAILURE);
  }
  * Don't forget to free the mmapped memory ... usually at end of main
   *
   
  return (fd);
}
*/
/**************************************************************/
void shmSetup() {
  //   file is probably: include/deg.conf
  printf("Setting up shared memory...\n");

  shmKey = ftok(KELVINCONF,'b');                // key unique identifier for shared memory, other programs use 'LN' tag
  //  shmKey = ftok("include/deg.conf",'b');  // key unique identifier for shared memory, other programs use 'LN' tag

  shmid = shmget(shmKey, sizeof (struct thermometer), 0666 | IPC_CREAT);   // get ID of shared memory, size, permissions, create if necessary
  if (shmid == -1){
    printf("shmid=%li, shmKey=%i, size=%li\n",
	   shmid, shmKey, sizeof (struct thermometer));
    perror("shmget");
    exit(EXIT_FAILURE);
  }
  printf("May need to remove shmid: ipcrm -m %li  \n",shmid);
  degptr =  (struct thermometer*) shmat (shmid, (void *)0, 0);   // link area used; struct lnfill pointer char *lnptr

  if (degptr == (struct thermometer *)(-1)){                                  // check for errors
    printf("number 2a error\n");
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  degptr->shm = shmid;   // this is the number of the shared memory segment

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
