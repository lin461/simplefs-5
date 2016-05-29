/*
 * server.c
 *
 *  Created on: May 27, 2016
 *      Author: gy
 */

#include "network.h"

static int	ssock = -1;
static Sockaddr	*sAddr = NULL;
static int 	sport = 44054;
static int 		sPacketLoss = -1;
static uint32_t 	sSeqNum	= -1;
static uint32_t	sGlobalID;
static uint32_t	sTransNum = 0;
static char *sfilename = NULL;
static char smountPath[MAXMAXPATHLEN];
static uint32_t sfileid = -1;
static logEntry_t *slog[MAXWRITENUM]; // array of pointers to struct entry


/* ----------------------------------------------------------------------- */
int initServer(unsigned short portNum, char *mount, int drop) {
	if (portNum < 0 || drop < 0 || drop > 100 || mount == NULL) {
		RFError("Invalid Params.");
		return -1;
	}

	if (sAddr != NULL || ssock != -1 || sPacketLoss != -1) {
		RFError("Already initialized.");
		return -1;
	}

	sport = portNum;
	sPacketLoss = drop;
	sSeqNum = 0;
	sGlobalID = genRandom();


	strncpy(smountPath, mount, MAXMAXPATHLEN);
	dbg_printf("mount path - %s!\n", smountPath);

	int res = mkdir(smountPath, 0777);
	if (res == -1) {
		RFError("machine already in use");
		return -1;
	}

	if (netInit(sport, &ssock, &sAddr) < 0) {
		RFError("netinit Fails.");
		return -1;
	}

  	dbg_printf("Finish InitReplFs: mysock = %d\n", ssock);
  	dbg_printf("Finish InitReplFs: myAddr = %p\n", sAddr);

	return 1;
}

/* ----------------------------------------------------------------------- */
void processPktInit(pktHeader_t *pkt) {
	dbg_printf("== in processPktInit====\n");
	print_header(pkt, true);

	uint32_t cseq = ntohl(pkt->seqid);
	uint32_t cgid = ntohl(pkt->gid);

	pktHeader_t *p = (pktHeader_t*)alloca(sizeof(pktHeader_t));
	p->gid = htonl(sGlobalID);
	p->seqid = htonl(sSeqNum);
	sSeqNum++;
	p->type = PKT_INITACK;

	print_header(p, false);

	if (sendto(ssock, (void *) p, sizeof(pktHeader_t),
			0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
		RFError("SendOut fail\n");
		//TODO
	}
}

/* ----------------------------------------------------------------------- */
bool isLogEmpty() {
	if (slog == NULL) {
		return true;
	}
	int i = 0;
	for (; i < MAXWRITENUM; i++) {
		if (slog[i] != NULL) {
			return false;
		}
	}

	return true;
}

/* ----------------------------------------------------------------------- */
int processPktOpen(pktOpen_t *pkt) {
	dbg_printf("== in processPktOpen====\n");
	print_header(&pkt->header, true);

	uint32_t cseq = ntohl(pkt->header.seqid);
	uint32_t cgid = ntohl(pkt->header.gid);

	if (!isLogEmpty()) {
		dbg_printf("have opening file.\n");
		return -1;
	}
	if (sfilename != NULL)
		free(sfilename);
	sfilename = NULL;
	sfileid = -1;

	sfilename = (char *)calloc(MAXFILENAMELEN, sizeof(char));
	if (sfilename == NULL) {
		RFError("no enough memory!\n");
		return -1;
	}
	strncpy((char *)sfilename, pkt->filename, MAXFILENAMELEN);
	sfileid = ntohl(pkt->fileid);


	pktOpenACK_t *p = (pktOpenACK_t*)alloca(sizeof(pktOpenACK_t));
	p->header.gid = htonl(sGlobalID);
	p->header.seqid = htonl(sSeqNum);
	sSeqNum++;
	p->header.type = PKT_OPENACK;

	p->fileid = htonl(sfileid);

	print_header(&p->header, false);

	if (sendto(ssock, (void *) p, sizeof(pktOpenACK_t),
			0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
		RFError("SendOut fail\n");
		//TODO
	}

	// init slog

	// init write number



	return sfileid;
}

/* ----------------------------------------------------------------------- */
int processWrite(pktWriteBlk_t *pkt) {
//	print_header(&pkt->header, true);
	print_writeBlk(pkt, true);
	uint32_t fd = ntohl(pkt->fileid);
	if (fd != sfileid) {
		printf("not current open file.\n");
		return -1;
	}

	uint32_t transnum = ntohl(pkt->transNum);
	if (transnum != sTransNum) {
		printf("not current transaction.\n");
		return -1;
	}

	uint8_t writenum = pkt->writeNum;
	if (writenum >= MAXWRITENUM) {
		printf("Max write number!\n");
		return -1;
	}
	uint16_t blocksize = ntohs(pkt->blocksize);
	if (blocksize >= MAXBUFFERSIZE) {
		printf("Max butter size!\n");
		return -1;
	}

	uint32_t byteoffset = ntohl(pkt->offset);
	if (byteoffset >= MAXFILESIZE) {
		printf("Max file size!\n");
		return -1;
	}

	// write to slog
	logEntry_t* log = (logEntry_t*)calloc(1, sizeof(logEntry_t));
	log->offset = byteoffset;
	log->size = blocksize;
	memcpy((char *) &log->buffer, &pkt->buffer, blocksize);
	if (slog[writenum] != NULL) {
		free(slog[writenum]);
	}
	slog[writenum] = log;

//	print_logentry(slog);
	return 0;
}
/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	int size = sizeof(Sockaddr);
	char mountpath[MAXMAXPATHLEN];

//	sport = atoi(argv[1]);
//	strncpy(mountpath, argv[2], MAXMAXPATHLEN);
	strncpy(mountpath, argv[1], MAXMAXPATHLEN);
	int drop = atoi(argv[2]);

	if (initServer(sport, mountpath, drop) < 0) {
		RFError("InitServer fail.");
		exit(-1);
	}

	int cc;
	pktGeneric_t pkt;

	// TODO check input
	while(1) {
//		cc = recvfrom(ssock, &pkt, sizeof(pkt), 0, (struct sockaddr *)sAddr, (socklen_t *)&size);
		uint32_t rand = genRandom();
		cc = recvfrom(ssock, &pkt, sizeof(pkt), 0, NULL, NULL);
		if (cc <= 0) {
			RFError("recev error.");
			//TODO
			continue;
		}
		if ((rand % 100) < sPacketLoss) {
			dbg_printf("packet dropped.\n");
			continue;
		}

		switch(pkt.header.type) {
			case PKT_INIT:
				processPktInit(&pkt.header);
				break;
			case PKT_INITACK:
				break;
			case PKT_OPEN:
				processPktOpen(&pkt.open);
				break;
			case PKT_OPENACK:
				break;
			case PKT_WRITE:
				processWrite(&pkt.writeblock);
				break;
			case PKT_COMMITREQ:
				break;
			case PKT_COMMITYES:
				break;
			case PKT_COMMITRESEND:
				break;
			case PKT_COMMIT:
				break;
			case PKT_COMMITACK:
				break;
			case PKT_ABORT:
				break;
			case PKT_ABORTACK:
				break;
			default:
				break;
		}

	}
}
