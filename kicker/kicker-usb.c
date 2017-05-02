/******************************************************************/
/* Program kicker is used to control the BNC-632 Waveform         */
/*    Generator to control the beam to LeRIBSS                    */
/*                                                                */
/* To be compiled with: gcc -lm -o kicker kicker.c                */
/*                                                                */
/* C. J. Gross, February 2009                                     */
/******************************************************************/

#include <stddef.h>
#include <limits.h>
#include <string.h>  /* String function definitions */
#include <float.h>
#include <ctype.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <stdlib.h>
#include <math.h>    /* Math definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <signal.h>  /* Signal interrupt definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <errno.h>   /* Error number definitions */
#include <fcntl.h>   /* File control definitions */
#include <time.h>    /* Time definitions */
#include <sys/select.h>
#include <sys/time.h>

typedef void (*sig_t) (int);
sig_t signal(int sig, sig_t func);

long int flag;
char *GetDate(void);
void MotorRun(long int loop, float duration, float motor);
void Looper();
void SignalHandle(int sig);

long int writeport(int fd, char *data);
long int writeport2(int fd, char data[]);
long int readport(int fd, char *data);
long int readport100(int fd, char data[]);
long int getbaud(int fd);
long int setbaud(int baud);
long int SerialOpen();
long int PortWriteBin(int fd, unsigned short int databin[]);

long int BNCMode();
long int BNCIO(long int command);
long int BNCIO2(long int command, long int ii);
long int BNCFunction();
long int BNCSine();
long int BNCResetSine();  // manual recommends initializing BNC-632 here (sets offset to 0 V)
long int BNCPulse();
long int BNCArbitrary();
long int BNCStatus();
long int BNCFrontOff();
long int BNCFrontOn();
long int BNCFrontOnOff();
long int BNCLCDEchoOn();
long int BNCLCDEchoOff();
long int BNCLCDEchoOnOff();
long int BNC_0();
long int BNC_1();
long int BNC_2();
long int BNC_3();
long int BNC_4();
long int BNC_5();
long int BNC_6();
long int BNC_7();
long int BNC_8();
long int BNC_9();
long int BNCMHz();
long int BNCkHz();
long int BNCHz();
long int BNCOffset();
long int BNCTrigger();
long int BNCDot();
long int BNCMinus();
long int BNCField();
long int BNCFieldLeft();
long int BNCFieldRight();
long int BNCDigitUp();
long int BNCDigitDown();
long int BNCDigitLeft();
long int BNCDigitRight();
long int Manual();
long int PulseF1();
long int PulseF2(long int duty);
long int PulseF3(long int trig);
long int PulseF4(double freq);
long int PulseF5();
long int ArbF3();
long int BNCParameters();
long int BNCDigital();
long int BNCDC(long int ii);
long int BNCWaveform();
long int BNCWaveformUpload(long int ii, long int wave[]);
long int BNCWaveformUploadBin(long int ii, unsigned short int wavebin[]);
long int BNCWaveFormatDigital();
long int BNCTerminate();
//long int Offset();
long int GrowDecay();
long int IntegerDigits(long int mm);
long int DoubleDigits(double xx);
char BNCCommand(long int command);
void Help();
long int MTCMovement();
void WaveformDiagram();
void SetupParameters();
long int BNCWaveformCreate();
long int Menu(long int panel);

long int amplitude=0, offset=0;
int fd;
long int baud;
long int mtc=200;
long int flagfrontpanel=0;
long int igrow=100,idecay=100,trig=1,cycles=1;
double xclock;
long int wave[16384];
unsigned short int wavebin[16384];
static long int max=16384;

/******************************************************************/
int main () {
  char com1;
  long int ans=0;
  long int ii=0,jj,kk;
  long int stat;
  long int onoff[max];
  char ESC=27;

  printf ("Welcome to program kicker \n\n");
  printf ("  This program remotely controls a BNC-632 Waveform Generator via RS-232 \n");
  printf ("    - set BNC-632 baudrate to 38400 by selecting Mode-Offset-Knob rotate \n\n");
  printf ("  You will need the following information \n");
  printf ("    - time duration of grow-in   (beam  on) \n");
  printf ("    - time duration of decay-out (beam off) \n");
  printf ("    - time duration of MTC movement \n");
  printf ("    - number of grow-in/decay-out sycles per MTC movement \n");
  printf ("    - is the MTC master or slave to this program\n");
  //  printf ("    - control-c is used to escape out of any loops \n");
  //  printf ("    - control-\\ is used to emergency quit the program \n");
  printf (" \n ");

/*
Serial port opened and settings set to:
  8-bit, no parity, 1 stop bit, baud rate 9600
*/	
  printf (" Opening serial port ...    ");
  ii=SerialOpen();
  if (ii==1) exit (EXIT_FAILURE);  // end on failed open of serial port
  printf (" ... successful !\n ");
  printf (" Check status of BNC-632 generator ... \n");
	
  flagfrontpanel=BNCFrontOff();  // Turn off front panel
  //  BNCIO(30);                     // Reset device in to known state (sine wave)
  BNCStatus(); 

  printf ("----------------------------------------------------------------------------\n");
  amplitude=1900;
  offset=amplitude/2;
  SetupParameters();
  BNCWaveformCreate();  // create waveform of max=16384 points based on grow-in/decay-out/cycles/mtc move times
  BNCDigital();         // set up the BNC for beam deflection

  while (flag==0){
    signal(SIGHUP,SignalHandle);
    printf ("----------------------------------------------------------------------------\n");
    WaveformDiagram();

    ans = Menu(flagfrontpanel);
    if (0 > ans || ans > 17) {
	printf("Number out of range \n");
        ans = 99;
    }      
    switch ( ans ) { 
    case  1: 
      BNCDigital();
      break;
    case  2: 
      printf ("----------------------------------------------------------------------------\n");
      SetupParameters();
      BNCWaveformCreate();  // create waveform of max=16384 points based on grow-in/decay-out/cycles/mtc move times
      printf("%c[1m",ESC);  // turn on bold face
      printf("\n *** Type 1 to implement new setup *** \a \n"); // \a = bell
      printf("%c[0m",ESC);  // turn off bold face
      break;
    case  3: 
      printf("max = %li\n",max);
      for (ii=0; ii<max; ii++) onoff[ii]=0;
      BNCWaveformUpload(max,onoff);
      printf("\nSettings completed \n");
      break;
    case  4: 
      printf("max = %li\n",max);
      for (ii=0; ii<max; ii++) onoff[ii]=1;
      BNCWaveformUpload(max,onoff);
      printf("\nSettings completed \n");
      break;
    case  5: BNCStatus();  break;
    case  6: break;  //GrowDecay();  break;

    case 11: BNCTrigger(); break;
    case 12: flagfrontpanel=BNCFrontOn();  break;
    case 13: flagfrontpanel=BNCFrontOff();  break;
    case 14: BNCParameters(); break;
    case 15: kk = Manual(); break;
    case 16: MTCMovement(); break;
    case 10: Help(); break;
    case 0:
      printf("Kicker ending ....\n");
      close(fd);
      exit (EXIT_FAILURE);
    default:
      break;
    }
  }

  return 0;
}


/******************************************************************/
void SignalHandle(int sig) {

  printf("Sig = %i\n",sig);
  if (sig == 2) flag = 2;
  if (sig == 20) printf("reseting signals?\n");

  return;
}

/******************************************************************/
long int initport(int fd) {
  struct termios options;
  // Get the current options for the port...
  tcgetattr(fd, &options);
/*
  printf ("Original serial port settings: \n");
  printf ("Serial port configuration c_cflag = %x \n",options.c_cflag);
  printf ("Serial port configuration c_iflag = %x \n",options.c_iflag);
  printf ("Serial port configuration c_oflag = %x \n",options.c_oflag);
  printf ("Serial port configuration c_lflag = %x \n",options.c_lflag);
*/
  /*
  cfmakeraw disables all input/output processing to raw I/O.  effectively
  this zeroes c_iflag, c_oflag, c_lflag in the structure options which sets 
  up the serial port.  I had trouble restarting this program after the
  computer was rebooted after a power outage.  I needed to add these 
  lines with cfmakeraw and checking c_iflag, oflag, and lflag.  Minicom
  was used to help debug the problem as the program could run with minicom
  configuring the port.
  */
  cfmakeraw(&options);
  /*
  options.c_iflag=1;
  options.c_oflag=0;
  options.c_lflag=0;
  */

  // Set the baud rates to 9600...*/
  //cfsetispeed(&options, B9600);
  //cfsetospeed(&options, B9600);
   cfsetispeed(&options, B38400);
   cfsetospeed(&options, B38400);
   //  cfsetispeed(&options, B115200);  too fast for BNC-632!
   //  cfsetospeed(&options, B115200);  too fast for BNC-632!
  // Enable the receiver and set local mode (ignore modem status lines)...
  options.c_cflag |= (CLOCAL | CREAD);
  // below are bit-wise operations; complement sign  ~ 
  options.c_cflag &= ~PARENB;  /* disable parity*/
  options.c_cflag &= ~CSTOPB;  /* disable 2 stop bit (thi chooses 1 stop bit)*/
  options.c_cflag &= ~CSIZE;   /* disable character mask size*/
  options.c_cflag |= CS8;      /* set 8 bits */
  options.c_cflag &= ~CRTSCTS; /* disables hardware flow control */


  // Set the new options for the port...
  tcsetattr(fd, TCSANOW, &options);  //TCSANOW = change immediately
  tcgetattr(fd, &options);
/*
  printf ("Flip-flop's serial port settings: \n");
  printf ("Serial port configuration c_cflag = %x \n",options.c_cflag);
  printf ("Serial port configuration c_iflag = %x \n",options.c_iflag);
  printf ("Serial port configuration c_oflag = %x \n",options.c_oflag);
  printf ("Serial port configuration c_lflag = %x \n",options.c_lflag);
*/
  return 1;
}

/******************************************************************/
long int getbaud(int fd) {
  struct termios termAttr;
  long int inputSpeed = -1;
  speed_t baudRate;
  tcgetattr(fd, &termAttr);
  /* Get the input speed.                              */
  baudRate = cfgetispeed(&termAttr);
  switch (baudRate) {
  case B0:      inputSpeed =      0; break;
  case B50:     inputSpeed =     50; break;
  case B110:    inputSpeed =    110; break;
  case B134:    inputSpeed =    134; break;
  case B150:    inputSpeed =    150; break;
  case B200:    inputSpeed =    200; break;
  case B300:    inputSpeed =    300; break;
  case B600:    inputSpeed =    600; break;
  case B1200:   inputSpeed =   1200; break;
  case B1800:   inputSpeed =   1800; break;
  case B2400:   inputSpeed =   2400; break;
  case B4800:   inputSpeed =   4800; break;
  case B9600:   inputSpeed =   9600; break;
  case B19200:  inputSpeed =  19200; break;
  case B38400:  inputSpeed =  38400; break;
  case B57600:  inputSpeed =  57600; break;
  case B115200: inputSpeed = 115200; break;
  }
  return inputSpeed;
}

/*****************************************************************/
long int SerialOpen(){
  long int xx=0;
  //  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
  fd = open("/dev/tty.usbserial", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    perror("\nopen_port: Unable to open /dev/ttyUSB0 - \n");
    perror("\nopen_port: Unable to open /dev/tty.usbserial - \n");
    return 1;
  } else {
    fcntl(fd, F_SETFL, 0);
  }
  
  //  printf("baud=%d\n", getbaud(fd));
  xx = getbaud(fd);
  printf (" initial baud rate is %li  will try to initialize port \n",xx);
  initport(fd);
  getbaud(fd);
  printf(" baud=%d .... ", getbaud(fd));
  return;
}

/******************************************************************/
long int portflush(int fd) {
  long int len=1, n=0;
  
  n = 1;
  while (n != 0){
    n = tcflush(fd,TCIFLUSH);
  }
  //  if (n == 0) printf("successful buffer flush \n");
  
  return 1;
}

/******************************************************************/
long int portwrite1(int fd, char *data) {
  long int len=1, n=0;

/*  removed sleep option to speed up large data writes such as waveform data */
  n = write(fd, data, len);
  //  usleep(500000);   // add a pause (0.5 sec) to allow module to process/respond
  if (n < 0) {
    fputs("write failed!\n", stderr);
    return 0;
  }
  
  return 1;
}

/******************************************************************/
long int portwrite2(int fd, char data2[]) {
  long int len=2, n=0;
  
/*  removed sleep option to speed up large data writes such as waveform data */
  n = write(fd, data2, len);
  //  usleep(500000);   // add a pause (0.5 sec) to allow module to process/respond
  if (n < 0) {
    fputs("write failed!\n", stderr);
    return 0;
  }
  
  return 1;
}

/******************************************************************/
long int portread(int fd, char *data) {
  long int n=0,len=1;
  long int numport=0;
  char com1;
  fd_set fds;                      // type definition for select
  struct timeval timeout;          // structure defining timeout for select
  
  *data='?';                       // set data to known value to ensure it changes

/* FD_SET must be in line above select!   */
  timeout.tv_sec=0;                // time out in seconds
  timeout.tv_usec=300000;          // time out in microseconds
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  numport = select(FD_SETSIZE,&fds,(fd_set*)NULL,(fd_set*)NULL,&timeout);

// printf("numport = %i\n",numport);
	
  usleep(500000);   // add a pause (0.5 sec) to allow module to process/respond
  if (numport > 0) {
    n = read(fd, data, len);
  }
  if (n < 0) {
    if (errno == EAGAIN) {
      printf("SERIAL EAGAIN ERROR\n");
      return 0;
    } 
    else {
      printf("SERIAL read error %d %s\n", errno, strerror(errno));
      return 0;
    }
  }                    
  return 1;
}

/******************************************************************/
long int portread2(int fd, char *data) {
  long int n=0,len=1;
  long int numport=0;
  char com1;
  fd_set fds;                      // type definition for select
  struct timeval timeout;          // structure defining timeout for select
/*
   This is to be called after we've picked up the first data character of 
   a multi-character read
*/
  *data='?';                       // set data to known value to ensure it changes
  //  usleep(500000);   // add a pause (0.5 sec) to allow module to process/respond
  n = read(fd, data, len);
  
  if (n < 0) {
    if (errno == EAGAIN) {
      printf("SERIAL EAGAIN ERROR\n");
      return 0;
    } 
    else {
      printf("SERIAL read error %d %s\n", errno, strerror(errno));
      return 0;
    }
  }                    
  return 1;
}

/******************************************************************/
char BNCCommand(long int command) {
  char bnc[50];

/*
    Command list below; each commend returns '>' = 0x3e
*/
  bnc[0] = 0x30;  // Number 0 or power measure
  bnc[1] = 0x31;  // Number 1 or AM
  bnc[2] = 0x32;  // Number 2 or FM
  bnc[3] = 0x33;  // Number 3 or phase modulated
  bnc[4] = 0x34;  // Number 4 or sweep
  bnc[5] = 0x35;  // Number 5 or FSK
  bnc[6] = 0x36;  // Number 6 or burst
  bnc[7] = 0x37;  // Number 7 or SSB
  bnc[8] = 0x38;  // Number 8 or DTMF generator
  bnc[9] = 0x39;  // Number 9 or DTMF detection

  bnc[10] = 0x7A;  // z - MHz,   dBm
  bnc[11] = 0x79;  // y - kHz,  Vp-p, Sec
  bnc[12] = 0x78;  // x -  Hz, mvp-p,  ms
  bnc[13] = 0x6f;  // o - Offset
  bnc[14] = 0x74;  // t - Trigger
  bnc[15] = 0x03;  // 
  bnc[16] = 0x2E;  // . - decimal point or * 
  bnc[17] = 0x2D;  // - - minus or # sign
  bnc[18] = 0x2c;  // , - data separator character
  bnc[19] = 0x70;  // p = sync out in some data downloads

  bnc[20] = 0x6D;  // m - Mode
  bnc[21] = 0x6A;  // j - Function generator (not v as stated in manual)
  bnc[22] = 0x62;  // b - Sinewave
  bnc[23] = 0x71;  // q - Pulse
  bnc[24] = 0x67;  // g - Arbritary
  bnc[25] = 0x03;  // 
  bnc[26] = 0x77;  // w - waveform download
  bnc[27] = 0x64;  // d - waveform formatted in digital
  bnc[28] = 0x78;  // x - terminate download mode
  bnc[29] = 0x62;  // b - waveform formatted in binary

  bnc[30] = 0x61;  // a - reset to sinewave
  bnc[31] = 0x76;  // v - report hardware and software versions
  bnc[32] = 0x6B;  // k - enable/disable front panel/knob (1/0)
  bnc[33] = 0x65;  // e - enable/disable LCD echo to terminal (1/0)
  bnc[34] = 0x66;  // f - move cursor by field number (0-9)
  bnc[35] = 0x3F;  // ? - help
  bnc[36] = 0x68;  // h - help
  bnc[37] = 0x05;  // ^E - control
  bnc[38] = 0x65;  // e - echo on/off front panel LCD (1/0)
  bnc[39] = 0x6b;  // k - turn on/off front panel control (1/0)

  bnc[40] = 0x70;  // p - Move field left
  bnc[41] = 0x6E;  // n - Move field right
  bnc[42] = 0x75;  // u - Move digit up
  bnc[43] = 0x64;  // d - Move digit down
  bnc[44] = 0x6C;  // l - Move digit position left
  bnc[45] = 0x72;  // r - Move digit position right
  bnc[46] = 0x66;  // f - Field (coupled with number 0-9) 
  bnc[47] = 0x03;  // 
  bnc[48] = 0x73;  // Store or recall
  bnc[49] = 0x43;  // Clear or other

  return (bnc[command]);
}

/******************************************************************/
long int BNCParameters() {
  char amp[128],off[128];
  long int ii,jj,kk;
  long int old_amp, old_off, volt;

  printf("Changing amplitudes takes effect only after restarting pulsing \n");
  printf("Changing offset occurs immediately - only do this if you know what you are doing! \n");
  printf("Offset is usually set to 1/2 amplitude \n");
  printf("Amplitude and offset must be > 0 \n");

  old_amp=amplitude;
  old_off=offset;
  printf("Amplitude in millivolts ?\n");
  scanf("%s",amp);

  jj=0;
  kk=0;
  for (ii=0;ii<5;ii++){
    if (isdigit( amp[ii] )) jj++; 
  }
  amplitude = atoi(amp);
  printf (" amplitude = %s = %i, jj = %i \n", amp, amplitude, jj);

  if (amplitude < 0) {
    printf("Positive amplitude values only! Restoring old amplitude and offset....\n");
    amplitude = old_amp;
    return 0;
  }
  else if (amplitude > 2000){
    printf("Amplitude > 2000 mV! Restoring old amplitude and offset....\n");
    amplitude = old_amp;
    return 0;
  }

  printf("Offset in millivolts ? \n");
  scanf("%s",off);
  for (ii=0;ii<5;ii++){
    if (isdigit( off[ii] )) kk++; 
  }
  offset = atoi(off);
  printf (" offset = %s = %i, kk = %i \n",off, offset, kk);
  
  if (offset < 0) {
    printf("Positive offset values only! Restoring old offset and amplitude....\n");
    amplitude = old_amp;
    offset = old_off;
    return 0;
  }
  else if (offset > (amplitude/2 + 50) ){
    printf("Suspect offset-amplitude relationship!... Restoring old amplitude and offset....\n");
    amplitude = old_amp;
    offset = old_off;
    return 0;
  }

/*
   MAY NEED AT SOME POINT TO FIGURE OUT HOW TO NEGATIVE OFFSETS
   but initially don't need to because amplitude is +/-, ie.,
        -V/2 < amplitude <+V/2
*/

  BNCOffset();
  IntegerDigits(offset);       // set offset 
  BNCIO(12);                   // millivolts
  BNCIO(13);

  return 0;
}
/******************************************************************/
long int BNCIO( long int command){
  long int stat=99;
  char com1;
/*	
   For the BNC-632 I have decided to ignore the response to almost all 
   commands as this caused trouble in the early stages.  One could
   probably implement the code to look for responses (BNC-632 promises
   that all commands except save/restore are completed in 300ms).  So
   a usleep(300000); could be used.
*/
  com1=BNCCommand(command);      // get the proper command code
  portwrite1(fd,&com1);          // write to the device 
                               	
  return(stat);
}

/******************************************************************/
long int BNCIO2( long int ii, long int jj){
  long int stat=99;
  char data[2];
/*	
   For the BNC-632 I have decided to ignore the response to almost all 
   commands as this caused trouble in the early stages.  kthe exception
   is command 33 to echo back the front panel.  In introducing movement
   by field, it was discovered that it was possible to have the field 
   command time out if one didn't send the number of the field desired
   in time.  Therefore, this routine was implemented to send 2 data words
   immediately in time.
*/
  data[0]=BNCCommand(ii);      // Get code for first command
  data[1]=BNCCommand(jj);      // Get code for second command

  if (ii==33 && jj==1){       // erase read/write buffers prior to command
    portflush(fd);            // flush before reading LCD panel
    usleep(500000);           // pause to allow flush to complete
    portwrite2(fd,data);      // write to BNC
    portread2 (fd,data);      // read response of BNC
  } 
  else {
    portwrite2(fd,data);      // just write to device
  }
  
  return(stat);
}

/******************************************************************/
long int BNCResetSine(){
  long int stat=99;

  BNCIO(30);
	  
  return(stat);
}

/******************************************************************/
long int BNCFunction(){
  long int stat=99;

  BNCIO(21);

  return(stat);
}

/******************************************************************/
long int BNCSine(){
  long int stat=99;

  BNCIO(22);

  return(stat);
}

/******************************************************************/
long int BNCPulse(){
  long int stat=99;

  BNCIO(23);
	  
  return(stat);
}

/******************************************************************/
long int BNCWaveform(){
  long int stat=99;

  BNCIO(26);
	  
  return(stat);
}

/******************************************************************/
long int BNCWaveFormatDigital(){
  long int stat=99;

  BNCIO(27);
	  
  return(stat);
}

/******************************************************************/
long int BNCTerminate(){
  long int stat=99;

  BNCIO(28);
	  
  return(stat);
}

/******************************************************************/
long int BNCFrontOff(){
  long int stat=99;

  BNCIO2(32,0);
  
  return(0);
}
/******************************************************************/
long int BNCFrontOn(){
  long int stat=99;

  BNCIO2(32,1);

  return(1);
}
/******************************************************************/
long int BNCFrontOnOff(){
  long int ii, stat=99;

  printf("Front panel on/off=1/0 \n");
  scanf ("%li", &ii);  

  BNCIO2(32,ii);
	  
  return(ii);
}
/******************************************************************/
long int BNCLCDEchoOn(){
  long int stat=99;

  BNCIO2(33,1);
 
  return(stat);
}
/******************************************************************/
long int BNCLCDEchoOff(){
  long int stat=99;

  BNCIO2(33,0);
 
  return(stat);
}
/******************************************************************/
long int BNCLCDEchoOnOff(){
  long int ii, stat=99;

  printf("LCD echo on/off=1/0 \n");
  printf("Beware of this as you may screw up the current status display of this program! \n");
  scanf ("%li", &ii);  

  BNCIO2(33,ii);
 
  return(stat);
}
/******************************************************************/
long int BNC_0(){
  long int stat=99;

  BNCIO(0);

  return(stat);
}
/******************************************************************/
long int BNC_1(){
  long int  stat=99;

  BNCIO(1);  

  return(stat);
}
/******************************************************************/
long int BNC_2(){
  long int  stat=99;

  BNCIO(2);  

  return(stat);
}
/******************************************************************/
long int BNC_3(){
  long int  stat=99;

  BNCIO(3);  

  return(stat);
}
/******************************************************************/
long int BNC_4(){
  long int  stat=99;

  BNCIO(4);  

  return(stat);
}
/******************************************************************/
long int BNC_5(){
  long int  stat=99;

  BNCIO(5);  

  return(stat);
}
/******************************************************************/
long int BNC_6(){
  long int  stat=99;

  BNCIO(6);  

  return(stat);
}
/******************************************************************/
long int BNC_7(){
  long int  stat=99;

  BNCIO(7);  

  return(stat);
}
/******************************************************************/
long int BNC_8(){
  long int  stat=99;

  BNCIO(8);  

  return(stat);
}
/******************************************************************/
long int BNC_9(){
  long int  stat=99;

  BNCIO(9);  

  return(stat);
}
/******************************************************************/
long int BNCMHz(){
  long int  stat=99;

  BNCIO(10);  

  return(stat);
}

/******************************************************************/
long int BNCkHz(){
  long int  stat=99;

  BNCIO(11);  

  return(stat);
}

/******************************************************************/
long int BNCHz(){
  long int  stat=99;

  BNCIO(12);  

  return(stat);
}

/******************************************************************/
long int BNCOffset(){
  long int  stat=99;

  BNCIO(13);  

  return(stat);
}

/******************************************************************/
long int BNCTrigger(){
  long int  stat=99;

  BNCIO(14);  

  return(stat);
}

/******************************************************************/
long int BNCDot(){
  long int  stat=99;

  BNCIO(16);  

  return(stat);
}

/******************************************************************/
long int BNCMinus(){
  long int  stat=99;

  BNCIO(17);  

  return(stat);
}

/******************************************************************/
long int BNCFieldLeft(){
  long int  stat=99;

  BNCIO(40);  

  return(stat);
}

/******************************************************************/
long int BNCFieldRight(){
  long int  stat=99;

  BNCIO(41);  

  return(stat);
}

/******************************************************************/
long int BNCDigitUp(){
  long int  stat=99;

  BNCIO(42);  

  return(stat);
}

/******************************************************************/
long int BNCDigitDown(){
  long int  stat=99;

  BNCIO(43);  

  return(stat);
}

/******************************************************************/
long int BNCDigitLeft(){
  long int  stat=99;

  BNCIO(44);  

  return(stat);
}

/******************************************************************/
long int BNCDigitRight(){
  long int  stat=99;

  BNCIO(45);  
	  
  return(stat);
}

/******************************************************************/
long int BNCField(){
  long int ii, jj=46, stat=99;
/*
Need to ask field first before sending command to device
as it looks like it times out waiting for the field value.
*/
  printf ("Field number (0-9), 0=turns curser off ?\n");
  scanf ("%li", &ii);

  BNCIO2(jj,ii);  

  return(stat);
}

/******************************************************************/
long int BNCArbitrary(){
  long int stat=99;

  BNCIO(24);  

  return(stat);
}

/******************************************************************/
long int GrowDecay(){
  long int  stat=99;
  long int ii, jj, kk, mm, iduty, msec, clockfreq;
  double duty, freq;
  char com1, data, grow[128],decay[128];
  //  long int trig=1;         // self-triggered or continuous mode; 0 = external trigger

  for (ii=0;ii<127;ii++){
    grow[ii]=0;
    decay[ii]=0;
  }

  printf("Integer input  - actual time may be slightly different due to hardware\n");
  printf("   Fixed hardware setup:\n");
  printf("     - power supply amplifier is x1000 \n");
  printf("     - positive only signals \n");
  printf("     - voltage (mV) set to half that shown on LCD  \n");  
  printf("     - self-triggered mode   \n");  

  printf("Time for grow-in (ms) \n");       
  scanf("%s",grow);
  printf("Time for decay-out (ms) \n");    
  scanf("%s",decay);

  jj=0;
  kk=0;
  for (ii=0;ii<20;ii++){
    if (isdigit(  grow[ii] )) jj++; 
    if (isdigit( decay[ii] )) kk++; 
  }
  igrow = atoi(grow);
  idecay = atoi(decay);
  //  printf (" igrow = %s = %i, jj = %i \n", grow,  igrow, jj);
  //  printf ("idecay = %s = %i, kk = %i \n",decay, idecay, kk);
/*
  calculate waveform duration (msec) and duty cycle (iduty) in integer percentage
  to go to freguencies << 1 Hz need to which to arbitrary mode to set clock frequency
  as suggested in manual.
*/
  msec = igrow+idecay;              // cycle time
  iduty = idecay*100/msec;          // time signal is high - need 100 to get percent
  freq = 1000./(double) msec;       // frequency
  clockfreq = freq * 16000;         // hardware determined to go to low sub Hz frequencies
 
  printf ("    Cycle time = %.2lf Hz\n", freq);
  printf (" Beam  on time = %i ms\n", msec);
  printf (" Beam off time = %i \% \n", iduty);
  printf ("    Clock time = %i if needed for < 1Hz \n", clockfreq);

/*
   Now configure BNC-632
    Go to pulse mode
*/
  BNCPulse();
 
/* do field 1 commands - positive signals */
  PulseF1();
	
/* do field 2 commands - duty cycle */
  PulseF2(iduty); 

/* do field 3 commands - trigger */
  PulseF3(trig); 
	
/* do field 5 commands - amplitude */
  PulseF5();

/* 
  do field 4 commands - frequency - last to allow arbitrary function
  option to fine tune repitition rate.
*/
  PulseF4(freq); 

/* Return to Field 0 to turn off cursor */
  BNCIO(46);      // go to field
  BNCIO(0);       // select field 0
  

/* 
    still need to explore arb. mode for << 1 Hz pulses) 
	still need to check if pulse mode can have < 1 Hz values
*/
  return(stat);
}

/******************************************************************/
long int PulseF1() {
  long int stat=0;
/*
   go to field 1 (positive signal only field) and set to Y
*/
  BNCIO(46);      // go to field
  BNCIO(1);       // select field 2
  BNCIO(1);       // 0 = yes; 1 = no
	
	return (stat);
}

/******************************************************************/
long int PulseF2(long int iduty) {
  long int stat=0;
/*
  go to field 5 (voltage field) and set to millivolts
  do interger math to get % in characters
*/

  BNCIO(46);             // go to field
  BNCIO(2);              // select field 2
  IntegerDigits(iduty);  // load and set digits
  BNCHz();               // acts as enter for percentage

  return (stat);
}

/******************************************************************/
long int PulseF3(long int trig) {
  long int stat=0;
/*
  go to field 3 (trigger field) and set to continuous
*/
  BNCIO(46);      // go to field
  BNCIO(3);       // select field 2
  BNCIO(1);       // 0 = Trigger; 1 = Continuous

  return (stat);
}

/******************************************************************/
long int PulseF4(double freq) {
  long int contrep;
  long int stat=0;
/*
  go to field 4 (frequency field) 87,654,321
*/

  printf("frequency = %0.2lf\n",freq);

  BNCIO(46);           // go to field
  BNCIO(4);            // select field 4
  BNCIO(49);           // clear all digits
  DoubleDigits(freq);  // load and set digits
  BNCHz();             // enters Hz
	  
/*
   Pulse mode can only be given in 1 Hz steps.  I will guess that
   the fastest complete grow-in and decay cycle will always be greater
   than 100 ms or 10 Hz.  Presumably, this is adjustable.
*/
  if (freq <= 10.) {
    BNCIO(40);      // clear previous setting
    BNCIO(1);       // set to 1 Hz
    BNCHz();        // enters Hz

    BNCIO(24);      // go to arbitrary mode
    BNCIO(46);      // go to field
    BNCIO(2);       // select field 2
        
    contrep = 16000*freq;    // see p 51 in manual
    DoubleDigits(contrep);   // load and set digits
    BNCHz();                 // enters Hz

/* go to Arbitrary function field 3 and make sure amplitude is set */
    ArbF3();
 }

  return (stat);
}


/******************************************************************/
long int PulseF5() {
  long int stat=0;
/*
  go to field 5 (voltage field) and set to millivolts
*/

  BNCIO(46);                 // go to field
  BNCIO(5);                  // select field 5
 
  BNCIO(49);                 // clear all digits
  IntegerDigits(amplitude);  // load and set digits
  BNCIO(12);                 // millivolts selection
	
  return (stat);
}


/******************************************************************/
long int ArbF3() {
  long int stat=0;
/*
  go to field 5 (voltage field) and set to millivolts
*/

  BNCIO(46);                // go to field
  BNCIO(3);                 // select field 3
 
  BNCIO(49);                // clear all digits
  IntegerDigits(amplitude); // load and set digits
  BNCIO(12);                // millivolts selection
	
  return (stat);
}

/******************************************************************/
long int Manual(){
  long int stat, command=0;

  while (command > -1 && command < 50){
    printf(" 0 - Number 0 or power measure\n");//    0x30          
    printf(" 1 - Number 1 or AM\n");//               0x31
    printf(" 2 - Number 2 or FM\n");//               0x32 
    printf(" 3 - Number 3 or phase modulated\n");//  0x33 
    printf(" 4 - Number 4 or sweep\n");//            0x34
    printf(" 5 - Number 5 or FSK\n");//              0x35
    printf(" 6 - Number 6 or burst\n");//            0x36
    printf(" 7 - Number 7 or SSB\n");//              0x37
    printf(" 8 - Number 8 or DTMF generator\n");//   0x38
    printf(" 9 - Number 9 or DTMF detection\n");//   0x39

    printf("10 - MHz,   dBm \n");//                  0x7a = z 
    printf("11 - kHz,  Vp-p, Sec \n");//             0x79 = y 
    printf("12 -  Hz, mvp-p,  ms\n");//              0x78 = x 
    printf("13 - Offset \n");//                      0x6f = o 
    printf("14 - Trigger \n");//                     0x74 = t 
    printf("15 -  \n");//
    printf("16 - decimal point or *  \n");//         0x2e = . 
    printf("17 - - minus or # sign \n");//           0x2d = -
    printf("18 - , comma sign \n");//                0x2c = ,
    printf("19 - p - sync out in some data downloads \n");// 0x70 = p

    printf("20 - Mode \n");//                        0x6d = m  
    printf("21 - Function \n");//                    0x76 = v 
    printf("22 - Sinewave \n");//                    0x62 = b  
    printf("23 - Pulse \n");//                       0x71 = q  
    printf("24 - Arbritary \n");//                   0x67 = g  
    printf("25 -  \n");//
    //    printf("26 - Download waveform \n");//           0x77 = w
    //    printf("27 - Digital format download\n");//      0x64 = d
    //    printf("28 - Terminate download mode \n");//     0x78 = x
    //    printf("29 - Binary format download \n");//      0x62 = b
    
    printf("30 - reset to sinewave \n");//                          0x6D = a 
    printf("31 - \n"); // Report hardware and software versions not implemented    0x76 = v 
    printf("32 - enable/disable front panel/knob (1/0) \n");//      0x6B = k 
    printf("33 - enable/disable LCD echo to terminal (1/0) \n");//  0x65 = e  
    printf("34 - \n");//  
    printf("35 - \n");// Help function through hardware not implemented    0x3F = ? 
    printf("36 - \n");// Help function through hardware not implemented    0x68 = h
    printf("37 - \n");// ^E test communication - returns ^C not implemented    0x05 = ^E  
    printf("38 - Echo LCD terminal (1 = on, 0 = off) \n");//        0x65 = e
    printf("39 - \n");//  
    
    printf("40 - Move field left \n");//                0x70 = p  
    printf("41 - Move field right \n");//               0x6E = n 
    printf("42 - Move digit up \n");//                  0x75 = u 
    printf("43 - Move digit down \n");//                0x64 = d 
    printf("44 - Move digit position left \n");//       0x6C = l  
    printf("45 - Move digit position right \n");//      0x72 = r  
    printf("46 - Field (couple with number 0-9) \n");// 0x66 = f
    printf("47 -  \n");//
    printf("48 - Store or recall  \n");//               0x73 =  
    printf("49 - Clear or other \n");//                 0x43 = 
    printf(">50 - eexit manual operation mode \n");//       
    printf("\n");
    
    printf ("Enter the command ?\n");
    scanf ("%li", &command);

    switch (command) {
    case  0: BNC_0(); break;
    case  1: BNC_1(); break;
    case  2: BNC_2(); break;
    case  3: BNC_3(); break;
    case  4: BNC_4(); break;
    case  5: BNC_5(); break;
    case  6: BNC_6(); break;
    case  7: BNC_7(); break;
    case  8: BNC_8(); break;
    case  9: BNC_9(); break;
    case 10: BNCMHz(); break;
    case 11: BNCkHz(); break;
    case 12: BNCHz(); break;
    case 13: BNCOffset(); break;
    case 14: BNCTrigger(); break;
    case 16: BNCDot(); break;
    case 17: BNCMinus(); break;
    case 20: BNCMode(); break;
    case 21: BNCFunction(); break;
    case 22: BNCSine(); break;
    case 23: BNCPulse(); break;
    case 24: BNCArbitrary(); break;
      //    case 26: BNCWaveform(); break;
      //    case 27: BNCWaveFormatDigital(); break;
      //    case 28: BNCTerminate(); break;
      //    case 29: BNCWaveFormatBinary(); break;
    case 30: BNCResetSine(); break;
    case 32: flagfrontpanel=BNCFrontOnOff(); break;
    case 33: BNCLCDEchoOnOff(); break;
    case 38: BNCStatus(); break;
    case 40: BNCFieldLeft(); break;
    case 41: BNCFieldRight(); break;
    case 42: BNCDigitUp(); break;
    case 43: BNCDigitDown(); break;
    case 44: BNCDigitLeft(); break;
    case 45: BNCDigitRight(); break;
    case 46: BNCField(); break;
    case 48: printf("not implemented\n"); break;
    case 49: printf("not implemented\n"); break;
    default: printf("Invalid command .... returning ....\n"); break;
    }
  }
  BNCStatus();
  return(stat);
}

/******************************************************************/
long int BNCStatus(){
  char data;
  long int ii=0, stat=99;

/*
  Report buffer consists of two lines of 40 characters sandwiched by
  a double quote, carriage return, and a linefeed sequence.  Do not know
  the order of these things:
   <cr> = 0x0d
   <lf> = 0x0a
    "   = 0x22

0         1         2         3         4
 1234567890123456789012345678901234567890
 Cursor: off                                ...<cr><lf> + 11 + <cr><lf> = 15
"Sinewave mode                           "  ...40 + 2 quotes + <cr><lf> = 44
" 1,000,000.00 Hz               -10.0 dBm"  ...40 + 2 quotes + <cr><lf> = 44

*/
  printf ("\n\n");
  printf ("Reporting status...\n");
  BNCLCDEchoOn();      // Turn on LCD echo
  usleep(500000);
  portread(fd,&data);         // read first character after initial call
  for (ii =0; ii < 104; ii++) {
    portread2(fd,&data);       // read next group of characters 
    printf("%c",data);
  }
  //  printf("\n characters read=%i\n",ii);
  printf ("\n\n");
  BNCLCDEchoOff();      // Turn off LCD echo

  return(stat);
}

/******************************************************************/
long int BNCMode(){
  long int ii, stat=99;

  printf ("Function (0-9) ? \n");
  printf ("0 - Power measure \n");
  printf ("1 - Am \n");
  printf ("2 - FM \n");
  printf ("3 - Phase modulated \n");
  printf ("4 - Sweep \n");
  printf ("5 - FSK \n");
  printf ("6 - Burst \n");
  printf ("7 - SSb \n");
  printf ("8 - DTMF generator \n");
  printf ("9 - DTMF detection \n");

  scanf ("%i", &ii);

  BNCIO(20);                // go to Mode

  switch(ii){
  case  0: BNC_0(); break;
  case  1: BNC_1(); break;
  case  2: BNC_2(); break;
  case  3: BNC_3(); break;
  case  4: BNC_4(); break;
  case  5: BNC_5(); break;
  case  6: BNC_6(); break;
  case  7: BNC_7(); break;
  case  8: BNC_8(); break;
  case  9: BNC_9(); break;
  default: BNC_0(); break;
  }

  return(stat);
}

/******************************************************************/
long int IntegerDigits(long int mm){
  long int ii,jj,hi=10,nflag=0;

  for (ii=hi; ii>=0; ii--){                     // integer value loop with 10 digits
    jj = mm / (long int) pow(10,ii);            // get digit
    if (jj != 0 && nflag == 0) nflag=1;         // finding first non-zero digit
    if (nflag == 1){                            // process all significant digits
      mm = mm - (jj * (long int) pow(10,ii));   // set up mm for next loop
      BNCIO(jj);                                // digit of 10^jj Hz
    }
  }
  return;
}
 
/******************************************************************/
long int DoubleDigits(double xx){
  long int ii,kk,hi=8,lo=-3,nflag=0;
  double yy;

/* Don't worry about rounding at 0.01 level; do not require that precision */

  for (kk=hi; kk>lo; kk--){                  // double value loop
    yy = xx / pow(10,kk);                    // get digit
    ii = (long int) yy;                      // get integer digit
    if (ii != 0 && nflag == 0) nflag=1;      // output first non-zero value
    if (nflag == 1){                     
      xx = xx - ((double)ii * pow(10,kk));   // set up xx for next loop
      if (kk == -1) BNCIO(16);               // add decimal point
      BNCIO(ii);                             // digit of 10^kk Hz
    }
  }
  return;
}
/******************************************************************/
long int MTCMovement(){

  printf("Time for MTC tape to move (ms) \n");       
  scanf("%li",&mtc);

  return;
}

/******************************************************************/
long int BNCDigital() {
  long int ii, jj, kk, msec, volt;
  long int ll,mm, ilo, ihi;
  long int stat=99;

  volt = amplitude - offset;
  if (volt > 2000 || volt < -2000) {
    printf (" ERROR - max voltage difference between amplitude and offset is 2000 mV \n");
    printf(" - amplitude = %li mV\n",amplitude);
    printf(" - offset = %li mV\n",offset);
    printf(" - volt difference = %li mV\n",volt);
    printf(" RETURNING without adjusting pulsing\n");
  }

  BNCWaveformCreate();  // create waveform of max=16384 points based on grow-in/decay-out/cycles/mtc move times

  printf("Setting ....\n");
  printf(" - wave function to arbitrary \n");

  BNCArbitrary();                   // select arbitrary waveform

/* Set trigger                                   */
  if (trig == 0) {
    printf(" - trigger to MTC/computer\n");
  }
  else {
    printf(" - trigger to continuous\n");
  }
  BNCIO(46);      // go to field
  BNCIO(1);       // select field 1
  BNCIO(trig);    // 0 = yes; 1 = no = external trigger

/* Set clock                                       */
  printf(" - clock = %0.2lf\n",xclock);
  BNCIO(46);      // go to field
  BNCIO(2);       // select field 2
  BNCIO(49);      // clear flield
  DoubleDigits(xclock);
  BNCHz();        // enters Hz

/* Set amplitude                                   */
  printf(" - amplitude = %li \n",amplitude);
  BNCIO(46);      // go to field
  BNCIO(3);       // select field 3
  BNCIO(49);      // clear flield
  IntegerDigits(amplitude);
  BNCIO(12);     // millivolts
 
/* Return to Field 0 to turn off cursor */
  BNCIO(46);      // go to field
  BNCIO(0);       // select field 0

/* Set offset                                   */
  printf(" - offset = %li \n",offset);
  BNCOffset();                 // go to offset 
  IntegerDigits(offset);       // set offset
  BNCIO(12);                   // millivolts
  BNCIO(13);                   // exit offset mode

/* Upload waveform data                         */
//  BNCWaveformUpload(max, wave);
  BNCWaveformUploadBin(max, wavebin);

  printf("Settings completed \n");

  return;
}

/******************************************************************/
long int BNCWaveformCreate(){
  long int ii,jj,kk,ll,mm,ilo,ihi;
  long int msec;
  double xmsec,xmax,beamon,beamoff,mtcmove,hz;
/*
   Create waveform of max=16384 points based on 
     - grow-in time (igrow,  beamon,  jj)
     - decay-out    (idecay, beamoff, kk)
     - number of grow-in+decay-out cycles (cycles, mm)
     - mtc move times  (mtc, mtcmove)
     - msec/xmsec == waveform duration in ms
*/
  msec = mtc+(idecay+igrow)*cycles;      // total time (inc. mtc move) for 1 waveform

  xmax  = (double)max;
  xmsec = (double)msec;
  hz = 1000. / xmsec;            // total frequency of each pulse
  xclock = hz * xmax;             // BNC-632 relation to waveform points to frequency

  if (xclock < 0.01){
    printf("Wrong input !\n");
    printf("Growth, decay must be greater than 1 ms  !\n");
    printf("Growth + decay must be less than  1,600,000 s !\n");
  }

  beamon  = ((double)igrow /xmsec);     // fraction of beam on per cycle
  beamoff = ((double)idecay/xmsec);     // fraction of beam off per cycle
  mtcmove = ((double)mtc   /xmsec);     // fraction of beam off per cycle for mtc move

  printf("Beam will be pulsed .... \n");
  if (trig == 0) {
    printf(" - via MTC or computer (if computer, make sure MTC cannot issue a trigger) \n");
  }
  else {
    printf(" - continuously at %0.2lf Hz  ", hz);
    printf(" -> MTC movement derived from BNC-632 sync out or amplifier out\n");
  }
  printf(" - grow-in         for %li ms \n",igrow);
  printf(" - decay-out       for %li ms \n",idecay);
  printf(" - cycles/move     for %li    \n",cycles);
  printf(" - mtc movement    for %li ms \n",mtc);
  printf(" - complete cycle takes %li ms \n",msec);
  printf(" - beam on  for %0.2lf\%  \n",(beamon*cycles)*100);
  printf(" - beam off for %0.2lf\% (includes MTC move)\n",((beamoff*cycles)+mtcmove)*100);

/* Load waveform into array for upload to BNC-632  */

  jj = (long int)(beamon *xmax);    // number of wavepoints for beam on
  kk = (long int)(beamoff*xmax);    // number of wavepoints for beam off
  ll = jj+kk;                       // number of wavepoints for one grow-in-decay-out cycle
  ihi = max-1;                      // set to upper array limit
  for (mm=0; mm < cycles; mm++){                   // cycles per mtc move (usually 1)
    ilo = ihi-kk;                                  // run continuously down array dimensions
    for (ii=ihi; ii> ilo; ii--){
      wave[ii] = 1;           // decay out
      wavebin[ii]=0xf07f;     // decay out = highest 12-bits = +1; see comment below on binary data
    }
    ihi = ilo;                                     // set next limits
    ilo = ihi-jj;
    for (ii=ihi; ii> ilo; ii--){
      wave[ii] = 0;           // grow-in
      wavebin[ii]=0x0080;     // grow-in = highest 12-bits = 0 (do not want negative pulses) 
    }
   ihi = ilo;                                     // set limit for next pass or mtc move
  }
/*
  Binary data on linux is sent with low byte first followed by high byte.
  Therefore if I want to send:
      +1 = 7ff0 I must send f07f.
       0 = 0000 I must send 0000.
      +1 = 7ff8 I must send f87f ==> 8 sets the sync out signal to high
      -1 = 8000 I must send 0080
*/
  for (ii=ihi; ii>-1; ii--) {
    wave[ii] = 1;           // MTC move at beginning of waveform
    wavebin[ii]=0xf87f;     // MTC move + sync out high = highest 12-bits = +1, 13th bit set to 1
  }
  if (idecay == 0) wave[0]=0;            // experimentally determined fix to return to zero output
  if (idecay == 0) wavebin[0]=0x0000;    // experimentally determined fix to return to zero output

  return 0;
}

/******************************************************************/
long int BNCWaveformUpload(long int len, long int waveup[]){
  long int ii;

  printf(" - waveform of %li data points\n",len);
  BNCIO2(26,27);             // select download + digital format mode
  for (ii=0; ii<len; ii++){
    BNCIO2(waveup[ii],18);    // download data point and data separator ","
    printf("\r - waveform of %li data points downloaded ",ii+1);
    fflush(stdout);
  } 
  BNCTerminate();                   // terminate download mode "x"
  printf("\n");

  return 0;
}
/******************************************************************/
long int BNCWaveformUploadBin(long int len, unsigned short int wavebin[]){
  long int ii, jj=3;

  printf(" - waveform of %li data points binary\n",len);
  BNCIO2(26,29);             // select download + binary format mode
  PortWriteBin(fd,wavebin);

  printf(" - waveform of %li data points uploaded - binary\n",len);
  printf(" - wait %li seconds for upload to finish and time-out\n",jj);
  sleep(jj);            // sleep for 3 s to terminate upload
  printf("\n");

  return 0;
}

/******************************************************************/
long int PortWriteBin(int fd, unsigned short int databin[]) {
  long int len=32768, n=0;  //16384
  
/*  removed sleep option to speed up large data writes such as waveform data */
  n = write(fd, databin, len);

  if (n < 0) {
    fputs("write failed!\n", stderr);
    return 0;
  }
  //  printf("n=%li\n",n);
  return 1;
}

/******************************************************************/
long int Menu(long int panel){
  long int ans;

  printf ("----------------------------------------------------------------------------\n");
  printf ("       Measure       |         Manual          |    Current settings   \n");
  printf ("----------------------------------------------------------------------------\n");
  printf ("1 - run pulsing      | 11 - send trigger       | Grow-in     = %li ms\n",igrow);
  printf ("2 - set-up pulsing   | 12 - front panel on     | Decay-out   = %li ms\n",idecay);
  printf ("3 - continuous beam  | 13 - front panel off    | MTC move    = %li ms \n",mtc);
  printf ("4 - stop beam        | 14 - amplitude/offset   | Ampl. (P-P) = %li mV\n",amplitude);
  if (trig == 0){
    printf ("5 - status           | 15 - manual operation   | Trigger     = External \n");
  }
  else {
    printf ("5 - status           | 15 - manual operation   | Trigger     = Continuous \n");
  }
  printf ("6 -                  | 16 - MTC movement time  | Cycles      = %li \n",cycles);
  printf ("-----------------------------------------------| Offset      = %li mV\n",offset);
  if (panel == 0){
    printf ("0 - end              | 10 - help               | Front panel = Off  \n");
  }
  else {
    printf ("0 - end              | 10 - help               | Front panel = On   \n");
  }
  printf ("----------------------------------------------------------------------------\n");

  scanf ("%li", &ans);          /* reads in ans (pointer is indicated */

  return ans;
}

/******************************************************************/
void Help(){
  char ESC=27;

  printf("*************************************************************************************\n");
  printf("********************************* Kicker Help ***************************************\n");
  printf("*************************************************************************************\n");
  printf("                                   Commands                                          \n");
  printf("*************************************************************************************\n");
  printf("0 - end program \n");
  printf("  - can be done and not affect BNC-632 beam pulsing \n");
  printf("  - *** ON RESTART BNC-632 WILL GO TO SINE WAVE *** \n");
  printf("  - *** MAKE SURE BEAM IS OFF                   *** \n");
  printf("1 - set up and begin pulsing of beam \n");
  printf("  - provide grow-in time, decay-out time, and trigger\n");
  printf("  - times = integer format\n");
  printf("  - times = milliseconds\n");
  printf("  - trigger (y) = continuous or repeating pulsing\n");
  printf("  - trigger (n) = computer/MTC control of pulsing\n");
  printf("  - MTC controlled, a TTL signal must be provided on Ext Trigger on back of BNC-632\n");
  printf("  - pulse starts on leading edge of external trigger\n");
  printf("  - pulse beginning is high (beam off) for MTC movement\n");
  printf("  - pulse middle is low (beam on) for grow-in cycle\n");
  printf("  - pulse beginning is high (beam off) for decay-out cycle\n");
  printf("  - program can provide single triggers to ensure short burst of beam\n");
  printf("2 - resume pulsing of beam from previous set-up parameters \n");
  printf("3 - turn off pulsing of the beam by setting the BNC-632 to no output\n");
  printf("4 - stop the beam using the BNC-632 to deflect the beam by raising output\n");
  printf("5 - show status by printing the front LCD panel of the BNC-632\n");
  printf("\n");
  printf("10 - list this section \n");
  printf("11 - issue a single trigger to the BNC (see last section of 1 above\n");
  printf("12 - turn on  front panel of BNC-632 for hand control\n");
  printf("13 - turn off front panel of BNC-632 to prevent accidental changes (default)\n");
  printf("14 - adjust the amplitude and offset of the BNC-632 waveform\n");
  printf("   - *** DO NOT ADJUST UNLESS YOU KNOW WHAT YOU ARE DOING ***\n");
  printf("   - amplitude is the signal height peak-to-peak ie -V/2 -> +V/2\n");
  printf("   - maximum voltage range is 0 -> 2000 mV for our HV amplifier\n");
  printf("   - offset is an adjustment to ensure 0 mV is 0 mV \n");
  printf("   - offset is also used to ensure no steering of the beam should occur\n");
  printf("   - Initially offset is set to 1/2 amplitude so signal goes 0 -> Vo \n");
  printf("   - values are -6000 mV < offset < +6000 mV \n");
  printf("15 - go to manual control of the BNC-632\n");
  printf("   - issue commands as if you were using the front panel\n");
  printf("   - a list of possible commands is always printed to the screen\n");
  printf("16 - input the time it takes the MTC to move the tape fo rthe next measurement\n");
  printf("   - default is 200 ms \n");
  printf("   - electronic noise may require more time than the physical tape movement \n");
  printf("\n");
  printf("*************************************************************************************\n");
  printf("              Hints on hardware setup and running philosophy                         \n");
  printf("*************************************************************************************\n");
  printf("Baud rate on BNC-632 should be set to 38400 \n");
  printf(" - turn on BNC-632 \n");
  printf(" - default baud rate is 9600 \n");
  printf(" - go to Mode, Offset \n");
  printf(" - turn knob until 38400 is chosen \n");
  printf(" - program should now be able to communicate \n");
  printf("Sync out on front panel is a duplicate of actual signal BUT may not go to 0 V \n");
  printf(" - can use signal out to monitor signals going to amplifier \n");
  printf(" - sync is up when signal is up \n");
  printf("Sig out should be sent to amplifier input \n");
  printf(" - amplifier is x1000 \n");
  printf(" - beam is deflected when signal goes up\n");
  printf(" - check that down is 0 mV or give a correction voltage to opposite deflector plate\n");
  printf("Program does not read BNC-632 except for the front panel LCD\n");
  printf(" - changes to parameters by front panel will not be given to program\n");
  printf(" - exiting program does not affect beam pulsing\n");
  printf(" - restarting program DOES affect beam pulsing and should not be done with beam on target\n");
  printf(" - this can be changed but manual suggests BNC-632 should be started in known state\n");
  printf(" - known state is sine wave, offset = 0 mV, frequency,amplitude at previous sine wave values\n");
  printf("To stop beam just for tape movement \n");
  printf(" - set grow-in to any non-zero value\n");
  printf(" - set decay-out to 0 ms\n");
  printf(" - set tape movement time required\n");
  printf("\n");
      printf("%c[1m",ESC);  // turn on bold face
  printf("*********Notes on REMOTE OPERATION of the MTC******** \n");
      printf("%c[0m",ESC);  // turn off bold face
  printf("  1) Set the dwell time on the MTC control unit to a value lower \n");
  printf("     than the dwell time determined by the remote control program \n");
  printf("     (time between desired MTC moves). A value of 10 ms is suggested. \n");

  printf("  2) Set \"Action when completed\" to STOP.  This ensures that when the \n");
  printf("     remote program issues the next move command, the MTC has completed  \n");
  printf("     the previous move cycle. \n");

  printf("  3) Check the time it takes to move the tape.  This time plus the dwell \n");
  printf("     time on the MTC controller MUST be less than the remote dwell time. \n");
  printf("     You can make the dwell time as short as you want, but you must alter \n");
  printf("     the acceleration and deceleration speed or the distance to change  \n");
  printf("     the time to move the tape.   \n");

  printf("  4) Refer to the manual for options and suggestions.  These changes \n");
  printf("     should be done very carefully and the action of the tape observed \n");
  printf("     afterwards.  This means the following should be valid: \n");
  printf("   \n");
  printf("    Time between moves < MTC dwell + MTC acceleration + MTC deceleration \n");
  printf("    where MTC dwell = 10 ms and typical accel/decel times are 100-200 ms \n");


  printf("\n");
  WaveformDiagram();
  printf("\n");
  printf("*************************************************************************************\n");
  return;
}

/******************************************************************/
void WaveformDiagram(){

  printf("                    Signal waveform                           \n");
  printf("  |<-------- 1 complete cycle or waveform ------------------>|\n");
  printf("   ______                           ________________________________\n");
  printf("__|      |_________________________|                         |      |_____\\ \n");
  printf("    MTC      Grow-in (beam on)        Decay-out (beam off)     MTC  \n");
  printf("    move                                                       move \n");

  return;
}
/******************************************************************/
void SetupParameters(){
 /* 
    all variables are global variables   
 */
  printf("Digital waveform input \n");
  printf(" - clock     is set to     1 ms \n");
  printf(" - amplitude is set to %li mV \n",amplitude);
  printf(" - offset    is set to %li mV \n", offset);
  printf("\n");

  printf("Time for  grow-in  (ms) ?  ");       
  scanf("%li",&igrow);
  if (igrow  < 1) igrow = 1;

  printf("Time for decay-out (ms) ?  ");    
  scanf("%li",&idecay);
  if (idecay < 0) idecay = 0;
  printf("Pulsing controller: \n");
  printf("   0 = MTC/computer -> external signal from MTC or program command 11 \n");    
  printf("   1 - self         -> beam pulsing repeats automatically \n");    
  printf("\nWhat controls pulsing ? ");    
  scanf("%li",&trig);
  if (trig != 1) trig =0;
  printf("How many pulses per MTC movement ? (1 is normal operations)  ");    
  scanf("%li",&cycles);

  return;
}

/******************************************************************/
/* ********************************************************************* *
 * testtime.c
 *
 * Written by:        M.J. Brinkman
 * At the request of: C.J. Gross
 *
 *     (c) 1997  All rights reserved.
 *
 *  A simple test to check to see if we get a nicely formatted time
 *  and date string.
 * ********************************************************************* *
 *  Global includes that need to be there.
 * ********************************************************************* */

/* ********************************************************************* *
 * char *GetDate()
 *
 *  A function that returns the pretty-formatted date/time string.
 * ********************************************************************* */

char *GetDate() {
	/*
	 *  Variable type definitions.  Both of these types found in
	 *  <time.h>.
	 */
    struct tm *timer;
    time_t    myClock;
	
	/*
	 *  Get the current clock time.
	 *  Form:  time(time_t *)
	 *  Stores the current clock time in the time_t pointer.
	 */
    time((time_t *) &myClock);
	
	/*
	 *  Set us in the current time zone,
	 *  Form:  tzset()
	 */
    tzset();
	
	/*
	 *  Convert the current clock to the current time in our
	 *  local time zone.
	 *  Form:  struct tm *localtime(time_t *)
	 */
    timer = (struct tm *) localtime((time_t *) &myClock);
	
	/*
	 *  Now do all the nice formatting.
	 *  Form: char *asctime((struct tm *))
	 */
    return (char *) asctime(timer);
}


