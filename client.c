/****************/
/* Your Name	Ying Gao*/
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


static uint32_t 	myTransNum = -1;
static uint8_t		myWriteNum = -1;
static uint32_t 	myFileID = -1;
static int 		myNumServers = -1;
static int 		myPacketLoss = -1;

static char	*myFilename = NULL;

static uint32_t 	mySeqNum	= -1;
static uint32_t	myGlobalID;

static logEntry_t *myLogs[MAXWRITENUM];

/* ------------------------------------------------------------------ */

int
InitReplFs( unsigned short portNum, int packetLoss, int numServers ) {
#ifdef DEBUG
  printf( "\n\nInitReplFs: Port number %d, packet loss %d percent, %d servers\n",
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
	myTransNum = 0;


	dbg_printf("addr = %p\n", &mySock);

  	if (netInit(portNum, &mySock, &myAddr) < 0) {
  		RFError("netInit Fail.");
  		return ErrorReturn;
  	}

  	dbg_printf("Finish InitReplFs: mysock = %d\n", mySock);
  	dbg_printf("Finish InitReplFs: mtAddr = %p\n", myAddr);


  	if (checkServers(numServers) < 0) {
  		return ErrorReturn;
  	}


  	return NormalReturn;
}

/* ------------------------------------------------------------------ */

int OpenFile(char * fileName) {
	int fd;

	ASSERT(fileName);

#ifdef DEBUG
	printf("\n\nOpenFile: Opening File '%s'\n", fileName);
#endif
    // If this file is already opened
    if ((myFilename != NULL) && strcmp(fileName, myFilename) == 0) {
        return myFileID;
    }
	// check if has opening file
	if (myFileID != -1 || myFilename != NULL || !ismyLogEmpty()) {
		RFError("Has file opened.");
		return -1;
	}
	myFilename = (char *)calloc(MAXFILENAMELEN, sizeof(char));
	if (myFilename == NULL) {
		RFError("no enough memory!\n");
		return -1;
	}
	strncpy(myFilename, fileName, MAXFILENAMELEN);
	myFilename[MAXFILENAMELEN - 1] = '\0';

	myFileID = genRandom();

	if (openfilereq() < 0) {
		return -1;
	}

	// init write number;
	myWriteNum = 0;


//  fd = open( fileName, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
//
//#ifdef DEBUG
//  if ( fd < 0 )
//    perror( "OpenFile" );
//#endif
//
//  return( fd );
	return myFileID;
}

/* ------------------------------------------------------------------ */

int WriteBlock(int fd, char * buffer, int byteOffset, int blockSize) {
	//char strError[64];
	int bytesWritten;

	ASSERT(fd >= 0);
	ASSERT(byteOffset >= 0);
	ASSERT(buffer);
	ASSERT(blockSize >= 0 && blockSize < MaxBlockLength);

#ifdef DEBUG
	printf("WriteBlock: Writing FD=%d, Offset=%d, Length=%d\n", fd, byteOffset,
			blockSize);
#endif

	if (myFileID == -1) {
		dbg_printf("No open file. \n");
		return -1;
	}

	if (myFileID != fd) {
		dbg_printf("Different open file.\n");
		return -1;
	}

	// starting write
	// add to log
	logEntry_t* log = (logEntry_t*) calloc(1, sizeof(logEntry_t));
	log->offset = byteOffset;
	log->size = blockSize;
	memcpy((char *) &log->buffer, buffer, blockSize);
	myLogs[myWriteNum] = log;

	// write to packet
	pktWriteBlk_t *p = (pktWriteBlk_t *) alloca(sizeof(pktWriteBlk_t));
	p->header.gid = htonl(myGlobalID);
	p->header.seqid = htonl(mySeqNum);
	p->header.type = PKT_WRITE;
	mySeqNum++;
	strncpy((char *) p->buffer, buffer, blockSize);
	p->fileid = htonl(myFileID);
	p->blocksize = htons(blockSize);
	p->offset = htonl(byteOffset);
	p->transNum = htonl(myTransNum);
	p->writeNum = myWriteNum;

//	print_header(&p->header, false);
	print_writeBlk(p, false);

	//// For resend testing
//	if (myWriteNum == 2) {
//		myWriteNum++;
//		return blockSize;
//	}

	int res = sendto(mySock, (void *) p, sizeof(pktWriteBlk_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr));
	if (res <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	// update current write number
	myWriteNum++;


//	if (lseek(fd, byteOffset, SEEK_SET) < 0) {
//		perror("WriteBlock Seek");
//		return (ErrorReturn);
//	}
//
//	if ((bytesWritten = write(fd, buffer, blockSize)) < 0) {
//		perror("WriteBlock write");
//		return (ErrorReturn);
//	}
//
//	return (bytesWritten);

//	print_logentry(myLogs);
	return blockSize;

}

/* ------------------------------------------------------------------ */

int Commit(int fd) {
	ASSERT(fd >= 0);

#ifdef DEBUG
	printf("Commit: FD=%d\n", fd);
#endif

	if (myFileID == -1) {
		RFError("No file opening now.\n");
		return -1;
	}

	if (fd != myFileID) {
		RFError("Invalid file handle.\n");
		return -1;
	}

	if (ismyLogEmpty()) {
		printf("No outstanding changes.\n");
		return 0;
	}
	/****************************************************/
	/* Prepare to Commit Phase			    */
	/* - Check that all writes made it to the server(s) */
	/****************************************************/
	commitreq(fd);

	/****************/
	/* Commit Phase */
	/****************/
	commitPhase2(fd);

	// clean up log
	cleanupmylog();
	myTransNum++;
	myWriteNum = 0;

	return (NormalReturn);

}

/* ------------------------------------------------------------------ */

int Abort(int fd) {
	ASSERT(fd >= 0);

#ifdef DEBUG
	printf("Abort: FD=%d\n", fd);
#endif

	if (myFileID == -1) {
		RFError("No file opening now.\n");
		return -1;
	}

	if (fd != myFileID) {
		RFError("Invalid file handle.\n");
		return -1;
	}

	if (ismyLogEmpty()) {
		printf("No outstanding changes.\n");
		return 0;
	}

	/*************************/
	/* Abort the transaction */
	/*************************/
	aborting(fd);

	// clean up log
	cleanupmylog();
	myTransNum++;
	myWriteNum = 0;

	return (NormalReturn);
}

/* ------------------------------------------------------------------ */

int CloseFile(int fd) {

	ASSERT(fd >= 0);

#ifdef DEBUG
	printf("Close: FD=%d\n", fd);
#endif

	/*****************************/
	/* Check for Commit or Abort */
	/*****************************/

//  if ( close( fd ) < 0 ) {
//    perror("Close");
//    return(ErrorReturn);
//  }

	Commit(fd);

	// close file
	if (myFilename != NULL) {
		free(myFilename);
		myFilename = NULL;
	}
	myFileID = -1;
	myTransNum = 0;
	myWriteNum = 0;

	return (NormalReturn);
}

/* ------------------------------------------------------------------ */
int aborting(int fd) {
	dbg_printf("\n\nStarting aborting.\n");
	int res;
	int currentServers = 0;
	struct timeval startTime, sendTime;
	pktGeneric_t pkt;
	fd_set fdmask;

	uint32_t serverids[myNumServers];
	memset(serverids, 0, myNumServers);

	// send out abort packet
	pktCommon_t *p = (pktCommon_t *) alloca(sizeof(pktCommon_t));
	p->header.gid = htonl(myGlobalID);
	p->header.seqid = htonl(mySeqNum);
	p->header.type = PKT_ABORT;
	mySeqNum++;
	p->fileid = htonl(myFileID);
	p->transNum = htonl(myTransNum);

	print_header(&p->header, false);
	dbg_printf("file(%u)\t trans#(%u) \t\n", myFileID, myTransNum);

	if (sendto(mySock, (void *) p, sizeof(pktCommon_t), 0,
			(struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	gettimeofday(&startTime, NULL);
	gettimeofday(&sendTime, NULL);

	int size = sizeof(Sockaddr);
	while (1) {
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == myNumServers) {
			dbg_printf("have enough servers.\n");
			return 0;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			RFError(
					"No enough servers. Abort truncate server number..");
			// update current server number
			myNumServers = currentServers;
			print_servers(serverids, myNumServers);
			return 0;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(&p->header, false);
			dbg_printf("file(%u)\t trans#(%u) \t\n", myFileID, myTransNum);
			if (sendto(mySock, (void *) p, sizeof(pktCommon_t), 0,
					(struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				return ErrorReturn;
			}
			gettimeofday(&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

//		dbg_printf("... here time = %ld.%06ld \n", remain.tv_sec, remain.tv_usec);
		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
			dbg_printf("continue from select.\n");
			continue;
		}

		uint32_t rand = genRandom();

		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			dbg_printf("continue from recvfrom.\n");
			continue;
		}
		// drop packet
		if ((rand % 100) < myPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		uint32_t pktgid = ntohl(pkt.header.gid);
		// process packet and update server number
		if (pkt.header.type != PKT_ABORTACK || pktgid == myGlobalID) {
//			dbg_printf("skip Wrong type or mine\n");
			continue;
		}

		pktCommon_t *precv = (pktCommon_t*) &pkt.common;

		uint32_t pktfileid = ntohl(precv->fileid);
		if (pktfileid != myFileID) {
			dbg_printf("Not the same file I requested.\n");
			continue;
		}

//		uint32_t transnum = ntohl(precv->transNum);
//		if (transnum != myTransNum) {
//			dbg_printf("Not the same Transaction Number.\n");
//			continue;
//		}

		print_header(&precv->header, true);

		// count current servers
		uint32_t sgid = ntohl(precv->header.gid);
		currentServers = countServers(serverids, sgid);
		dbg_printf("currentServers=%d\n", currentServers);
	}

	return 0;
}

/* ------------------------------------------------------------------ */
int commitResend(pktCommitResend_t *pkt) {
	uint32_t writebitmap[2];
	writebitmap[0] = ntohl(pkt->L_writeNumReq);
	writebitmap[1] = ntohl(pkt->H_writeNumReq);
	int i, j;
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 32; j++) {
			int bit = (writebitmap[i] >> j) & 1;
			if (bit == 0) {
				continue;
			}
			uint8_t index = i * 32 + j;
			if (myLogs[index] == NULL) {
				continue;
			}
			logEntry_t* log = myLogs[index];
			// write to packet
			pktWriteBlk_t *p = (pktWriteBlk_t *) alloca(sizeof(pktWriteBlk_t));
			p->header.gid = htonl(myGlobalID);
			p->header.seqid = htonl(mySeqNum);
			p->header.type = PKT_WRITE;
			mySeqNum++;
			strncpy((char *) p->buffer, log->buffer, log->size);
			p->fileid = htonl(myFileID);
			p->blocksize = htons(log->size);
			p->offset = htonl(log->offset);
			p->transNum = htonl(myTransNum);
			p->writeNum = index;;

			print_writeBlk(p, false);

			int res = sendto(mySock, (void *) p, sizeof(pktWriteBlk_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr));
			if (res <= 0) {
				RFError("SendOut fail\n");
				return ErrorReturn;
			}
		}
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*
 * Still consider success if currentNumberServers < myNumServers,
 * and update myNumServers
 */
int commitreq(int fd) {
	dbg_printf("\n\nStarting commitreq.\n");
	int res;
	int currentServers = 0;
	struct timeval startTime, sendTime;
	pktGeneric_t pkt;
	fd_set fdmask;

	uint32_t serverids[myNumServers];
	memset(serverids, 0, myNumServers);

	// send out commitreq packet
	pktCommitReq_t *p = (pktCommitReq_t *)alloca(sizeof(pktCommitReq_t));
	p->header.gid = htonl(myGlobalID);
	p->header.seqid = htonl(mySeqNum);
	p->header.type = PKT_COMMITREQ;
	mySeqNum++;
	p->fileid = htonl(myFileID);
	p->totalWriteNum = myWriteNum; // how many writes.
	p->transNum = htonl(myTransNum);

	print_header(&p->header, false);
	dbg_printf("file(%u)\t trans#(%u) \t totalWrite#(%d)\n", myFileID, myTransNum, myWriteNum);

	if (sendto(mySock, (void *) p, sizeof(pktCommitReq_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	gettimeofday (&startTime, NULL);
	gettimeofday (&sendTime, NULL);

	int size = sizeof(Sockaddr);
	while (1) {
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == myNumServers) {
			dbg_printf("have enough servers.\n");
			return 0;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			RFError("No enough servers. Commit 1st phase truncate server number..");
			// update current server number
			myNumServers = currentServers;
			print_servers(serverids, myNumServers);
			return 0;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(&p->header, false);
			dbg_printf("file(%u)\t trans#(%u) \t totalWrite#(%d)\n", myFileID, myTransNum, myWriteNum);
			if (sendto(mySock, (void *) p, sizeof(pktCommitReq_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				return ErrorReturn;
			}
			gettimeofday (&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
			dbg_printf("continue from select.\n");
			continue;
		}

		uint32_t rand = genRandom();

		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			dbg_printf("continue from recvfrom.\n");
			continue;
		}
		// drop packet
		if ((rand % 100) < myPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		uint32_t pktgid = ntohl(pkt.header.gid);
		// process packet and update server number
		if (pktgid == myGlobalID) {
		//	dbg_printf("Drop self packet\n");
			continue;
		}

		uint8_t type = pkt.header.type;
		if (type == PKT_COMMITYES) {
			dbg_printf("receive COMMIT YES packet.\n");
			pktCommon_t *precv = (pktCommon_t*)&pkt.common;

			uint32_t pktfileid = ntohl(precv->fileid);
			if (pktfileid != myFileID) {
				dbg_printf("Not the same file I requested.\n");
				continue;
			}

//			uint32_t transnum = ntohl(precv->transNum);
//			if (transnum != myTransNum) {
//				dbg_printf("Not the same Transaction Number.\n");
//				continue;
//			}

			print_header(&precv->header, true);

			// count current servers
			uint32_t sgid = ntohl(precv->header.gid);
			currentServers = countServers(serverids, sgid);
			dbg_printf("currentServers=%d\n", currentServers);
		} else if (type == PKT_COMMITRESEND) {
			dbg_printf("receive COMMIT RESEND packet.\n");
			pktCommitResend_t *precv = (pktCommitResend_t*)&pkt.commitresend;

			uint32_t pktfileid = ntohl(precv->fileid);
			if (pktfileid != myFileID) {
				dbg_printf("Not the same file I requested.\n");
				continue;
			}

//			uint32_t transnum = ntohl(precv->transNum);
//			if (transnum != myTransNum) {
//				dbg_printf("Not the same Transaction Number.\n");
//				continue;
//			}

			commitResend(precv);
		}
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*
 * Still consider success if currentNumberServers < myNumServers,
 * and update myNumServers
 */
int commitPhase2(int fd) {
	dbg_printf("\n\nStarting commitPhase2.\n");
	int res;
	int currentServers = 0;
	struct timeval startTime, sendTime;
	pktGeneric_t pkt;
	fd_set fdmask;

	uint32_t serverids[myNumServers];
	memset(serverids, 0, myNumServers);

	// send out commit packet
	pktCommon_t *p = (pktCommon_t *)alloca(sizeof(pktCommon_t));
	p->header.gid = htonl(myGlobalID);
	p->header.seqid = htonl(mySeqNum);
	p->header.type = PKT_COMMIT;
	mySeqNum++;
	p->fileid = htonl(myFileID);
	p->transNum = htonl(myTransNum);

	print_header(&p->header, false);
	dbg_printf("file(%u)\t trans#(%u) \t\n", myFileID, myTransNum);

	if (sendto(mySock, (void *) p, sizeof(pktCommon_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	gettimeofday (&startTime, NULL);
	gettimeofday (&sendTime, NULL);

	int size = sizeof(Sockaddr);
	while (1) {
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == myNumServers) {
			dbg_printf("have enough servers.\n");
			return 0;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			RFError("No enough servers. Commit 2st phase truncate server number..");
			// update current server number
			myNumServers = currentServers;
			print_servers(serverids, myNumServers);
			return 0;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(&p->header, false);
			dbg_printf("file(%u)\t trans#(%u) \t\n", myFileID, myTransNum);
			if (sendto(mySock, (void *) p, sizeof(pktCommon_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				return ErrorReturn;
			}
			gettimeofday (&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

//		dbg_printf("... here time = %ld.%06ld \n", remain.tv_sec, remain.tv_usec);
		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
			dbg_printf("continue from select.\n");
			continue;
		}

		uint32_t rand = genRandom();

		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			dbg_printf("continue from recvfrom.\n");
			continue;
		}
		// drop packet
		if ((rand % 100) < myPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		uint32_t pktgid = ntohl(pkt.header.gid);
		// process packet and update server number
		if (pkt.header.type != PKT_COMMITACK || pktgid == myGlobalID) {
//			dbg_printf("skip Wrong type or mine\n");
			continue;
		}

		pktCommon_t *precv = (pktCommon_t*)&pkt.common;

		uint32_t pktfileid = ntohl(precv->fileid);
		if (pktfileid != myFileID) {
			dbg_printf("Not the same file I requested.\n");
			continue;
		}

//		uint32_t transnum = ntohl(precv->transNum);
//		if (transnum != myTransNum) {
//			dbg_printf("Not the same Transaction Number.\n");
//			continue;
//		}

		print_header(&precv->header, true);

		// count current servers
		uint32_t sgid = ntohl(precv->header.gid);
		currentServers = countServers(serverids, sgid);
		dbg_printf("currentServers=%d\n", currentServers);
	}

	return 0;
}


/* ------------------------------------------------------------------ */
int openfilereq() {
	dbg_printf("Starting openfile.\n");
	int res;
	int currentServers = 0;
	struct timeval startTime, sendTime;
	pktGeneric_t pkt;
	fd_set fdmask;

	uint32_t serverids[myNumServers];
	memset(serverids, 0, myNumServers);

	// send out open packet
	pktOpen_t *p = (pktOpen_t *)alloca(sizeof(pktOpen_t));
	p->header.gid = htonl(myGlobalID);
	p->header.seqid = htonl(mySeqNum);
	p->header.type = PKT_OPEN;
	mySeqNum++;
	strncpy((char *)p->filename, myFilename, MAXFILENAMELEN);
	p->fileid = htonl(myFileID);

	print_header(&p->header, false);

	if (sendto(mySock, (void *) p, sizeof(pktOpen_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	gettimeofday (&startTime, NULL);
	gettimeofday (&sendTime, NULL);

	int size = sizeof(Sockaddr);

	while (1) {
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == myNumServers) {
			dbg_printf("have enough servers.\n");
			return 0;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			RFError("No enough servers. close fileID..");
			// close file
			myFileID = -1;
			if (myFilename != NULL)
				free(myFilename);
			myFilename = NULL;
			return -1;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(&p->header, false);
			if (sendto(mySock, (void *) p, sizeof(pktOpen_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				return ErrorReturn;
			}
			gettimeofday (&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

//		dbg_printf("... here time = %ld.%06ld \n", remain.tv_sec, remain.tv_usec);
		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
			dbg_printf("continue from select.\n");
			continue;
		}

		uint32_t rand = genRandom();

		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			dbg_printf("continue from recvfrom.\n");
			continue;
		}
		// drop packet
		if ((rand % 100) < myPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		uint32_t pktgid = ntohl(pkt.header.gid);
		// process packet and update server number
		if (pkt.header.type != PKT_OPENACK || pktgid == myGlobalID) {
//			dbg_printf("Wrong type or mine\n");
			continue;
		}

		uint32_t pktfileid = ntohl(pkt.openack.fileid);
		// drop if not the file requested to open
		if (pktfileid != myFileID) {
			dbg_printf("Not my open request.\n");
			continue;
		}

		pktOpenACK_t *precv = (pktOpenACK_t*)&pkt.openack;
		print_header(&precv->header, true);

		// count current servers
		uint32_t sgid = ntohl(precv->header.gid);
		currentServers = countServers(serverids, sgid);
		dbg_printf("currentServers=%d\n", currentServers);
	}

	return 0;
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
	p->seqid = htonl(mySeqNum);
	p->type = PKT_INIT;
	mySeqNum++;

	print_header(p, false);

	if (sendto(mySock, (void *) p, sizeof(pktHeader_t),
			0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
		RFError("SendOut fail\n");
		return ErrorReturn;
	}

	gettimeofday (&startTime, NULL);
	gettimeofday (&sendTime, NULL);

	int size = sizeof(Sockaddr);
	while (1) {
		FD_ZERO(&fdmask);
		FD_SET(mySock, &fdmask);

		if (currentServers == inputNumServers) {
			dbg_printf("have enough servers.\n");
			return 0;
		}
		if (isTimeout(startTime, WAIT_TIMEOUT)) {
			printf("Init: No enough servers (%d).\n", currentServers);
                        print_servers(serverids, myNumServers);
			return -1;
		}
		if (isTimeout(sendTime, RESEND_TIMEOUT)) { //Resend
			dbg_printf("timeout resend.\n");
			print_header(p, false);
			if (sendto(mySock, (void *) p, sizeof(pktHeader_t),
					0, (struct sockaddr *) myAddr, sizeof(Sockaddr)) <= 0) {
				RFError("SendOut fail");
				return ErrorReturn;
			}
			gettimeofday (&sendTime, NULL);
		}

		struct timeval remain;
		getRemainTime(sendTime, RESEND_TIMEOUT_SEC, &remain);

//		dbg_printf("... here time = %ld.%06ld \n", remain.tv_sec, remain.tv_usec);
		if (select(32, &fdmask, NULL, NULL, &remain) <= 0) {
			dbg_printf("continue from select.\n");
			continue;
		}

		uint32_t rand = genRandom();

		res = recvfrom(mySock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (res <= 0) {
			dbg_printf("continue from recvfrom.\n");
			continue;
		}
		// drop packet
		if ((rand % 100) < myPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		uint32_t pktgid = ntohl(pkt.header.gid);
		// process packet and update server number
		if (pkt.header.type != PKT_INITACK || pktgid == myGlobalID) {
//			dbg_printf("Wrong type or mine\n");
			continue;
		}

		// count current servers
		pktHeader_t *precv = (pktHeader_t*)&pkt.header;
		print_header(precv, true);
		uint32_t sgid = ntohl(precv->gid);
		currentServers = countServers(serverids, sgid);
		dbg_printf("currentServers=%d\n", currentServers);
	}

	return 0;
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
	to.tv_sec = 0;
	to.tv_usec = 500000;

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

/* ------------------------------------------------------------------ */
bool ismyLogEmpty() {
	if (myLogs == NULL) {
		return true;
	}
	int i = 0;
	for (; i < MAXWRITENUM; i++) {
		if (myLogs[i] != NULL) {
			return false;
		}
	}

	return true;
}

/* ------------------------------------------------------------------ */
void cleanupmylog() {
	if (myLogs == NULL) {
		return;
	}

	int i;
	for (i = 0; i < MAXWRITENUM; i++) {
		if (myLogs[i] != NULL) {
			free(myLogs[i]);
			myLogs[i] = NULL;
		}
	}
	return;
}

/* ------------------------------------------------------------------ */
int countServers(uint32_t *servers, uint32_t gid) {
	int i = 0;
	for(; i < myNumServers; i++) {
		if (servers[i] == gid) {
			dbg_printf("Duplicated\n");
			break;
		}
		if (servers[i] == 0) {
			servers[i] = gid;
			break;
		}
	}
	int cnt = 0;
	while (cnt < myNumServers && servers[cnt] != 0) {
		cnt++;
	}
	return cnt;
}
