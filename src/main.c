#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "Quantis.h"


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
			fprintf(stderr, "read() in socket_read() failed: %d %s\n", errno, strerror(errno));
			break;
		}
		else
		{
			break;
		}
	}

	return retval;
}



ssize_t socket_write(int fildes, void *buf, size_t nbyte)
{
  ssize_t retval;

  for (;;) {
    retval = write(fildes, buf, nbyte);
    if (retval >= 0)
      break;
    else if (errno == EINTR)
      continue;
    else if (errno == EAGAIN)
      continue;
#ifdef EWOULDBLOCK
    else if (errno == EWOULDBLOCK)
      continue;
#endif
    else if (retval == -1) {
      fprintf(stderr, "write() in socket_write() failed: %d %s\n", errno, strerror(errno));
      break;
    }
    else
      break;
  }
  
  return retval;
}



typedef enum SERVER_STATE_ENUM
{
	SERVER_STATE_NotConnected              = 0,    /* No open connection. */
	SERVER_STATE_Idle                      = 1,    /* Waiting for new commands. */
	SERVER_STATE_SendData                  = 2,    /* Send the buffer. */
	SERVER_STATE_GetReadParameter          = 3,
	SERVER_STATE_GetReadBlockingParameter  = 4,
	SERVER_STATE_GetWriteParameter         = 5
} SERVER_STATE_T;


typedef struct SERVER_HANDLE_STRUCT
{
	SERVER_STATE_T tState;
	int iFd;
	unsigned int uiMax;
	unsigned int uiCnt;
	unsigned char aucCmd[8];
	unsigned char aucData[257];
} SERVER_HANDLE_T;



void server_close_instance(SERVER_HANDLE_T *ptHandle)
{
	/* Close the server instance. */
	close(ptHandle->iFd);
	ptHandle->tState = SERVER_STATE_NotConnected;
	ptHandle->iFd    = -1;
}



void server_state_idle(SERVER_HANDLE_T *ptHandle)
{
	int iFd;
	ssize_t ssizResult;
	unsigned char *pucData;
	size_t sizData;
	unsigned char ucCommand;
	unsigned long ulValue;
	unsigned int uiCnt;


	/* Get the file descriptor. */
	iFd = ptHandle->iFd;

	/* Read the command byte. */
	ssizResult = socket_read(iFd, ptHandle->aucCmd, 1);
	fprintf(stderr, "read: %d\n", ssizResult);
	if( ssizResult==1 )
	{
		fprintf(stderr, "s%02d cmd: %02x\n", uiCnt, ptHandle->aucCmd[0]);

		/* Decode the command. */
		ucCommand = ptHandle->aucCmd[0];
		if( ucCommand==0x00 )
		{
			/* Return the number of bits of entropy in the pool. */
			/* FIXME: what should I return here? */
			ulValue = 4096*8;

			ptHandle->uiMax = 4;
			ptHandle->uiCnt = 0;
			ptHandle->aucData[0] =  ulValue         & 0x000000ffU;
			ptHandle->aucData[1] = (ulValue >>  8U) & 0x000000ffU;
			ptHandle->aucData[2] = (ulValue >> 16U) & 0x000000ffU;
			ptHandle->aucData[3] = (ulValue >> 24U) & 0x000000ffU;
			ptHandle->tState = SERVER_STATE_SendData;
		}
		else if( ucCommand==0x01 )
		{
			/* Read some random bytes.
			 * Expect the size information.
			 */
			ptHandle->uiMax = 1;
			ptHandle->uiCnt = 0;
			ptHandle->tState = SERVER_STATE_GetReadParameter;
		}
		else if( ucCommand==0x02 )
		{
			/* Read some random bytes.
			 * Expect the size information.
			 */
			ptHandle->uiMax = 1;
			ptHandle->uiCnt = 0;
			ptHandle->tState = SERVER_STATE_GetReadBlockingParameter;
		}
		else if( ucCommand==0x03 )
		{
			/* Receive entropy bytes.
			 * Expect the size information.
			 */
			ptHandle->uiMax = 5;
			ptHandle->uiCnt = 0;
			ptHandle->tState = SERVER_STATE_GetWriteParameter;
		}
		else if( ucCommand==0x04 )
		{
			/* Get PID. */
			/* FIXME: insert the real PID here. */
			ptHandle->uiMax = 5;
			ptHandle->uiCnt = 0;
			ptHandle->aucData[0] = 4;
			ptHandle->aucData[1] = '1';
			ptHandle->aucData[2] = '2';
			ptHandle->aucData[3] = '3';
			ptHandle->aucData[4] = 0;
			ptHandle->tState = SERVER_STATE_SendData;
		}
	}
	else
	{
		if( errno!=0 )
		{
			fprintf(stderr, "read error: %d %s\n", errno, strerror(errno));
			server_close_instance(ptHandle);
		}
	}
}



void server_state_send_data(SERVER_HANDLE_T *ptHandle)
{
	int iFd;
	ssize_t ssizResult;
	unsigned char *pucData;
	size_t sizData;


	/* Get the file descriptor. */
	iFd = ptHandle->iFd;

	/* Are some bytes left to send? */
	if( ptHandle->uiCnt>=ptHandle->uiMax )
	{
		/* No -> sending finished. */
		ptHandle->tState = SERVER_STATE_Idle;
	}
	else if( ptHandle->uiCnt>=sizeof(ptHandle->aucData) )
	{
		fprintf(stderr, "The buffer index exceeds the buffer size!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		/* Get the number of bytes left to send. */
		sizData = ptHandle->uiMax - ptHandle->uiCnt;
		/* Get the pointer to the data. */
		pucData = ptHandle->aucData + ptHandle->uiCnt;
		
		/* Send the data. */
		ssizResult = socket_write(iFd, pucData, sizData);
		if( ssizResult>0 )
		{
			ptHandle->uiCnt += ssizResult;
			if( ptHandle->uiCnt>=ptHandle->uiMax )
			{
				/* No -> sending finished. */
				ptHandle->tState = SERVER_STATE_Idle;
			}
		}
		else
		{
			fprintf(stderr, "write: %d\n", ssizResult);
		}
	}
}



void server_state_receive_cmd(SERVER_HANDLE_T *ptHandle)
{
	int iFd;
	ssize_t ssizResult;
	unsigned char *pucData;
	size_t sizData;


	/* Get the file descriptor. */
	iFd = ptHandle->iFd;

	/* Are some bytes left to receive? */
	if( ptHandle->uiCnt>=ptHandle->uiMax )
	{
		/* No -> receive finished. */
		ptHandle->tState = SERVER_STATE_Idle;
	}
	else if( ptHandle->uiCnt>=sizeof(ptHandle->aucCmd) )
	{
		fprintf(stderr, "The buffer index exceeds the buffer size!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		/* Get the number of bytes left to send. */
		sizData = ptHandle->uiMax - ptHandle->uiCnt;
		/* Get the pointer to the data. */
		pucData = ptHandle->aucCmd + ptHandle->uiCnt;
		
		/* Send the data. */
		ssizResult = socket_read(iFd, pucData, sizData);
		if( ssizResult>0 )
		{
			ptHandle->uiCnt += ssizResult;
			if( ptHandle->uiCnt>=ptHandle->uiMax )
			{
				/* No -> sending finished. */
				ptHandle->tState = SERVER_STATE_Idle;
			}
		}
		else
		{
			fprintf(stderr, "read: %d\n", ssizResult);
		}
	}
}



void read_random_bytes(unsigned char *pucData, size_t sizData)
{
	int iResult;
	char acDataStamp[256];
	time_t t;
	struct tm *tmp;


	t = time(NULL);
	tmp = localtime(&t);
	memset(acDataStamp, 0, sizeof(acDataStamp));
	strftime(acDataStamp, sizeof(acDataStamp)-1, "%F %T", tmp);
	printf("%s: Requested %d random bytes.\n", acDataStamp, sizData);
	while( sizData!=0 )
	{
		iResult = QuantisRead(QUANTIS_DEVICE_USB, 0, pucData, sizData);
		if( iResult<0 )
		{
			fprintf(stderr, "ERROR: Failed to read %d bytes from the Quantis device: %d\n", sizData, iResult);
			exit(EXIT_FAILURE);
		}
		else if( iResult>sizData )
		{
			fprintf(stderr, "ERROR: requested only %d bytes , but read %d from the Quantis device!\n", sizData, iResult);
			exit(EXIT_FAILURE);
		}
		else
		{
			sizData -= iResult;
		}
	}
}



void send_random_packet(SERVER_HANDLE_T *ptHandle, unsigned char ucPacketSize)
{
	ptHandle->uiMax = ucPacketSize + 1;
	ptHandle->uiCnt = 0;
	ptHandle->aucData[0] = ucPacketSize;
	read_random_bytes(ptHandle->aucData+1, ucPacketSize);
	ptHandle->tState = SERVER_STATE_SendData;
}



void send_random_packet_block(SERVER_HANDLE_T *ptHandle, unsigned char ucPacketSize)
{
	unsigned int uiCnt;


	ptHandle->uiMax = ucPacketSize;
	ptHandle->uiCnt = 0;
	read_random_bytes(ptHandle->aucData, ucPacketSize);
	ptHandle->tState = SERVER_STATE_SendData;
}



#define MAX_CONNECTIONS 16

static void server_loop(int iSocketFd, struct sockaddr_un *psSocketAddr)
{
	fd_set sReadFdSet, sWriteFdSet, sExceptFdSet;
	struct timeval sTimeout;
	int iFdMax;
	int iResult;
	int iFd;
	unsigned int uiCnt;
	SERVER_HANDLE_T atServerHandles[MAX_CONNECTIONS];
	ssize_t ssizResult;
	socklen_t sSockAddrLen;
	unsigned char ucDataSize;


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
			case SERVER_STATE_GetReadParameter:
			case SERVER_STATE_GetReadBlockingParameter:
			case SERVER_STATE_GetWriteParameter:
				/* This instance is waiting for new data. Notify if we can read something here. */
				iFd = atServerHandles[uiCnt].iFd;
				FD_SET(iFd, &sReadFdSet);
				FD_SET(iFd, &sExceptFdSet);
				if( iFd>iFdMax )
				{
					iFdMax = iFd;
				}
				break;

			case SERVER_STATE_SendData:
				/* This instance is waiting to write data. */
				iFd = atServerHandles[uiCnt].iFd;
				FD_SET(iFd, &sWriteFdSet);
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
						exit(EXIT_FAILURE);
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
							server_state_idle(atServerHandles+uiCnt);
						}
						break;

					case SERVER_STATE_SendData:
						if( FD_ISSET(iFd, &sWriteFdSet) )
						{
							server_state_send_data(atServerHandles+uiCnt);
						}

					case SERVER_STATE_GetReadParameter:
						if( FD_ISSET(iFd, &sReadFdSet) )
						{
							server_state_receive_cmd(atServerHandles+uiCnt);
							if( atServerHandles[uiCnt].tState==SERVER_STATE_Idle )
							{
								/* Send some random bytes. */
								ucDataSize = atServerHandles[uiCnt].aucCmd[0];
								if( ucDataSize>0 )
								{
									send_random_packet(atServerHandles+uiCnt, ucDataSize);
								}
							}
						}
						break;
				
					case SERVER_STATE_GetReadBlockingParameter:
						if( FD_ISSET(iFd, &sReadFdSet) )
						{
							server_state_receive_cmd(atServerHandles+uiCnt);
							if( atServerHandles[uiCnt].tState==SERVER_STATE_Idle )
							{
								/* Send some random bytes. */
								ucDataSize = atServerHandles[uiCnt].aucCmd[0];
								if( ucDataSize>0 )
								{
									send_random_packet_block(atServerHandles+uiCnt, ucDataSize);
								}
							}
						}
						break;
				
					case SERVER_STATE_GetWriteParameter:
						if( FD_ISSET(iFd, &sReadFdSet) )
						{
							server_state_receive_cmd(atServerHandles+uiCnt);
							if( atServerHandles[uiCnt].tState==SERVER_STATE_Idle )
							{
								/* Ignore the input data. */
								ucDataSize = atServerHandles[uiCnt].aucCmd[4];
fprintf(stderr, "write: ignoring %d bytes of data\n", ucDataSize);
								/* FIXME: ignore the data. */
							}
						}
						break;
					}
				}
			}


			/* Is there a new connection? */
			if( FD_ISSET(iSocketFd, &sReadFdSet) )
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
						}
					}
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
	int iQuantisDevices;


	printf("quantisd V1.0 by cthelen@hilscher.com\n");
	printf("Using Quantis library V%f\n", QuantisGetLibVersion());


	/* Look for a quantis device. For now take the first USB device. */
	iQuantisDevices = QuantisCount(QUANTIS_DEVICE_USB);
	if( iQuantisDevices==0 )
	{
		fprintf(stderr, "ERROR: No Quantis USB devices found!\n");
		exit(EXIT_FAILURE);
	}
	printf("Found %d Quantis USB devices. Using first device.\n", iQuantisDevices);


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
