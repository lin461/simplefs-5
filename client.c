/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2014	*/
/****************/

//#define DEBUG

#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "client.h"
#include "network.h"


/*
 * Global Virables
 */

static	int		mySock = -1;
static Sockaddr	*myAddr = NULL;

static logEntry_t	*myWriteLogs;

static uint32_t 	myTransNum;
static uint32_t 	myFileID;
static int 		myNumServers = -1;
static int 		myPacketLoss = -1;

static uint32_t 	mySeqNum	= -1;
static uint32_t	myGlobalID;

//static struct timeval myLastStartTime;
//static struct timeval myLastSendTime;

/* ------------------------------------------------------------------ */

int
InitReplFs( unsigned short portNum, int packetLoss, int numServers ) {
#ifdef DEBUG
  printf( "InitReplFs: Port number %d, packet loss %d percent, %d servers\n", 
	  portNum, packetLoss, numServers );
#endif

  /****************************************************/
  /* Initialize network access, local state, etc.     */
  /****************************************************/
	if (packetLoss < 0 || packetLoss > 100 || numServers < 0) {
		RFError("Wrong Input Params.");
		return ErrorReturn;
	}

	// check if already initialized
	if (myAddr != NULL || mySock != -1 || myPacketLoss != -1 || myNumServers != -1) {
		RFError("Already initialized.");
		return ErrorReturn;
	}

	myPacketLoss = packetLoss;
	myNumServers = numServers;
	mySeqNum = 0;
	myGlobalID = genRandom();


	dbg_printf("addr = %p\n", &mySock);

  	if (netInit(portNum, &mySock, &myAddr) < 0) {
  		RFError("netInit Fail.");
  		return ErrorReturn;
  	}

  	dbg_printf("Finish InitReplFs: mysock = %d\n", mySock);
  	dbg_printf("Finish InitReplFs: mtAddr = %p\n", myAddr);

  	// init log array
//  	myWriteLogs = (logEntry_t*)calloc(myNumServers, sizeof(logEntry_t));
//  	if (myWriteLogs == NULL) {
//  		RFError("No memory.");
//  		return ErrorReturn;
//  	}

  	if (checkServers(numServers) < 0) {
  		RFError("check servers Fail.");
  		return ErrorReturn;
  	}

  	dbg_printf("done check server\n");


  	return NormalReturn;
}

/* ------------------------------------------------------------------ */

int
OpenFile( char * fileName ) {
  int fd;

  ASSERT( fileName );

#ifdef DEBUG
  printf( "OpenFile: Opening File '%s'\n", fileName );
#endif

  fd = open( fileName, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );

#ifdef DEBUG
  if ( fd < 0 )
    perror( "OpenFile" );
#endif

  return( fd );
}

/* ------------------------------------------------------------------ */

int
WriteBlock( int fd, char * buffer, int byteOffset, int blockSize ) {
  //char strError[64];
  int bytesWritten;

  ASSERT( fd >= 0 );
  ASSERT( byteOffset >= 0 );
  ASSERT( buffer );
  ASSERT( blockSize >= 0 && blockSize < MaxBlockLength );

#ifdef DEBUG
  printf( "WriteBlock: Writing FD=%d, Offset=%d, Length=%d\n",
	fd, byteOffset, blockSize );
#endif

  if ( lseek( fd, byteOffset, SEEK_SET ) < 0 ) {
    perror( "WriteBlock Seek" );
    return(ErrorReturn);
  }

  if ( ( bytesWritten = write( fd, buffer, blockSize ) ) < 0 ) {
    perror( "WriteBlock write" );
    return(ErrorReturn);
  }

  return( bytesWritten );

}

/* ------------------------------------------------------------------ */

int
Commit( int fd ) {
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Commit: FD=%d\n", fd );
#endif

	/****************************************************/
	/* Prepare to Commit Phase			    */
	/* - Check that all writes made it to the server(s) */
	/****************************************************/

	/****************/
	/* Commit Phase */
	/****************/

  return( NormalReturn );

}

/* ------------------------------------------------------------------ */

int
Abort( int fd )
{
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Abort: FD=%d\n", fd );
#endif

  /*************************/
  /* Abort the transaction */
  /*************************/

  return(NormalReturn);
}

/* ------------------------------------------------------------------ */

int
CloseFile( int fd ) {

  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Close: FD=%d\n", fd );
#endif

	/*****************************/
	/* Check for Commit or Abort */
	/*****************************/

  if ( close( fd ) < 0 ) {
    perror("Close");
    return(ErrorReturn);
  }

  return(NormalReturn);
}

/* ------------------------------------------------------------------ */
int checkServers(int inputNumServers) {
	dbg_printf("Starting check available servers.\n");
	int res;
	int currentServers = 0;
	struct timeval startTime;
	struct timeval sendTime;
	pktGeneric_t pkt;

	fd_set fdmask;

	uint32_t serverids[inputNumServers];
	memset(serverids, 0, inputNumServers);
	// sendout Init packet
	pktHeader_t *p = (pktHeader_t*)alloca(sizeof(pktHeader_t));
	p->gid = htonl(myGlobalID);
	p->seqid = htonl(mySeqNum++);
	p->type = PKT_INIT;

	print_header(p, false);

	dbg_printf("myAddr = %p, p = %p\n", myAddr, p);
	if (sendto(mySock, (void *) p, sizeof(pktHeader_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		// TODO how to deal with this
		return ErrorReturn;
	}
	dbg_printf("== myAddr = %p, p = %p\n", myAddr, p);

	gettimeofday (&startTime, NULL);
	gettimeofday (&sendTime, NULL);

	int size = sizeof(Sockaddr);
	while (1) {
		dbg_printf("in while...\n");
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == inputNumServers) {
			dbg_printf("have enough servers.\n");
			return 1;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			RFError("No enough servers.");
			return -1;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(p, false);
			dbg_printf("mtsock = %d, myAddr = %p, p = %p\n", mySock, myAddr, p);
			if (sendto(mySock, (void *) p, sizeof(pktHeader_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				// TODO how to deal with this
				return ErrorReturn;
			}
			gettimeofday (&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

//		dbg_printf("... here time = %ld.%06ld \n", remain.tv_sec, remain.tv_usec);
		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
//			RFError("select failed");
			dbg_printf("continue from select.\n");
			continue;
		}

		// another way
//		if (setsockopt(mySock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
//			RFError("setsockopt failed (SO_RCVTIMEO)");
//			return -1;
//		}
//		dbg_printf("wait for packet. - %p\n", &pkt);

		// need to copy sockaddr, since recvfrom will fill it.
//		Sockaddr recv_addr;
//		memcpy(&recv_addr, myAddr, size);
//
//		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&recv_addr, (socklen_t *)&size);
		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			// If nothing received, try again,
//			RFError("recev error."); //TODO continue..
			dbg_printf("continue from recvfrom.\n");
			continue;
		}

		// process packet and update server number
		if (pkt.header.type != PKT_INITACK || pkt.header.gid == myGlobalID) {
			dbg_printf("Wrong type or mine\n");
			continue;
		}

		pktHeader_t *precv = (pktHeader_t*)&pkt.header;
		print_header(precv, true);
		uint32_t sgid = ntohl(precv->gid);
		int i = 0;
		for(; i < currentServers; i++) {
			if (serverids[i] == sgid) {
				dbg_printf("Duplicated\n");
				break;
			}
		}
		if (i == currentServers) {
			serverids[i] = sgid;
			currentServers++;
		}
		dbg_printf("currentServers=%d\n", currentServers);
	}

	return 1;
}

/* ------------------------------------------------------------------ */
void getRemainTime(struct timeval start, long int timeout, struct timeval *remain) {
	struct timeval diff, current;
	gettimeofday(&current, NULL);
	diff.tv_sec = current.tv_sec - start.tv_sec;
	diff.tv_usec = current.tv_usec - start.tv_usec;

	if (diff.tv_usec < 0) {
		diff.tv_sec--;
		diff.tv_usec += 1000000;
	}

	struct timeval to;
	to.tv_sec = timeout;
	to.tv_usec = 0;

	remain->tv_sec = to.tv_sec - diff.tv_sec;
	remain->tv_usec = to.tv_usec - diff.tv_usec;

	if (remain->tv_usec < 0) {
		remain->tv_sec--;
		remain->tv_usec += 1000000;
	}

	if ((remain->tv_sec < 0) ||
			((remain->tv_sec == 0) && (remain->tv_usec <= 0))) {
		remain->tv_sec = 0;
		remain->tv_usec = 0;
	}
}

