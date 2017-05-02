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
#include <vector>
#include <iterator>
#include <algorithm>
#include <stdint.h>

// 64-bit unsigned integer clock in picoseconds is good for 213.5 days
// 16 F's = 64 binary 1's = 18 446 744 073 709 551 615 ps = 18 446 744 s = 5124 hr = 213.5 d

// stuff for mapped LDF output
#define LISTDATA_INTS (268435456)
#define LISTDATA_SIZE (LISTDATA_INTS * sizeof(int))  //1 GB of 4 byte ints; this is 262144 pages of 1024 * 4 bytes
//int xxx[268435456];  //1 GB of 4 byte ints; this is 262144 pages of 1024 * 4 bytes

int mmapListDataInp(char *infile);                // sets up the mapped memory
int mmapListDataOut(char *outfile);               // sets up the mapped memory
int openFileInp(char *oname);                     // opens list mode file and calls mmapSetup
int openFileOut(char *oname);                     // opens list mode file and calls mmapSetup
void closeFileInp(int ifile);                     // unmaps/closes list mode file
void closeFileOut(int ofile);                     // unmaps/closes list mode file
int *xxxptr;                                      // mapped array of ints (32 bit = 4 bytes)...268,435,456 = 1 GB
uint64_t *tolptr;                                 // mapped array of ints (64 bit = 8 bytes)...
/*
short *sssptr;
union int16int32 {
    int *xxxptr;              // mapped array of ints (32 bit = 4 bytes)...268,435,456
    short *sssptr;
} y;
*/
// long time_x=0;
off_t fsize=0;
off_t pos=0;
int spillNum=0;     // spill number in header file
int lastSpill=0;    // record last spill position to get off nn=0 on first/second write to file
int xsize=0;
int cnt0=0;
int rcount=0;
//char infile[200] = "\0";   // file to open
//char oname[190]="\0";      // name of file to open
//int ofile =0;              // integer file descriptor
int onum=0;                // number appended to file for auto close and open at 1 GB
int intFilesize=0;         // size of data file in int
const int maxVSN = 15;     // 14 modules + 1 for user supplied data

const int SPILLDATA_MAX=2000000;     // maximum data in spill (might need adjusting)
//unsigned int mod[15][200000];        // 2D-array that contains each module of data
long count=0, scount=0, wcount=0;    // counts of data for sorting checks

void pixie16ORNL();
//void pixie16NSCL();
void userStruc(unsigned int usrData[]);
void buildStruc(unsigned int modData[], unsigned int lenSpill);         // builds individual events into struct event
//void buildStruc(unsigned int max, unsigned int vsn);         // builds individual events into struct event
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

struct event {                                               // and puts them into a vector SpillData
    long tim;                                                // which is then sorted in time
    unsigned int crate, slot, chan;                          // crate, slot, and chan of pixie module
    unsigned int pileup, energy, cfd;                        // pileup bit, energy, cfd (fraction between 10 ns)
    unsigned int len, lenh, lent;                            // length of record, header, trace
    unsigned int trace[1500], sum[4], qdc[8];                // from sums and trace from pixie
    unsigned int vchan, satBit;                              // also from pixie
    unsigned int id, cal_energy, mult;
    unsigned int evtTime, corTime, hiResTime;
};
/*
struct by_time {                                                  // sort in time routine
    bool operator() (event const &left, event const &right) {
        return left.tim < right.tim;                              // sorts in FIFO; if > later times will be first
    }
};
std::vector<event> spillData;                                     // vector that holds spill events sorted time
*/
struct by_time1 {                                                  // sort in time routine
    bool operator() (evt const &left, evt const &right) {
        return left.ps < right.ps;                              // sorts in FIFO; if > later times will be first
    }
};

std::vector<evt> spillData1;                                     // vector that holds spill events sorted time
/**************************************************************************/
int main(int argc, char **argv) {
    struct stat sb;             // file status structure for fstat
    int ifile=0, ofile=0;
    char oname[190]="\0";      // name of file to open
    long time_x=0;
/*
     Input LDF-equivalent file on command line
*/
    if (argc == 2) {
        strcpy(oname,argv[1]);
    }
    else {
        printf(" No LDF file defined....input file name:\n");
        scanf ("%s",oname);
    }
    ifile=openFileInp(oname);
    fstat(ifile,&sb);
    intFilesize = sb.st_size/sizeof(int);
    printf ("file size in ints = %i\n",intFilesize);      // file is open and size determined
    strcat(oname,".tol");
    ofile=openFileOut(oname);
    fstat (ofile,&sb);
    printf ("file size in ints = %llu\n",sb.st_size/sizeof(int));      // file is open and size determined

    time_x = time(NULL);
    printf("Starting at time ...%li %lx \n",time_x, time_x);
    //    pixie16NSCL();
    
    pixie16ORNL();

    closeFileInp(ifile);
    closeFileOut(ofile);
    printf ("wcount=%li   scount=%li   count=%li  rcount=%i \n", wcount,scount,count,rcount);
    printf ("missing data = %i\n",(((intFilesize-(scount*16))/4) - count));
    return EXIT_SUCCESS;
}

/**************************************************************/
void pixie16ORNL() {
/*
  Function to read and sort data taken in ORNL/UTK "new native data" with memory mapping
*/
  int ii=0, jj=0, kk=0, nn=0;
  int S='S', P='P', I='I', L='L', T='T', A='A';
  unsigned int wallClock=0;
  unsigned int lenRec=0;
  unsigned int lenUser=0, lenPix=0;
  unsigned int modData[2000000]; // SPILLDATA_MAX=1000000;
  unsigned int usrData[1024];    // User data = 1024
  unsigned int lenSpill=0;
  int lt0, lt1, lt2, lt3, ltx;
  int pos=0;
  int bytesmissed=0;

  lt0 = 2 * sizeof(uint64_t);  // size of 1 128 bit boundary
  lt1 = 2 * lt0;               // size of 2 128-bit boundary
  lt2 = 3 * lt0;               // size of 3 128-bit boundary
  lt3 = 4 * lt0;               // size of 4 128-bit boundary
  //  printf (" sizes %i %i %i %i \n",lt0,lt1,lt2,lt3);
  printf ("intFilesize = %i\n",intFilesize);
  printf ("events based on size = %i\n",((intFilesize-(197*16))/4));
  ii=0;
  while (ii < intFilesize-1) {    // step through each element of the file
    /*
    printf ("spil0 = %u, %u, %u, %u \n",xxxptr[ii], xxxptr[ii+1], xxxptr[ii+2], xxxptr[ii+3]);
    printf ("spil1 = %u, %u, %u, %u \n",xxxptr[ii+4], xxxptr[ii+5], xxxptr[ii+6], xxxptr[ii+7]);
    printf ("spil2 = %u, %u, %u, %u \n",xxxptr[ii+8], xxxptr[ii+9], xxxptr[ii+10], xxxptr[ii+11]);
    printf ("spil3 = %u, %u, %u, %u \n",xxxptr[ii+12], xxxptr[ii+13], xxxptr[ii+14], xxxptr[ii+15]);
    printf ("spil4 = %u, %u, %u, %u \n",xxxptr[ii+16], xxxptr[ii+17], xxxptr[ii+18], xxxptr[ii+19]);
    */
    if (xxxptr[ii]==S && xxxptr[ii+1]==P && xxxptr[ii+2]==I && xxxptr[ii+3]==L){
      spillNum  = xxxptr[ii+4];                                    // spill number in file
      lenSpill  = xxxptr[ii+5] - ii;   // - (ii+8);          // next spill location - this spill location - ii = length of spill
      wallClock = xxxptr[ii+6];                                   // time of day since computer epoch 1/1/1970 in seconds
      lenUser   = xxxptr[ii+7];
      if (lenUser == 0) {
	lenPix = xxxptr[ii+8];
	memcpy(&modData,&xxxptr[ii+8],sizeof(int)*lenPix);   // copy 1 spill into modData to send to one of the hissubs
      }
      else {
	lenPix = xxxptr[ii + 7 + xxxptr[ii+7]];                      // start of buffer + length of user data + 7 
	memcpy(&usrData,&xxxptr[ii+7],sizeof(int)*lenUser);          // copy 1 spill of user data into usrData to process
	memcpy(&modData,&xxxptr[ii+7+lenUser],sizeof(int)*lenPix);   // copy 1 spill into modData to send to one of the hissubs
	// call user data processor
	// user data has format: length, time read, data encoded id,data in one uint32
      }
      //      printf ("usrdata: %u, %u, %u, %u, %u, %u, %u %u %u %u\n",usrData[0], usrData[1], usrData[2], usrData[3],usrData[4],usrData[5],usrData[6],usrData[7],usrData[8],xxxptr[ii + 7 + xxxptr[ii+7]]);
      if (lenPix < SPILLDATA_MAX){                         // buffer size is 2,000,000
      //	memcpy(&modData,&xxxptr[ii+8],sizeof(int)*lenPix);   // copy 1 spill into modData to send to one of the hissubs
//
	userStruc(usrData);                                          // process user data
	jj = 0;
	kk = 0;
	if (lenSpill < (lenUser + lenPix)) {
	  printf ("Substituting lenSpill for lenPix !!!!!!!!!!!!!!  %u  %u  %u  %u \n",modData[0], lenSpill, lenUser, lenPix);
	  modData[0] = lenSpill - 8;
	  bytesmissed += (lenUser + lenPix) - lenSpill;
	  if (lenUser > 0) modData[0] = lenSpill - lenUser - 7;
	  printf ("----------------------------------------------------------------missing bytes: %i\n", bytesmissed);
	} else if (lenSpill > (lenUser + lenPix + 7)){
	  rcount += lenSpill-(lenUser + lenPix);
	  printf ("----------------------------------------------------------------extra bytes: %i  %i %i\n",(lenUser + lenPix),lenSpill, rcount);
	}
	/**/	
	while (jj< lenPix) {
	  if (modData[jj+1] < maxVSN || modData[jj+1] > -1) {
	    buildStruc(modData,lenSpill);  // identify parts of data for own sort possibilities...modData[jj] is length/VSN of module data
	    scount++;
	  } else {
	    wcount++;
	  }	  
	  jj+=modData[jj];                                                // increment jj until lenPix reached
	}                                                                 // data in spill gathered.
	std::sort(spillData1.begin(), spillData1.end(), by_time1());      // now sort vector in time
	nn = spillData1.size();                                       // determine number of elements in vector
	printf ("size of tol spill %i = %i with lenPix = data = %i diff =%i \n",spillNum,nn,lenPix/4,(nn-lenPix/4));
	
	if (lenUser == 0) kk = 8;
	else kk = 7 + lenUser;
	memcpy(&tolptr[pos],&xxxptr[ii],kk*4);      // 128 bit copy raw data header and user-supplied data
	pos+= kk/2;                                 // defined at uint64_t
	
	for (int mm=0; mm<nn; mm++){                                      // write out each element of the time ordered vector
	  //	  if (spillNum == 197 && spillData1[mm].energy ==0) printf("mm = %u, maxVSN = %u, \n",mm,spillData1[mm].status);
	  uint16_t test = spillData1[mm].status & 0x0007;              // check status word to find energy/sum/qdc header size
	  ltx = spillData1[mm].lent* sizeof(uint16_t);                 // get size of trace...together we know the size of the event
	  switch (test){
	  case 0:                                                  // write out 128 bits of time, id, status, energy, trace length
	    memcpy(&tolptr[pos],&spillData1[mm],lt0);            // 128 bit
	    pos += lt0/8;                                        // defined at uint64_t
	    break;
	  case 1:                                                  // write out 128 bits + 4 sum's
	    memcpy(&tolptr[pos],&spillData1[mm],lt1);            // 128 bit
	    pos += lt1/8;
	    break;
	  case 2:                                                  // write out 128 bits + 8 qdc's
	    memcpy(&tolptr[pos],&spillData1[mm],lt0);            // 128 bit
	    pos += lt0/8;
	    memcpy(&tolptr[pos],&spillData1[mm].qdc,lt1);        // 128 bit  since I need to skip the sums
	    pos += lt1/8;
	    break;
	  case 3:                                                  // write out 128 bits + 4 sum's + 8 qdc's
	    memcpy(&tolptr[pos],&spillData1[mm],lt3);            // 128 bit
	    pos += lt3/8;
	    break;
	  case 4:                                                  // write out 128 bits + trace
	    memcpy(&tolptr[pos],&spillData1[mm],lt0);            // 128 bit
	    pos += lt0/8;
	    memcpy(&tolptr[pos],&spillData1[mm].trace,ltx);      // since I need to skip sum's and qdc's
	    pos += ltx/8;
	    break;
	  case 5:                                                  // write out 128 bits + trace + sum
	    memcpy(&tolptr[pos],&spillData1[mm],lt1);            // 128 bit
	    pos += lt1/8;
	    memcpy(&tolptr[pos],&spillData1[mm].trace,ltx);      // since I need to skip qdc's
	    pos += ltx/8;
	    break;
	  case 6:                                                  // write out 128 bits + trace + qdc
	    memcpy(&tolptr[pos],&spillData1[mm],lt0);            // 128 bit
	    pos += lt0/8;
	    memcpy(&tolptr[pos],&spillData1[mm].qdc,lt1+ltx);    // 128 bit  since I need to skip the sums
	    pos += lt1/8+ltx/8;
	    break;
	  case 7:                                                  // write out 128 bits + trace + qdc + sum
	    memcpy(&tolptr[pos],&spillData1[mm],lt3+ltx);        // all of evt structure
	    pos += lt3/8+ltx/8;
	    break;	    
	  default:
	    printf ("\n Something wrong...can't copy data to tolptr test =%i \n",test);
	  }
	}
	spillData1.clear();                            // spill data written to file...now clear spill vector for reloading
      }
      else {
	printf ("\n Something wrong...spill data size longer than %u \n",SPILLDATA_MAX);
      }
      /**/
      ii=xxxptr[ii+5];                                            // next spill location
    }
    else if (xxxptr[ii]=='S' && xxxptr[ii+1]=='T' && xxxptr[ii+2]=='A' && xxxptr[ii+3]=='T'){
      printf("....................Stat data \n");
      ii=xxxptr[ii+5];                           // next spill location
    }
    else {
      printf(".............................................Data error? \n");
      ii++;
    }
  }
  printf ("spill = %u pos = %i \n",spillNum, pos);
  
  return;
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
    fd = mmapListDataInp(infile);     // call file open and map the array pointer xxxptr
    
    return (fd);
}
/**************************************************************/
int openFileOut(char *oname) {
    int fd;
    char outfile[200]="\0";
    /*
     Function to open and map list mode output file
     See poll for how file names auto incremented and limied to 1 GB in size
     */
    strcpy(outfile,oname);  // require full name to allow skipping already processed files
    fd = mmapListDataOut(outfile);     // call file open and map the array pointer xxxptr
    
    return (fd);
}

/**************************************************************/
void closeFileInp(int ifile) {
    /*
     Function to unmap and close list mode output file
     See poll for how file is opened, setup, truncated at end and closed
     */
    munmap(xxxptr,sizeof (*xxxptr));
    close(ifile);                                  // close in file
    printf("Input file unmapped and closed...\n");
    
    return;
}

/**************************************************************/
void closeFileOut(int ofile) {
  int kk=0;
/*
     Function to unmap and close list mode output file
    See poll for how file is opened, setup, truncated at end and closed
*/
//  int nn = ((sizeof(uint64_t)) * LISTDATA_SIZE);
    munmap(tolptr,sizeof (*tolptr));
    //    kk = ftruncate(ofile,(nn * sizeof(uint64_t)));  // truncate to end of last data
    //    if (kk == -1) perror("Error truncating list file at closing");
    close(ofile);                                  // close out file
    printf("Output file unmapped and closed...\n");
    
    return;
}

/**************************************************************/
int mmapListDataInp(char *infile) {
/*
    Function to open and memory map the list mode file
*/
    int fd=0;                // mapped file descriptor
    ssize_t result=0;
    
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
/*
     Now the file is ready to be mmapped.
*/
//	xxxptr = (int*) mmap(0, LISTDATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    xxxptr = (int*) mmap(0, fsize, PROT_READ, MAP_SHARED, fd, 0);          // cannot write to file (I hope!)
    //    xxxptr = (int*) mmap(0, LISTDATA_SIZE, PROT_READ, MAP_SHARED, fd, 0);          // cannot write to file (I hope!)
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

/**************************************************************/
int mmapListDataOut(char *outfile) {
    /*
     Function to open and memory map the list mode file
     */
    int fd=0;                // mapped file descriptor
    ssize_t result=0;
    off_t fileSize=0;
    
    printf("Setting up memory mapped file: %s\n", outfile);
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
    fd = open(outfile, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, (mode_t)0644);
    if (fd == -1) {
        perror("Error opening file path for writing");
        exit(EXIT_FAILURE);
    }
    //    fileSize = lseek(fd, 4*(intFilesize), SEEK_END);
    fileSize = lseek(fd, fsize-1, 0);//SEEK_END);
    if (fileSize == -1) {
        perror("Error establishing the necessary output tol file");
        exit(EXIT_FAILURE);
    }

    //    for (int ii=0; ii< 4*10000000; ii++){
    result = write (fd,"\0",1);
    if (result != 1){
        close(fd);
        perror("Error writing last byte of file");
    }
    
    /*
     Now the file is ready to be mmapped.
     */
    tolptr = (uint64_t*) mmap(0, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    //    tolptr = (uint64_t*) mmap(0, LISTDATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//    xxxptr = (int*) mmap(0, LISTDATA_SIZE, PROT_READ, MAP_SHARED, fd, 0);          // cannot write to file (I hope!)
    if (tolptr == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the output file");
        exit(EXIT_FAILURE);
    }
    
    /*
     Don't forget to free the mmapped memory usually at end of main
     */
    return (fd);
}

/**************************************************************/
//void buildStruc(unsigned int max, unsigned int vsn){
void buildStruc(unsigned int modData[],unsigned int lenSpill){
/*
 Function that decodes raw pixie16 data (Rev D) and puts it into a structure of type event
 */
  int kk=0, mm=0, nn=0;
  int max=0, vsn=-1;
  uint16_t bit128=0;
  uint64_t mhz100=10000;   // convert 100MHz  (10 ns step) to picoseconds
  uint64_t mhz250=4000;    // convert 250MHz  ( 4 ns step) to picoseconds
  uint64_t mhz500=2000;    // convert 500MHz  ( 2 ns step) to picoseconds
  uint64_t mhz1000=1000;   // convert 1000MHz ( 1 ns step) to picoseconds
  
  union int32int64 {
    uint64_t timl;           // 64-bit clock
    uint32_t tims[2];        // 32-bit clock parts from pixie
  } xt;
  uint32_t x0, x1, x2, x3;
  uint64_t tmax=0, tmin = 0xFFFFFFFFFFFFFFFF;
  uint16_t pileup, len, lenh, lent, crate, slot, chan, id, cfd, energy;
  struct evt n;
                                      //  spil = 83, 80, 73, 76
  tmax=0;
  tmin=0xFFFFFFFFFFFFFFFF;
  kk=2;                        // skipping poll added length [0] and VSN [1]
  max=modData[0];
  //  if (lenSpill < max) max = lenSpill;
  vsn=modData[1];
  while (kk < max) {         // copy data into variables to decode
    x0 = modData[kk];     // module information and pile-up bit
    x1 = modData[kk+1];   // low 32-bit of clock
    x2 = modData[kk+2];   // high 16 bit of clock and CFD
    x3 = modData[kk+3];   // trace length and energy
    xt.tims[0] = x1;
    xt.tims[1] = x2 & 0x0000FFFF;
//         m.tim   = xt.timl;                    // set clock to 48-bit
    pileup = (x0 & 0x80000000) >> 31;   // bit 31
    len    = (x0 & 0x3FFE0000) >> 17;   // bit 17:30
    lenh   = (x0 & 0x0001F000) >> 12;   // bit 12:16
    crate  = (x0 & 0x00000F00) >> 8;    // bit 8:11
    slot   = (x0 & 0x000000F0) >> 4;    // bit 4:7
    chan   = (x0 & 0x0000000F);         // bit 0:3
    cfd    = (x2 & 0xFFFF0000) >> 16;   // bit 16:31
    energy = (x3 & 0x0000FFFF);         // bit 0:15
    lent   = (x3 & 0x3FFF0000) >> 16;   // bit 16:29

    if (lent != 0) {
      printf("kk = %i   len of trace: %u : %u %u %u  %u  %u   %u \n",kk, len, lenh, lent, x0, x1, x2, x3);
      //      printf("kk = %i   len= %i lenh=%i %lent=i x's=%i  %i  %i   %i \n",kk, lent, max, vsn, x0, x1, x2, x3);
      printf(" Non-zero trace \n");
      printf ("kk= %i, modData[0] = %u max = %u \n",kk,modData[0], max);
      printf ("modData[kk] = %u max = %u \n",modData[0], max);
      //      return;
    }
    if (spillNum == 197 && energy == 0) { // when module has no data we put in length and module number plus may be some 0 padding
      //      for (mm=-20; mm< 21; mm++) printf ("index = %i  => %x \n",kk+mm,modData[kk+mm]);
      cnt0++;
      //      printf("kk = %i   len: %i lenh: %i lent=%i crate=%i slot=%i chan=%i  cfd=%i  pile=%i   %llu %i\n",kk, len, lenh, lent, crate, slot, chan, cfd, pileup,xt.timl,cnt0);
      //           sleep(1);
    }
    /*
    if (spillNum==116 && kk > 57100) {
      printf("kk = %i   len of trace: %i : %i %i %i %i %i  %i  %i   %i \n",kk, len, lenh, lent, max, vsn, x0, x1, x2, x3);
      printf ("modData[kk] = %u max = %u \n",modData[0], max);
      //      sleep(1);
      // return;
    }
    */

    n.ps = xt.timl * mhz100 + (cfd / 65536 * mhz100);     // time in ps
    n.id = chan + slot * 16 + crate * 256;                // unique ID based on crate, slot, channel of module
    n.energy = energy;                                    // energy
    if (lenh == 4) n.status = 0;
    else if (lenh == 8) n.status = 1;
    else if (lenh == 12) n.status = 2;
    else n.status = 3;                                    // m.lenh = 16
    if (pileup == 1) n.status = n.status + 8;
    if (lent != 0) {
      n.status += 4;
      bit128 = lent % 8;               // test if trace ends on 128-bit boundary
      n.lent = lent + (8 - bit128);
    }
    else n.lent = lent;                  // processed data length
    lent   = lent/2;                     // number of ints that contain 2 data points each of the trace for read
    
    if (lenh != 4) printf("lenh not 4!\n");
    switch (lenh) {                             // process optional header data
    case 4:                            // always there so needed just to prove no msidentified header at default: below
      break;
    case 8:
      for (nn=0; nn<4; nn++){
	n.sum[nn] = modData[kk+4+nn];   // energy sums 0=trailing, 1= leading, 2=gap, 3=baseline
      }
      printf("8 header identified\n");
      break;
    case 12:
      for (nn=0; nn<8; nn++){
	n.qdc[nn] = modData[kk+4+nn];   // QDC sum 1-8
      }
      printf("12 header identified\n");
      break;
    case 16:
      for (nn=0; nn<4; nn++){
	n.sum[nn] = modData[kk+4+nn];   // energy sums 0=trailing, 1= leading, 2=gap, 3=baseline
      }
      for (nn=0; nn<8; nn++){
	n.qdc[nn] = modData[kk+4+nn];   // QDC sum 1-8
      }
      printf("16 header identified\n");
      break;
    default:
      rcount++;
      //      printf("No header identified\n");
      break;
    }
    kk += lenh;
    //    if (lent != 0) return;
    /* not processing traces*/
    if (lent != 0) {                              // process trace if present
      printf ("In found a trace: length: %i!\n",lent);
      nn = 0;
      for (mm=0; mm< lent; mm++){
	nn = mm*2;
	n.trace[nn]   = (modData[kk+mm] & 0x7FFF);                       // bit 0:15   allow for 15-bit traces
	n.trace[nn+1] = (modData[kk+mm] & 0x7FFF0000) >> 16;             // bit 16:30
        
      }
      if (bit128 != 0) {
	printf (" bit128 = %i\n",bit128);
	for (mm=0; mm < bit128; mm++){
	  n.trace[nn+2+mm]=0;          // pad the trace to end on a 128-bit boundary
	}
      }
    }
    kk += lent;                           // increment variable to end of module data
    //    printf ("kk = %i, max = %i\n",kk,max);
    /**/
    spillData1.push_back(n);                 // loads event data into vector SpillData
    count++;
  }
  
// sorting by time done in main routine after all modules in the spill have been processed
  
  return;
}

/**************************************************************/

/**************************************************************
void pixie16NSCL() {
    
//     Function to read and sort data taken in NSCL format
     
    union int4int8 {
        unsigned long timl;           // 64-bit clock
        unsigned int  tims[2];        // 32-bit clock parts from pixie
    } xt;
    int ii=0, jj=0, kk=0, mm=0, nn=0;
    int lenRec=0;
    //    unsigned int RS=0x1E; // 30 in int32 format
    long lenEvt = 0, nextEvt=0, timeEvt=0;
    struct event m;              // vector and structure to be added to vector must be defined outside function
    unsigned int x0=0, x1=0,x2=0,x3=0;
    int flag=0;
    
    ii=0;
    while (ii < intFilesize-1) {
        lenEvt = (long) xxxptr[ii]/4;
        //        if (mod(xxxptr[ii]/4) != 0) flag =1;
        //        else flag=0;
        nextEvt= (long) ii+lenEvt;
        ii++;
        if (xxxptr[ii]== 30) {            // 30 int32 format equals "physics data"
            printf ("Found a 30: ii = %li    %u   %u   %u \n",ii, xxxptr[ii-1], xxxptr[ii],xxxptr[ii+1]);
            m.lent=0;
            jj = ii + 10;            // set ii to first pixie16 header word
            x0 = xxxptr[jj];     // 1st of header => event length, header length, module info
            x1 = xxxptr[++jj];   // 2nd of header => lower bit of clock
            x2 = xxxptr[++jj];   // 3rd of header => CFD and upper 16 bit of clock
            x3 = xxxptr[++jj];   // 4th of header => trace length and energy
            printf (" %u   %u   %u    %u \n", x0, x1, x2, x3);
            m.pileup = (x0 & 0x80000000) >> 31;   // bit 31
            m.len    = (x0 & 0x7FFE0000) >> 17;   // bit 17:30
            m.lenh   = (x0 & 0x0001F000) >> 12;   // bit 12:16
            m.crate  = (x0 & 0x00000F00) >> 8;    // bit 8:11
            m.slot   = (x0 & 0x000000F0) >> 4;    // bit 4:7
            m.chan   = (x0 & 0x0000000F);         // bit 0:3
            m.cfd    = (x2 & 0xFFFF0000) >> 16;   // bit 16:31
            m.energy = (x3 & 0x0000FFFF);         // bit 0:15
            m.lent   = (x3 & 0xFFFF0000) >> 16;   // bit 16:31
            m.lent   = m.lent/2;                  // number of ints that contain 2 data points each of the trace
            xt.tims[0] = x1;                  //xxxptr[++ii];
            xt.tims[1] = x2 & 0x0000FFFF;     //xxxptr[++ii];
            timeEvt =xt.timl;
            m.evtTime = timeEvt;
            
            if (m.lent != 0) {                              // process trace if present
                nn = 0;
                jj++;
                for (mm=0; mm< m.lent; mm++){
                    nn = mm*2;
                    m.trace[nn]   = (xxxptr[jj+mm] & 0x00000FFF);                       // bit 0:11
                    m.trace[nn+1] = (xxxptr[jj+mm] & 0x0FFF0000) >> 16;             // bit 16:27
                }
                jj += m.lent;
                printf ("found a trace and copied it for %i ints\n",m.lent);
            }
            
            while (++jj < nextEvt) {
                jj += 8;
                x0 = xxxptr[jj];     // 1st of header => event length, header length, module info
                x1 = xxxptr[++jj];   // 2nd of header => lower bit of clock
                x2 = xxxptr[++jj];   // 3rd of header => CFD and upper 16 bit of clock
                x3 = xxxptr[++jj];   // 4th of header => trace length and energy
                printf (" %u   %u   %u    %u \n", x0, x1, x2, x3);
            }
            

        }
        else if (xxxptr[ii]== 20) {
            printf ("Found a 20: next -1 = %li next %li   next +1 = %li \n",xxxptr[ii-1],xxxptr[ii], xxxptr[ii+1]);
        }
        else if (xxxptr[ii]== 31) {
            printf ("Found a 31: next -1 = %li next %li   next +1 = %li \n",xxxptr[ii-1],xxxptr[ii], xxxptr[ii+1]);
        }
        else if (xxxptr[ii]== 11) {
            for (kk=-330; kk< 300; kk++){
                printf(" Near 11: kk=%i xxxptr = %li\n", kk,xxxptr[ii+kk]);
            }
            
 //            printf ("Found a 11: next-1 = %li next %li   next+1 = %li \n",xxxptr[ii-1],xxxptr[ii], xxxptr[ii+1]);
 //            printf ("          : next+2 = %li next+3 %li   next+4 = %li \n",xxxptr[ii+2],xxxptr[ii+3], xxxptr[ii+4]);
             
            lenEvt = (long) xxxptr[ii+1];   // not divided by 4
            nextEvt= (long) ii+lenEvt;
            
            //            ii = intFilesize-1;
        }
        else {
            //            if (ii> 8000000) printf ("Searching....%li => %u    %x   \n",ii, xxxptr[ii],xxxptr[ii]);
            //            else printf ("lost....%li => %u \n",ii, xxxptr[ii]);
            printf ("lost....%li => %u \n",ii, xxxptr[ii]);
            //            ii++;
        }
        
        ii = nextEvt;// +1;
    }
    
    return;
}
*/
/**************************************************************/
//void buildStruc(unsigned int max, unsigned int vsn){
void userStruc(unsigned int usrData[]){
  int ii=0, jj=0, kk=0, mm=0;
  union bit16bit32 {
    unsigned int valu;                 // 32-bit computer clock
    unsigned short int s[2];        // two 16-bit computer clock
  } x;
  unsigned int id=0, dat=0, time1=0;
  unsigned int kelvin[10],geln[10], gelntime=0, kelvintime=0;
  
  mm = usrData[0];                // length of user data block
  kk = usrData[1];                // length of first data block
  time1 = usrData[2];             // time of first user data block
  ii = 3;
  while (ii < mm) {
    x.valu = usrData[ii];         // put in union to get ID, Data
    id  = x.s[1];               // 0xffff0000 = parameter id
    dat = x.s[0];               // 0x0000ffff = user data value
    ii++;
    if (id > 20 && id < 30) {
      kelvin[id - 21] = (unsigned int) dat;
      kelvintime = time1;
    }
    if (id > 30 && id < 40) {
      geln[id - 31] = (unsigned int) dat;
      gelntime = time1;
    }
    if (ii > kk && ii != mm){
      kk = usrData[ii++];       // next user data block
      time1 = usrData[ii++];    // next user data time
    }
  }

  //  for (jj=0;jj<4;jj++) printf("id = %u  data = %u  \n",jj,kelvin[jj]);

    
  return;
}
/**************************************************************/
