/*
 * network.h
 *
 *  Created on: May 25, 2016
 *      Author: gy
 */

#ifndef NETWORK_H_
#define NETWORK_H_


#define DEBUG

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif


#include <stdint.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXFILENAMELEN	128
#define MAXBUFFERSIZE	512
#define MAXMAXPATHLEN	128
#define MAXWRITENUM		64
#define MAXFILESIZE		1048576

#define WAIT_TIMEOUT			3000
#define RESEND_TIMEOUT			1000
#define RESEND_TIMEOUT_SEC		1

#define PKT_INIT			0
#define PKT_INITACK			1
#define PKT_OPEN			2
#define PKT_OPENACK			3
#define PKT_WRITE			4
#define	PKT_COMMITREQ		5
#define	PKT_COMMITYES		6
#define PKT_COMMITRESEND	7
#define PKT_COMMIT			8
#define PKT_COMMITACK		9
#define PKT_ABORT			10
#define PKT_ABORTACK		11

#define MULTICAST_GROUP       0xe0010101

typedef	struct sockaddr_in			Sockaddr;

typedef struct HeaderPkt {
	uint8_t type;
	uint32_t gid;
	uint32_t seqid;
} pktHeader_t;

typedef struct OpenPkt {
	pktHeader_t header;
	uint32_t fileid;
	char filename[MAXFILENAMELEN];
} pktOpen_t;

typedef struct OpenACKPkt {
	pktHeader_t header;
	uint32_t fileid;
} pktOpenACK_t;

typedef struct WriteBlkPkt {
	pktHeader_t header;
	uint8_t	writeNum;
	uint16_t blocksize;
	uint32_t fileid;
	uint32_t transNum;
	uint32_t offset;
	char buffer[MAXBUFFERSIZE];
} pktWriteBlk_t;

typedef struct CommitReqPkt {
	pktHeader_t header;
	uint8_t totalWriteNum;
	uint32_t fileid;
	uint32_t transNum;
} pktCommitReq_t;

typedef struct CommonPkt { //CommitYes, CommitACK, AbortACK
	pktHeader_t header;
	uint32_t fileid;
	uint32_t transNum;
} pktCommon_t;

typedef struct CommitAbortPkt { //Commit, Abort
	pktHeader_t header;
	uint8_t	closeflag;
	uint32_t fileid;
	uint32_t transNum;
} pktCommitAbort_t;

typedef struct CommitRePkt {
	pktHeader_t header;
	uint32_t fileid;
	uint32_t transNum;
	uint32_t H_writeNumReq;
	uint32_t L_writeNumReq;
} pktCommitResend_t;

typedef union GenericPkt {
	pktHeader_t header;
	pktOpen_t	open;
	pktOpenACK_t	openack;
	pktWriteBlk_t	writeblock;
	pktCommitReq_t	commitreq;
	pktCommon_t		common;
	pktCommitAbort_t commitabort;
	pktCommitResend_t	commitresend;
} pktGeneric_t;


/*****************************/
typedef struct logEntry {
	uint16_t offset;
	uint16_t size;
	char buffer[MAXBUFFERSIZE];
} logEntry_t;


void RFError(char *s);
//void getHostName(char *prompt, char **hostName, Sockaddr *hostAddr);
//Sockaddr *resolveHost(register char *name);
int netInit(in_port_t port, int *multisock, Sockaddr **groupAddr);
bool isTimeout(struct timeval oldtime, long timeout);
uint32_t genRandom();

void print_header(pktHeader_t *pkt, bool recv);
void print_writeBlk(pktWriteBlk_t *pkt, bool recv);
void print_logentry(logEntry_t **slog);

#endif /* NETWORK_H_ */
