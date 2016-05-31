/*
 * network.c
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "network.h"

/* ----------------------------------------------------------------------- */

int netInit(in_port_t port, int *multisock, Sockaddr **groupAddr) {
	dbg_printf("=== entering netInit === \n");
	Sockaddr *nullAddr;
	int reuse;
	u_char ttl;
	struct ip_mreq mreq;
	int sock;

	nullAddr = (Sockaddr*) malloc(sizeof(Sockaddr));
	if (nullAddr == NULL) {
		RFError("No enough memory.");
		return -1;
	}
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		RFError("can't get socket");
		return -1;
	}

	/* SO_REUSEADDR allows more than one binding to the same
	 socket - you cannot have more than one player on one
	 machine without this */
	reuse = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		RFError("setsockopt failed (SO_REUSEADDR)");
		return -1;
	}

	nullAddr->sin_family = AF_INET;
	nullAddr->sin_addr.s_addr = htonl(INADDR_ANY);
	nullAddr->sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) nullAddr, sizeof(Sockaddr)) < 0) {
		RFError("netInit binding");
		return -1;
	}

	/* Multicast TTL:
	 0 restricted to the same host
	 1 restricted to the same subnet
	 32 restricted to the same site
	 64 restricted to the same region
	 128 restricted to the same continent
	 255 unrestricted

	 DO NOT use a value > 32. If possible, use a value of 1 when
	 testing.
	 */
	ttl = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		RFError("setsockopt failed (IP_MULTICAST_TTL)");
		return -1;
	}

	/* join the multicast group */
	mreq.imr_multiaddr.s_addr = htonl(MULTICAST_GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq,
			sizeof(mreq)) < 0) {
		RFError("setsockopt failed (IP_ADD_MEMBERSHIP)");
		return -1;
	}

	/* Get the multi-cast address ready to use in SendData()
	 calls. */
	nullAddr->sin_addr.s_addr = htonl(MULTICAST_GROUP);
	*groupAddr = (Sockaddr *) nullAddr;
//	dbg_printf("Finish netInit: mysock = %d\n", sock);
//	dbg_printf("Finish netInit: addr = %p\n", nullAddr);
	*multisock = sock;

	return 0;
}

/* ----------------------------------------------------------------------- */

void RFError(char *s)

{
	fprintf(stderr, "CS244BReplFs: %s\n", s);
	perror("CS244BReplFs");
}

/* ----------------------------------------------------------------------- */

bool isTimeout(struct timeval oldtime, long timeout) {
	struct timeval newtime;
	gettimeofday(&newtime, NULL);
	return ((newtime.tv_sec - oldtime.tv_sec) * 1000
			+ (newtime.tv_usec - oldtime.tv_usec) / 1000) >= timeout ?
			true : false;
}

/* ----------------------------------------------------------------------- */

void initPktHeader(pktHeader_t *pkt, uint16_t type, uint32_t gid,
		uint32_t seqid) {
	pkt->type = htons(type);
	pkt->gid = htonl(gid);
	pkt->seqid = htonl(seqid);
}

/* ----------------------------------------------------------------------- */
// Helper
void print_header(pktHeader_t *pkt, bool recv) {
	if (recv) {
		dbg_printf("--Receive-----------------------------------\n");
	} else {
		dbg_printf("--Send-------------------------------------\n");
	}
	dbg_printf("|\ttype\t=\t%d\n", pkt->type);
	dbg_printf("|\tgid\t=\t%u\n", ntohl(pkt->gid));
	dbg_printf("|\tseqid\t=\t%u\n", ntohl(pkt->seqid));
	dbg_printf("--------------------------------------------\n");
}

void print_writeBlk(pktWriteBlk_t *pkt, bool recv) {
	if (recv) {
		dbg_printf("--Receive-----------------------------------\n");
	} else {
		dbg_printf("--Send-------------------------------------\n");
	}
	dbg_printf("|\ttype\t=\t%d\n", pkt->header.type);
	dbg_printf("|\tgid\t=\t%u\n", ntohl(pkt->header.gid));
	dbg_printf("|\tseqid\t=\t%u\n", ntohl(pkt->header.seqid));
	dbg_printf("|\tfildid\t=\t%u\n", ntohl(pkt->fileid));
	dbg_printf("|\ttranc# = %u, write# = %d\n", ntohl(pkt->transNum), pkt->writeNum);
	dbg_printf("|\tsize = %u, offset = %u\n", ntohs(pkt->blocksize), ntohl(pkt->offset));
	dbg_printf("--------------------------------------------\n");
}

void print_logentry(logEntry_t **slog) {
	dbg_printf(" ================== LOG ======================== \n");
	if (slog == NULL) {
		return;
	}
	int i = 0;
	for (; i < MAXWRITENUM; i++) {
		if (slog[i] == NULL)
			continue;
		dbg_printf("\t[%d]\tsize(%d)\toffset(%u)\n", i, slog[i]->size, slog[i]->offset);
	}
	dbg_printf(" ================== END ======================== \n");
}

void print_servers(uint32_t *servers, int num) {
	int i;
	for (i = 0; i < num; i++) {
		if (servers[i] != 0) {
			printf("[%d]%u\n", i, servers[i]);
		}
	}
}

/* ----------------------------------------------------------------------- */
uint32_t genRandom() {
	  struct timeval	now;
	  gettimeofday (&now, NULL);
	  srand(now.tv_usec/16000);
	  return random();
}

