/*
 Include file for usrdata programs
*/
#define ONLINEPATH "../data/online.bin"             // com data for control of daq programs
#define ONLINESIZE sizeof(struct onLine)

struct comDataOnLine {

  int com0;           // command to provide instructions to xia2disk
  int com1;           // command to provide instructions to 
  int online[100000]; // online buffer
};

struct comData *onlptr;

int openOnLine();
void closeOnLine(int mapOnl);
int mapOnl=-1;
