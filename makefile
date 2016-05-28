
CFLAGS	= -g -Wall -DSUN
CFLAGS += -Wno-unused-variable
# CFLAGS	= -g -Wall -DDEC
CC	= gcc
CCF	= $(CC) $(CFLAGS)

H	= .
C_DIR	= .

INCDIR	= -I$(H)
LIBDIRS = -L$(C_DIR)
LIBS    = -lclientReplFs

CLIENT_OBJECTS = client.o network.o
SERVER_OBJECTS = server.o network.o

all:	appl replFsServer

appl:	appl.o $(C_DIR)/libclientReplFs.a
	$(CCF) -o appl appl.o $(LIBDIRS) $(LIBS)
	
replFsServer:	$(SERVER_OBJECTS)
	$(CCF) -o replFsServer $(SERVER_OBJECTS)

appl.o:	appl.c client.h appl.h
	$(CCF) -c $(INCDIR) appl.c

$(C_DIR)/libclientReplFs.a:	$(CLIENT_OBJECTS)
	ar cr libclientReplFs.a $(CLIENT_OBJECTS)
	ranlib libclientReplFs.a

client.o:	client.c client.h network.h
	$(CCF) -c $(INCDIR) client.c
	
network.o:	network.c network.h
	$(CCF) -c $(INCDIR) network.c
	
server.o:	server.c network.h
	$(CCF) -c $(INCDIR) server.c

clean:
	rm -f appl replFsServer *.o *.a

