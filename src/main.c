#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>



#define SOCKET_DEFAULT "/tmp/quantisd"


static int set_fd_to_non_blocking(iFd)
{
	int iResult;
	int iFlags;


	/* Be pessimistic. */
	iResult = -1;

	/* Set the socket to non-blocking. */
	iFlags = fcntl(iFd, F_GETFL, 0);
	if( iFlags!=-1 )
	{
		iFlags |= O_NONBLOCK;
		iResult = fcntl(iFd, F_SETFL, iFlags);
		if( iResult!=0 )
		{
			iResult = -1;
		}
	}

	return iResult;
}


ssize_t socket_read(int fildes, void *buf, size_t nbyte)
{
	ssize_t retval;

	while(1)
	{
		retval = read(fildes, buf, nbyte);
		if (retval >= 0)
		{
			break;
		}
		else if (errno == EINTR)
		{
			continue;
		}
		else if (errno == EAGAIN)
		{
			continue;
		}
#ifdef EWOULDBLOCK
		else if (errno == EWOULDBLOCK)
		{
			continue;
		}
#endif
		else if (retval == -1)
		{
			fprintf(stderr, "read() in socket_read() failed: %d", errno);
			break;
		}
		else
		{
			break;
		}
	}

	return retval;
}




typedef enum SERVER_STATE_ENUM
{
	SERVER_STATE_NotConnected   = 0,    /* No open connection. */
	SERVER_STATE_Idle           = 1,    /* Waiting for new commands. */
} SERVER_STATE_T;


typedef struct SERVER_HANDLE_STRUCT
{
	SERVER_STATE_T tState;
	int iFd;
	unsigned char aucCmd[8];
} SERVER_HANDLE_T;


#define MAX_CONNECTIONS 16

static void server_loop(int iSocketFd, struct sockaddr_un *psSocketAddr)
{
	fd_set sReadFdSet, sWriteFdSet, sExceptFdSet;
	struct timeval sTimeout;
	int iFdMax;
	int iResult;
	int iFd;
	unsigned int uiCnt;
	unsigned int uiFreeServerHandles;
	SERVER_HANDLE_T atServerHandles[MAX_CONNECTIONS];
	ssize_t ssizResult;
	socklen_t sSockAddrLen;


	/* All server handles are free now. */
	uiFreeServerHandles = MAX_CONNECTIONS;

	/* Initialize all server handles. */
	for(uiCnt=0; uiCnt<MAX_CONNECTIONS; ++uiCnt)
	{
		atServerHandles[uiCnt].tState = SERVER_STATE_NotConnected;
		atServerHandles[uiCnt].iFd = -1;
	}

	while(1)
	{
		FD_ZERO(&sReadFdSet);
		FD_ZERO(&sWriteFdSet);
		FD_ZERO(&sExceptFdSet);

		/* Add the socket file descriptor. */
		FD_SET(iSocketFd, &sReadFdSet);
		iFdMax = iSocketFd;

		/* Add all open connections. */
		for(uiCnt=0; uiCnt<MAX_CONNECTIONS; ++uiCnt)
		{
			switch(atServerHandles[uiCnt].tState)
			{
			case SERVER_STATE_NotConnected:
				/* Do nothing here. */
				break;

			case SERVER_STATE_Idle:
				/* This instance is waiting for new commands. Notify if we can read something here. */
				iFd = atServerHandles[uiCnt].iFd;
				FD_SET(iFd, &sReadFdSet);
				FD_SET(iFd, &sExceptFdSet);
				if( iFd>iFdMax )
				{
					iFdMax = iFd;
				}
				break;
			}
		}

		/* Wait for a maximum of 2 seconds. */
		sTimeout.tv_sec = 2;
		sTimeout.tv_usec = 0;

		iResult = select(iFdMax + 1, &sReadFdSet, &sWriteFdSet, &sExceptFdSet, &sTimeout);
		if( iResult<0 )
		{
			fprintf(stderr, "ERROR: select failed: (%d) %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		else if( iResult>0 )
		{
			/* Process all open server handles. */
			for(uiCnt=0; uiCnt<MAX_CONNECTIONS; ++uiCnt)
			{
				if( atServerHandles[uiCnt].tState!=SERVER_STATE_NotConnected )
				{
					iFd = atServerHandles[uiCnt].iFd;
					if( FD_ISSET(iFd, &sExceptFdSet) )
					{
						fprintf(stderr, "Except on server #%d\n", uiCnt);
					}
					if( FD_ISSET(iFd, &sReadFdSet) )
					{
						fprintf(stderr, "Read on server #%d\n", uiCnt);
					}
					if( FD_ISSET(iFd, &sWriteFdSet) )
					{
						fprintf(stderr, "Write on server #%d\n", uiCnt);
					}

					switch( atServerHandles[uiCnt].tState )
					{
					case SERVER_STATE_NotConnected:
						/* This should not happen. */
						break;

					case SERVER_STATE_Idle:
						/* Waiting for a new command. */
						if( FD_ISSET(iFd, &sReadFdSet) )
						{
							/* Read the command byte. */
							ssizResult = socket_read(iFd, atServerHandles[uiCnt].aucCmd, 1);
							if( ssizResult==1 )
							{
								fprintf(stderr, "s%02d cmd: %02x\n", uiCnt, atServerHandles[uiCnt].aucCmd[0]);
							}
						}
						break;
					}
				}
			}


			/* Is there a new connection? */
			if( FD_ISSET(iSocketFd, &sReadFdSet) )
			{
				/* Is at least one server slot free? */
				if( uiFreeServerHandles>0 )
				{
					/* Find the first free slot. */
					uiCnt = 0;
					while( uiCnt<MAX_CONNECTIONS )
					{
						if(atServerHandles[uiCnt].tState==SERVER_STATE_NotConnected)
						{
							break;
						}
						++uiCnt;
					}

					if( uiCnt<MAX_CONNECTIONS )
					{
						sSockAddrLen = sizeof(struct sockaddr_un);
						iFd = accept(iSocketFd, (struct sockaddr*) psSocketAddr, &sSockAddrLen);
						if( iFd>=0 )
						{
							iResult = set_fd_to_non_blocking(iFd);
							if( iResult!=0 )
							{
								fprintf(stderr, "ERROR: failed to set the sockets flags: (%d) %s\n", errno, strerror(errno));
								exit(EXIT_FAILURE);
							}
							else
							{
								atServerHandles[uiCnt].tState = SERVER_STATE_Idle;
								atServerHandles[uiCnt].iFd    = iFd;
								--uiFreeServerHandles;
							}
						}
					}
				}
			}
		}
		else
		{
			/* Show some stats if idle. */
			for(uiCnt=0; uiCnt<MAX_CONNECTIONS; ++uiCnt)
			{
				printf("#%02d:\n", uiCnt);
				switch( atServerHandles[uiCnt].tState )
				{
				case SERVER_STATE_NotConnected:
					printf("\tnot connected\n");
					break;

				case SERVER_STATE_Idle:
					printf("\tidle\n");
					break;
				}
			}
		}
	}
}


int main(void)
{
	const char *pcSocketPath;
	int iResult;
	int iSocketFd;
	struct stat sSocketStatBuffer;
	struct sockaddr_un sSocketAddr;


	printf("quantisd V1.0 by cthelen@hilscher.com\n");


	pcSocketPath = SOCKET_DEFAULT;

	/*
	 * Does the socket already exist?
	 */
	iResult = stat(pcSocketPath, &sSocketStatBuffer);
	if( iResult!=0 )
	{
		/* Failed to stat the socket. */
		if( errno==ENOENT )
		{
			/* OK, the socket does not exist yet. */
			iResult = 0;
		}
		else
		{
			/* Other error accessing the socket. */
			fprintf(stderr, "ERROR: failed to stat the socket: (%d) %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		/* The socket exists. */
		fprintf(stderr, "ERROR: the socket already exists: '%s'\n", pcSocketPath);
		exit(EXIT_FAILURE);
	}



	/*
	 * Create a new socket.
	 */
	iSocketFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if( iSocketFd==-1 )
	{
		fprintf(stderr, "ERROR: failed to create a socket: (%d) %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*
	 * Bind the socket to the actual socket-file
	 */
	memset(&sSocketAddr, 0, sizeof(sSocketAddr));
	sSocketAddr.sun_family = AF_UNIX;
	strcpy(sSocketAddr.sun_path, pcSocketPath);
	iResult = bind(iSocketFd, (struct sockaddr*)(&sSocketAddr), sizeof(sSocketAddr));
	if( iResult!=0 )
	{
		fprintf(stderr, "ERROR: failed to bind the socket: (%d) %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	iResult = chmod(pcSocketPath, 0777);
	if( iResult!=0 )
	{
		fprintf(stderr, "ERROR: failed to chmod the socket: (%d) %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	iResult = listen(iSocketFd, SOMAXCONN);
	if( iResult!=0 )
	{
		fprintf(stderr, "ERROR: failed to listen to the socket: (%d) %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Set the socket to non-blocking. */
	iResult = set_fd_to_non_blocking(iSocketFd);
	if( iResult!=0 )
	{
		fprintf(stderr, "ERROR: failed to set the sockets flags: (%d) %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}


	/* Enter the server loop. */
	server_loop(iSocketFd, &sSocketAddr);


	return 0;
}
