/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2013	*/
/****************/
#ifndef CLIENT_H_
#define CLIENT_H_

#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>

enum {
  NormalReturn = 0,
  ErrorReturn = -1,
};

/* ------------------------------------------------------------------ */

#ifdef ASSERT_DEBUG
#define ASSERT(ASSERTION) \
 { assert(ASSERTION); }
#else
#define ASSERT(ASSERTION) \
{ }
#endif

/* ------------------------------------------------------------------ */

	/********************/
	/* Client Functions */
	/********************/
#ifdef __cplusplus
extern "C" {
#endif

extern int InitReplFs(unsigned short portNum, int packetLoss, int numServers);
extern int OpenFile(char * strFileName);
extern int WriteBlock(int fd, char * strData, int byteOffset, int blockSize);
extern int Commit(int fd);
extern int Abort(int fd);
extern int CloseFile(int fd);
int checkServers(int inputNumServers);
int openfilereq();
int commitreq(int fd);
int commitPhase2(int fd);
int aborting(int fd);
int countServers(uint32_t *servers,  uint32_t gid);

void getRemainTime(struct timeval start, long int timeout, struct timeval *remain);
bool ismyLogEmpty();
void cleanupmylog();

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */

#endif







