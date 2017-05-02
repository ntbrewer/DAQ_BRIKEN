//
//  scan-map.c
//  
//
//  Created by Carl Gross on 4/1/14.
//
//

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
// put include files for memory mapping
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>           // include file for open/close/return commands
#include <time.h>
//#include <vector>
//#include <iterator>
//#include <algorithm>
#include <stdint.h>

// 64-bit unsigned integer clock in picoseconds is good for 213.5 days
// 16 F's = 64 binary 1's = 18 446 744 073 709 551 615 ps = 18 446 744 s = 5124 hr = 213.5 d
int mmapListDataInp(char *infile);                // sets up the mapped memory
int openFileInp(char *oname);                     // opens list mode file and calls mmapSetup
void closeFileInp(int ifile);                     // unmaps/closes list mode file
uint64_t *tolptr;                                 // mapped array of ints (64 bit = 8 bytes)...

struct evt{
    uint64_t ps;           // 64-bit clock given in picoseconds  ... this could contain 48-bit clock + CFD information
    uint16_t id;           // encoding of mod-chan ID = chan + 16*slot + 256*crate : Pixie has 16 channels max in 14-slot crates up to 16 crates
     int16_t status;       // bits set 1 = sums, 2 = qdc, 4 = trace active, 8 = pile-up
    uint16_t energy;       // energy up to 65535
    uint16_t lent;         // length of trace     // 128-bit aligned
    uint32_t sum[4];       // 128-bit aligned
    uint32_t qdc[8];       // 128-bit aligned
    uint16_t trace[4000];  // must be divisible by 8 to be 128-bit aligned
};

off_t fsize=0;

/**************************************************************************/
int main(int argc, char **argv) {
    struct stat sb;             // file status structure for fstat
    int ifile=0, ofile=0;
    char oname[190]="\0";      // name of file to open
    uint64_t x0=0, xsave;
    int ii=0, jj=0, iimax=0, tt[1000], ss=0, rr=0, pp=0;

  union int16int32int64 {
    uint64_t x1;           // 64-bit clock
    uint32_t qs[2];
    uint16_t px[4];        // 32-bit clock parts from pixie
  } x;
  union int32int64 {
    uint64_t x0;           // 64-bit clock
    uint32_t qr[2];
  } w;
   int S='S', P='P', I='I', L='L', T='T', A='A';
   int spill=0, counts=0;
   struct evt n;
   int wallClock=0, lenUser=0, lenSpill=0;
/*
     Input LDF-equivalent file on command line
*/
    if (argc == 2) {
        strcpy(oname,argv[1]);
    }
    else {
        printf(" No tol file defined....input file name:\n");
        scanf ("%s",oname);
    }
    ifile=openFileInp(oname);
    //    fstat(ifile,&sb);
    //    intFilesize = sb.st_size/sizeof(int);
    //    printf ("file size in ints = %i\n",intFilesize);      // file is open and size determined
    iimax = (int) fsize/(sizeof(uint64_t));
    for (ii=0;ii<1000;ii++){
      tt[ii]=0;                       // event array
    }

    for (ii=0;ii<iimax; ii=ii+2){
      w.x0 = tolptr[ii];             // ps clock
      x.x1 = tolptr[ii+1];           // other header data
      if (w.qr[0]==S && w.qr[1]==P && x.qs[0]==I && x.qs[1]==L) {
	w.x0 = tolptr[ii+2];
	x.x1 = tolptr[ii+3];           // other header data
	spill = w.qr[0];
	lenSpill = w.qr[1] - ii;
	wallClock = x.qs[0];
	lenUser = x.qs[1];
	ii += 2;   // another 2 gets added at end of for loop
	if (lenUser != 0) ii += (lenUser-1)/2;
	printf (" Spill number: %i in ii = %i and spill length: %i \n",spill, ii, w.qr[1]);
      }
      else {
	n.id=x.px[0];
	n.status=x.px[1];
	n.energy=x.px[2];
	n.lent=x.px[3];
	printf("n.id=%i, n.status=%i, n.energy=%i, n.lent=%i\n",n.id,n.status,n.energy,n.lent);
	if (n.lent !=0) {
	  memcpy(&n.trace[0],&tolptr[ii+2],n.lent*2);
	  //	  for (jj=0;jj<n.lent;jj++) printf("trace[%i] = %u \n",jj,n.trace[jj]);
	  ii += (n.lent*2)/sizeof(uint64_t);
	}

	//      if (x.px[1] < 7) printf (" time = %llu, id=%u, E = %u \n",x0,x.px[0],x.px[2]);
	//      else printf (" time = %llu, id=%u, E = %u piled-up \n",x0,x.px[0],x.px[2]);
	if (n.id < 1000 && n.id > -1) tt[n.id]++;
	else {
	  rr++;
	  printf ("id out of range: %i \n", n.id);
	}
	if (n.id == 0){
	  printf (" chan 0 in spill %i at position %i or byte %i \n",spill,ii, ii*8);
	  //	  printf ("x0 = %x    x1 = %x  \n",w.x64,x.x64);
	  //       	  ii=iimax;
	}
	if (ii <7 && xsave > x0) ss++;
	if (x.px[1] > 7) pp++;
	xsave=x0;
      }
      //      if (spill == 400) break;
    }
    printf ("data out of time : %i\n",ss);
    printf ("id out of range  : %i\n",rr);
    printf ("piled up events  : %i\n",pp);
    for (ii=2;ii<1000;ii++){
      if (tt[ii] != 0) {
	printf ("ii = %i, tt = %i\n",ii,tt[ii]);
	counts += tt[ii]; 
      }
    }
    printf ("total events  : %i\n",counts);
    closeFileInp(ifile);
    //    printf ("wcount=%li   scount=%li   count=%li  \n", wcount,scount,count);
    
    return EXIT_SUCCESS;
}

/**************************************************************/
int openFileInp(char *oname) {
    int fd;
    char infile[200]="\0";
/*
     Function to open and map list mode output file
     See poll for how file names auto incremented and limied to 1 GB in size
*/
    strcpy(infile,oname);  // require full name to allow skipping already processed files
    fd = mmapListDataInp(infile);     // call file open and map the array pointer tolptr
    
    return (fd);
}

/**************************************************************/
void closeFileInp(int ifile) {
    /*
     Function to unmap and close list mode output file
     See poll for how file is opened, setup, truncated at end and closed
     */
    munmap(tolptr,sizeof (*tolptr));
    close(ifile);                                  // close in file
    printf("Input file unmapped and closed...\n");
    
    return;
}

/**************************************************************/
/**************************************************************/
int mmapListDataInp(char *infile) {
/*
    Function to open and memory map the list mode file
*/
    int fd=0;                // mapped file descriptor
    ssize_t result=0;
    //    off_t fsize=0;
    
    printf("Setting up memory mapped file: %s\n", infile);
/*
     Open a file for writing.
     - Creating the file if it doesn't exist.
     - Truncating it to 0 size if it already exists. (not really needed)
     Note: "O_WRONLY" mode is not sufficient when mmaping.
*/
    //printf("Setting up memory mapped file: %s\n", (char *)FILEPATH);
    // read/write/create/overwrite if exists/truncate to 0 size      u:rw, g: o:
    //    fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    //
    fd = open(infile, O_RDWR, (mode_t)0644);
    if (fd == -1) {
        perror("Error opening file path for writing");
        exit(EXIT_FAILURE);
    }
    fsize = lseek(fd,0,SEEK_END);
    printf ("File size: %i bytes\n",(int)fsize);
    /*
     Now the file is ready to be mmapped.
*/
//	xxxptr = (int*) mmap(0, LISTDATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    tolptr = (uint64_t*) mmap(0, fsize, PROT_READ, MAP_SHARED, fd, 0);          // cannot write to file (I hope!)
    if (tolptr == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the input file");
        exit(EXIT_FAILURE);
    }
    
/*
     Don't forget to free the mmapped memory usually at end of main
*/
    return (fd);
}
