/*
 * network.c
 *
 *  Created on: May 25, 2016
 *      Author: gy
 */

#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "network.h"

///* get hostname and host socket */
//void
//getHostName(char *prompt, char **hostName, Sockaddr *hostAddr)
//{
//	char		buf[128];
//	Sockaddr	*AddrTemp;
//
//	buf[0] = '\0';
//	for (AddrTemp = (Sockaddr *)NULL; AddrTemp == (Sockaddr *)NULL; )
//	  {
//		printf("%s %s: " , prompt, "(CR for any host)");
//		fgets(buf, sizeof(buf)-1, stdin);
//		if (strlen(buf) == 0)
//			break;
//		*hostName = (char*)malloc((unsigned) (strlen(buf) + 1));
//		if (*hostName == NULL)
//			RFError("no mem for hostName");
//		strcpy(*hostName, buf);
//
//		/* check for valid maze name */
//		AddrTemp = resolveHost(*hostName);
//		if (AddrTemp== (Sockaddr *) NULL) {
//			printf("Don't know host %s\n", *hostName);
//			free(*hostName);
//			*hostName = NULL;
//		}
//	}
//	if ((*hostName != NULL) &&
//	    (strlen(*hostName) != 0))
//		bcopy((char *) AddrTemp, (char *) hostAddr, sizeof(Sockaddr));
//}
//
///* ----------------------------------------------------------------------- */
//
//Sockaddr *
//resolveHost(register char *name)
//{
//	register struct hostent *fhost;
//	struct in_addr fadd;
//	static Sockaddr sa;
//
//	if ((fhost = gethostbyname(name)) != NULL) {
//		sa.sin_family = fhost->h_addrtype;
//		sa.sin_port = 0;
//		bcopy(fhost->h_addr, &sa.sin_addr, fhost->h_length);
//	} else {
//		fadd.s_addr = inet_addr(name);
//		if (fadd.s_addr != -1) {
//			sa.sin_family = AF_INET;	/* grot */
//			sa.sin_port = 0;
//			sa.sin_addr.s_addr = fadd.s_addr;
//		} else
//			return(NULL);
//	}
//	return(&sa);
//}

/* ----------------------------------------------------------------------- */

void netInit(in_port_t port, int *multisock, Sockaddr *groupAddr) {
	dbg_printf("=== entering netInit === \n");
	Sockaddr		nullAddr;
	Sockaddr		*thisHost;
//	char			buf[128];
	int				reuse;
	u_char          ttl;
	struct ip_mreq  mreq;
	int 			sock;

//	gethostname(buf, sizeof(buf));
//	if ((thisHost = resolveHost(buf)) == (Sockaddr *) NULL)
//		RFError("who am I?");

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	  RFError("can't get socket");


	/* SO_REUSEADDR allows more than one binding to the same
	   socket - you cannot have more than one player on one
	   machine without this */
	reuse = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse,
		   sizeof(reuse)) < 0) {
		RFError("setsockopt failed (SO_REUSEADDR)");
	}

	nullAddr.sin_family = AF_INET;
	nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	nullAddr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&nullAddr,
		 sizeof(nullAddr)) < 0)
		RFError("netInit binding");

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
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
		   sizeof(ttl)) < 0) {
		RFError("setsockopt failed (IP_MULTICAST_TTL)");
	}

	/* join the multicast group */
	mreq.imr_multiaddr.s_addr = htonl(MULTICAST_GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
		   &mreq, sizeof(mreq)) < 0) {
		RFError("setsockopt failed (IP_ADD_MEMBERSHIP)");
	}

	/* Get the multi-cast address ready to use in SendData()
           calls. */
	nullAddr.sin_addr.s_addr = htonl(MULTICAST_GROUP);
	memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
	dbg_printf("Finish netInit: mysock = %d\n", sock);
	dbg_printf("Finish netInit: addr = %p\n", multisock);
	*multisock = sock;
}

/* ----------------------------------------------------------------------- */

void RFError(char *s)

{
	fprintf(stderr, "CS244BReplFs: %s\n", s);
	perror("CS244BReplFs");
	exit(-1);
}

/* ----------------------------------------------------------------------- */

