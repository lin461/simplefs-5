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
static uint32_t	sTransNum;
static char *sfilename = NULL;
static char smountPath[MAXMAXPATHLEN];
static uint32_t scurFileID = -1;


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
	dbg_printf("sizeof mount %s!\n", smountPath);

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


/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	int size = sizeof(Sockaddr);
//	char *mountpath = "./mountpath";
	char mountpath[MAXMAXPATHLEN];

//	sport = atoi(argv[1]);
//	strncpy(mountpath, argv[2], MAXMAXPATHLEN);
	strncpy(mountpath, argv[1], MAXMAXPATHLEN);
	int drop = atoi(argv[3]);

	if (initServer(sport, mountpath, 0) < 0) {
		RFError("InitServer fail.");
		exit(-1);
	}

	int cc;
	pktGeneric_t pkt;

	// TODO check input
	while(1) {
		dbg_printf("myAddr = %p, p = %p\n", sAddr, &pkt);
//		cc = recvfrom(ssock, &pkt, sizeof(pkt), 0, (struct sockaddr *)sAddr, (socklen_t *)&size);
		cc = recvfrom(ssock, &pkt, sizeof(pkt), 0, NULL, NULL);
//		print_header(&pkt.header, true);
		if (cc <= 0) {
			RFError("recev error.");
			//TODO
			exit(-1);
		}

		switch(pkt.header.type) {
			case PKT_INIT:
				processPktInit(&pkt.header);
				break;
			case PKT_INITACK:
				break;
			case PKT_OPEN:
				break;
			case PKT_OPENACK:
				break;
			case PKT_WRITE:
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
