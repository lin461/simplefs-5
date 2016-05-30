/*
 * server.c
 *
 *  Created on: May 27, 2016
 *      Author: gy
 */

#include <fcntl.h>
#include <unistd.h>

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

/* ------------------------------------------------------------------ */
void cleanuplog() {
	if (slog == NULL) {
		return;
	}

	int i;
	for (i = 0; i < MAXWRITENUM; i++) {
		if (slog[i] != NULL) {
			free(slog[i]);
			slog[i] = NULL;
		}
	}
	return;
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
	dbg_printf("current file id = %u\n", sfileid);

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

//	uint32_t transnum = ntohl(pkt->transNum);
//	if (transnum != sTransNum) {
//		printf("not current transaction.\n");
//		return -1;
//	}

	uint8_t writenum = pkt->writeNum;
	if (writenum >= MAXWRITENUM) {
		printf("Max write number!\n");
		return -1;
	}
	uint16_t blocksize = ntohs(pkt->blocksize);
	if (blocksize > MAXBUFFERSIZE) {
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
int processAbort(pktCommon_t *pkt){
	int res;
	dbg_printf("== in processAbort ====\n");
	print_header(&pkt->header, true);

	uint32_t cseq = ntohl(pkt->header.seqid);
	uint32_t cgid = ntohl(pkt->header.gid);

	if (ntohl(pkt->fileid) != sfileid) {
		printf("not current open file.\n");
		return -1;
	}

//	uint32_t transnum = ntohl(pkt->transNum);
//	if (transnum != sTransNum) {
//		printf("not current transaction.\n");
//		return -1;
//	}

	// clean up log
	cleanuplog();

	// send out abortAck packet
	pktCommon_t *p = (pktCommon_t*)alloca(sizeof(pktCommon_t));
	p->header.gid = htonl(sGlobalID);
	p->header.seqid = htonl(sSeqNum);
	sSeqNum++;
	p->header.type = PKT_ABORTACK;

	p->fileid = htonl(sfileid);
	p->transNum = htonl(sTransNum);

	print_header(&p->header, false);
	dbg_printf("current file id = %u, trans# = %u\n", sfileid, sTransNum);

	if (sendto(ssock, (void *) p, sizeof(pktCommon_t),
			0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
		RFError("SendOut fail\n");
	}

	return 0;
}

/* ----------------------------------------------------------------------- */
int processCommit(pktCommon_t *pkt){
	char *fullfilename;
	int res;
	dbg_printf("== in processCommit ====\n");
	print_header(&pkt->header, true);

	uint32_t cseq = ntohl(pkt->header.seqid);
	uint32_t cgid = ntohl(pkt->header.gid);

	if (ntohl(pkt->fileid) != sfileid) {
		printf("not current open file.\n");
		return -1;
	}

//	uint32_t transnum = ntohl(pkt->transNum);
//	if (transnum != sTransNum) {
//		printf("not current transaction.\n");
//		return -1;
//	}

	int namelen = strlen(smountPath) + strlen(sfilename) + 2;
	fullfilename = (char *)calloc(namelen, sizeof(char));
	if (fullfilename == NULL) {
		RFError("No memory,\n");
		return -1;
	}
	snprintf(fullfilename, namelen, "%s/%s", smountPath, sfilename);
	fullfilename[namelen - 1] = '\0';

	int fd = open(fullfilename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		free(fullfilename);
		return -1;
	}

	// write log to file
	int i;
	for (i = 0; i < MAXWRITENUM; i++) {
		if (slog[i] == NULL) {
			continue;
		}
		if (lseek(fd, slog[i]->offset, SEEK_SET) < 0) {
			RFError("WriteBlock Seek");
			close(fd);
			free(fullfilename);
			return -1;
		}

		if ((res = write(fd, slog[i]->buffer, slog[i]->size)) < 0) {
			perror("WriteBlock write");
			close(fd);
			free(fullfilename);
			return -1;
		}
	}

	cleanuplog();

	// send out commitAck packet
	pktCommon_t *p = (pktCommon_t*)alloca(sizeof(pktCommon_t));
	p->header.gid = htonl(sGlobalID);
	p->header.seqid = htonl(sSeqNum);
	sSeqNum++;
	p->header.type = PKT_COMMITACK;

	p->fileid = htonl(sfileid);
	p->transNum = htonl(sTransNum);

	print_header(&p->header, false);
	dbg_printf("current file id = %u, trans# = %u\n", sfileid, sTransNum);

	if (sendto(ssock, (void *) p, sizeof(pktCommon_t),
			0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
		RFError("SendOut fail\n");
	}

	close(fd);
	free(fullfilename);


	return 0;
}

/* ----------------------------------------------------------------------- */
int processCommitReq(pktCommitReq_t *pkt){
	dbg_printf("== in processCommitReq====\n");
	print_logentry(slog);
	print_header(&pkt->header, true);
	uint32_t h_resendwrite = 0, l_resendwrite = 0;

	uint32_t cseq = ntohl(pkt->header.seqid);
	uint32_t cgid = ntohl(pkt->header.gid);

	if (ntohl(pkt->fileid) != sfileid) {
		printf("not current open file.\n");
		return -1;
	}

//	uint32_t transnum = ntohl(pkt->transNum);
//	if (transnum != sTransNum) {
//		printf("not current transaction.\n");
//		return -1;
//	}

	uint8_t totalwritenumber = pkt->totalWriteNum;
	int i;
	for (i = 0; i < totalwritenumber; i++) {
		if (slog[i] != NULL) {
			continue;
		}
		if (i < 32) { // update l_resendwrite
			l_resendwrite |= (1 << i);
		} else {
			h_resendwrite |= (1 << (i - 32));
		}
	}

	if (h_resendwrite == 0 && l_resendwrite == 0) { // send CommitYes
		pktCommon_t *p = (pktCommon_t*)alloca(sizeof(pktCommon_t));
		p->header.gid = htonl(sGlobalID);
		p->header.seqid = htonl(sSeqNum);
		sSeqNum++;
		p->header.type = PKT_COMMITYES;

		p->fileid = htonl(sfileid);
		p->transNum = htonl(sTransNum);

		print_header(&p->header, false);

		if (sendto(ssock, (void *) p, sizeof(pktCommon_t),
				0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
			RFError("SendOut fail\n");
			//TODO
		}

	} else { // send CommitResend
		pktCommitResend_t *p = (pktCommitResend_t*)alloca(sizeof(pktCommitResend_t));
		p->header.gid = htonl(sGlobalID);
		p->header.seqid = htonl(sSeqNum);
		sSeqNum++;
		p->header.type = PKT_COMMITRESEND;

		p->fileid = htonl(sfileid);
		p->transNum = htonl(sTransNum);
		p->H_writeNumReq = htonl(h_resendwrite);
		p->L_writeNumReq = htonl(l_resendwrite);

		print_header(&p->header, false);
		dbg_printf("HResend(%x)  LResend(%x)\n", h_resendwrite, l_resendwrite);

		if (sendto(ssock, (void *) p, sizeof(pktCommitResend_t),
				0, (struct sockaddr *) sAddr, sizeof(Sockaddr)) < 0) {
			RFError("SendOut fail\n");
			//TODO
		}
	}

	return 0;
}

/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	if (argc != 7) {
		return -1;
	}

	if ((strcmp("-port", argv[1]) |  strcmp("-mount", argv[3]) | strcmp("-drop", argv[5])) != 0)
		return -1;

	int size = sizeof(Sockaddr);
	char mountpath[MAXMAXPATHLEN];

	sport = atoi(argv[2]);
	strncpy(mountpath, argv[4], MAXMAXPATHLEN);
	int drop = atoi(argv[6]);

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
				processCommitReq(&pkt.commitreq);
				break;
			case PKT_COMMITYES:
				break;
			case PKT_COMMITRESEND:
				break;
			case PKT_COMMIT:
				processCommit(&pkt.common);
				break;
			case PKT_COMMITACK:
				break;
			case PKT_ABORT:
				processAbort(&pkt.common);
				break;
			case PKT_ABORTACK:
				break;
			default:
				break;
		}

	}
}
