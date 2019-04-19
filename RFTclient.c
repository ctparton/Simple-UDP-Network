#include "RFTheader.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>



/* error message format for fatalerror - Source Nick Cook*/
static const char *ERR_FMT = "%s:%d - %s, errno %d: %s\n";

/**************************************************************************
**************************************************************************
 All functions' signature
**************************************************************************
**************************************************************************/
int checksum(char *content, int len);
FILE * check_file(char *fileName);
int file_size(FILE *file);
void fatalerror(int line, char *msg);
bool isCorrupted(float prob);

struct sockaddr_in create_server_data(int sockfd , char* host, int port);
void send_meta_data(FILE *file,char *output, int sockfd, struct sockaddr_in server);
void send_file_normal(FILE *fp,int sockfd, struct sockaddr_in server);
void send_file_with_timeout(FILE *fp,int sockfd, struct sockaddr_in server, float prob_loss);

/**************************************************************************
**************************************************************************
 Functions that you need to implement
**************************************************************************
**************************************************************************/
/* arguments: socket id, host name, port number
   return: The internet socket address of the server */
struct sockaddr_in create_server_data(int sockfd , char* host, int port)
{
    struct sockaddr_in server;

    server.sin_family=AF_INET; // AF_INET = IPv4 Protocol
    server.sin_port = htons(port); // htons() - Converts to network byte order
    server.sin_addr.s_addr = inet_addr(host); // Convets IPv4 host into binary data in network byte order
	  return server;
}

/* arguments: input file, the name of the output file, socket id,
              port number, structure containing the server internet address */
void send_meta_data(FILE *file,char *output, int sockfd, struct sockaddr_in server)
{
    meta_data meta;
    strncpy(meta.name, output, 100);
    meta.size = file_size(file);

    ssize_t send = sendto(sockfd, &meta, sizeof(meta), 0, (struct sockaddr *) &server, sizeof(server));

    if (send < 0 ) {
      perror("SENDER: Failed to send meta data");
      close (sockfd );
      exit(1);
    }
    printf ("SENDER: Metadata sent\n");

}

/* arguments: input file, socket id, internet socket address of the receiver  */
void send_file_normal(FILE *fp,int sockfd, struct sockaddr_in server)
{
    char buffer[TOTALCHAR + 1];
    int i, ch;
    ssize_t val;
    segment data_msg, ack_msg;
    int msgSeq = 0;
    ssize_t len = sizeof(server);
    int count = 0;
    int fileSize = file_size(fp);
    int segmentCount = 0;

    printf("------------------------------------------------\n");
    printf("------------------------------------------------\n");
    printf("\nSENDER: Sending file with size of %d bytes...\n", fileSize);
    printf("------------------------------------------------\n");
    fseek( fp, 0, SEEK_SET ); // resets file position to start of file
    while (count < fileSize) {

        // Reading in 15 characters from the text file
        for (i = 0; (i < (sizeof(buffer)-1) && ((ch = fgetc(fp)) != EOF) && (ch != '\n')); i++) {
            buffer[i] = ch;
        }

        // Ensuring null termination
        buffer[i] = '\0';

        // Setting values of data segment struct
        data_msg.size = strlen(buffer);
        data_msg.sq = msgSeq;
        data_msg.type = TYPE_DATA;
        strcpy(data_msg.payload, buffer);
        data_msg.checksum = checksum(buffer, strlen(buffer));

        // Sending data segment
        if ((val=sendto(sockfd, &data_msg, sizeof(data_msg),0, (struct sockaddr *)&server, len))<0) {
            perror("SENDER: Reading Stream Message Error\n");
            close (sockfd );
            exit(1);
        } else if(val==0) {
            printf("SENDER: Ending connection\n");
            close (sockfd );
            exit(1);
        } else {
            printf("SENDER: Sending Segment: sq:(%d), size(%d), checksum(%d), content(%s)\n", data_msg.sq, data_msg.size,
                          data_msg.checksum, data_msg.payload);
        }

        printf("SENDER: Waiting for Ack from SERVER\n");

        // Recieving aknowledgement from server
        if ((val=recvfrom(sockfd,&ack_msg, sizeof(ack_msg), 0,(struct sockaddr *)&server, &len))<0) {
            perror("SENDER: Reading Stream Message Error\n");
            close (sockfd );
            exit(1);
        } else if (val==0) {
            printf("SENDER: Ending connection\n");
            close (sockfd );
            exit(1);
        } else {
            if (ack_msg.sq == data_msg.sq) {
                printf("SENDER: ACK RECIEVED: sq:(%d)\n", ack_msg.sq);
            }

        }

        // alternate the sequence number after each segment sent and received
        if (msgSeq == 0) {
            msgSeq = 1;
        } else {
            msgSeq = 0;
        }

        count += data_msg.size; // Keep track of how much data has been sent
        segmentCount++;
        printf("\t\t>>>>>>> NETWORK: sent %d/%d bytes successfully <<<<<<<<<\n", count, fileSize);
        printf("------------------------------------------------\n");
        printf("------------------------------------------------\n");
    }

    printf("Total number of segments (%d) for file of size (%d)\n", segmentCount, fileSize );
}


 /* arguments: input file, socket id, internet socket address of the receiver  */
void send_file_with_timeout(FILE *fp,int sockfd, struct sockaddr_in server, float prob_loss)
{

  char buffer[TOTALCHAR + 1];
  int i, ch, timeLeft;
  ssize_t val;
  segment data_msg, ack_msg;
  int msgSeq = 0;
  ssize_t len = sizeof(server);
  int count = 0;
  int fileSize = file_size(fp);
  int segmentCount = 0;

  // NEW declarations and assignments for this task
  int failedSegmentCount = 0;
  bool corrupted = false;

  // Setting timeout value and declaring fd_set buffer and appropriate boolean
  fd_set readfds;
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  bool resend = true;

  printf("------------------------------------------------\n");
  printf("------------------------------------------------\n");
  printf("\nSENDER: Sending file with size of %d bytes...\n", fileSize);
  printf("------------------------------------------------\n");
  fseek( fp, 0, SEEK_SET ); // resets file position to start of file
  while (count < fileSize) {
      resend = true;
      corrupted = true;
      // Getting 15 characters from the text file
      for (i = 0; (i < (sizeof(buffer)-1) && ((ch = fgetc(fp)) != EOF) && (ch != '\n')); i++) {
          buffer[i] = ch;
      }
      buffer[i] = '\0';


      while (resend) {

        corrupted = isCorrupted(prob_loss); // Randomly decides if segment is corrupted or not

        // Sets appropriate checksum if segment is corrupted
        if (corrupted) {
          data_msg.checksum = 0;
          corrupted = false;
        } else {
          data_msg.checksum = checksum(buffer, strlen(buffer));
        }

        // Setting values of data segment struct
        data_msg.size = strlen(buffer);
        data_msg.sq = msgSeq;
        data_msg.type = TYPE_DATA;
        strcpy(data_msg.payload, buffer);


        // Sending data segment
        if ((val=sendto(sockfd, &data_msg, sizeof(data_msg),0, (struct sockaddr *)&server, len))<0) {
            perror("SENDER: Reading Stream Message Error\n");
            close (sockfd );
            exit(1);
        } else if(val==0) {
            printf("SENDER: Ending connection\n");
            close (sockfd );
            exit(1);
        } else {
            printf("SENDER: Sending Segment: sq:(%d), size(%d), checksum(%d), content(%s)\n", data_msg.sq, data_msg.size,
                          data_msg.checksum, data_msg.payload);
        }

        if (data_msg.checksum == 0) {
          printf("\t\t>>>>>>> NETWORK: segment checksum is corrupted <<<<<<<<<\n\n");
        }
        printf("SENDER: Waiting for Ack from SERVER\n");

        // Clearing and resetting the bit array to monitor socket
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Calling select() to trigger timeout after packet has been sent
        timeLeft = select(8, &readfds, NULL, NULL, &timeout);

        if (timeLeft == 0) {
            printf("SENDER: time out, resending segment...\n\n");
            printf("****************************************************\n");
            failedSegmentCount++;
            resend = true;
        } else {
          resend = false;
        }
      }

      // Recieving aknowledgement from server
      if ((val=recvfrom(sockfd,&ack_msg, sizeof(ack_msg), 0,(struct sockaddr *)&server, &len))<0) {
          perror("SENDER: Reading Stream Message Error\n");
          close (sockfd );
          exit(1);
      } else if (val==0) {
          printf("SENDER: Ending connection\n");
          close (sockfd );
          exit(1);
      } else {
          printf("SENDER: ACK RECIEVED: sq:(%d)\n", ack_msg.sq);
      }

      // alternating the sequence number
      if (msgSeq == 0) {
          msgSeq = 1;
      } else {
          msgSeq = 0;
      }

      count += data_msg.size;
      segmentCount++;
      printf("\t\t>>>>>>> NETWORK: sent %d/%d bytes successfully <<<<<<<<<\n", count, fileSize);
      printf("------------------------------------------------\n");
      printf("------------------------------------------------\n");
  }
  printf("TOTAL FAILED SEGMENTS %d\n", failedSegmentCount);
  printf("TOTAL SUCCESSFUL SEGMENTS %d\n", segmentCount);
  printf("Total number of segments (%d) for file of size (%d)\n", segmentCount + failedSegmentCount, fileSize );
}

/**************************************************************************
**************************************************************************
                  The main function
**************************************************************************
**************************************************************************/
int main(int argc,char *argv[])
{
	int sockfd ;
	FILE *file;
    struct sockaddr_in server;

	/* 	accept input from console in the form of
	./client inputFile outputFile localhost portNumber 	*/
	if (argc != 5) {
		fprintf(stderr, "Usage: <inputFile> <outputFile> <localhost> <portNumber> \n");
		exit(1);
	}

	/* check the file actually exist	*/
	file=check_file(argv[1]);
	printf ("----------------------------------------------------\n");
	printf ("SENDER: File (%s) exists with a size of (%d) bytes\n",argv[1], file_size(file));
	printf ("----------------------------------------------------\n");
	printf ("----------------------------------------------------\n");

	/* create a socket */
	sockfd =socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd <0)
	{
		perror("SENDER: Failed to open socket");
		close (sockfd );
		exit(1);
	}
	printf ("SENDER: Socket is created\n");

	/* create a connection */
	server = create_server_data(sockfd, argv[3], atoi(argv[4]));
 	printf ("SENDER: Server data is prepared\n");

	/* Send meta data */
	send_meta_data(file,argv[2],sockfd,server);

	/* transmission options */
	int choice=0;
  float loss=0;
	printf ("----------------------------------------------------\n");
	printf ("----------------------------------------------------\n");
	printf("Choose one of the following options (1 or 2):\n ");
	printf("1. Normal transmission (no data segment is lost) \n ");
	printf("2: Transmission with time-out capabilities\n ");
    scanf("%d", &choice);
    switch(choice)
    {
        case 1:
            send_file_normal(file,sockfd, server);
            break;

        case 2:
			printf("Enter the probability of a corrupted checksum (between 0 and 1):\n ");
			scanf("%f", &loss);
			send_file_with_timeout(file,sockfd, server, loss);
            break;

         default:
            printf("Error! enter 1 or 2 \n");
    }

	printf("SENDER: File is sent\n");

	/* Close the file */
	fclose(file);

	/* Close the socket */
	close (sockfd );

 	return 0;
}

/*************************************************************************
**************************************************************************
 These functions are implemented for you .. Do NOT Change them
**************************************************************************
**************************************************************************/


/* calculate the segment checksum by adding the payload */
int checksum(char *content, int len)
{
	int i;
	int sum = 0;
	for (i = 0; i < len; i++)
		sum += (int)(*content++);
	return sum;
}

/* check if the input file does exist */
FILE * check_file(char *fileName)
{
	FILE *file = fopen(fileName, "rb");
	if(!file) {
		perror("SENDER: File does not exists");
 		fclose(file);
		exit(1);
	}
	return file;
}

/* return file size */
int file_size(FILE *file)
{
	fseek(file, 0L, SEEK_END);
	int size = ftell(file);
	return size;
}

void fatalerror(int line, char *msg) {
    printf("Line %d : %s\n", line, msg);
    exit(0);
}

/* decide whether the segment is corrupted or not given a certain probability */
bool isCorrupted(float prob)
{
    //srand(time(NULL));
    if(((float)(rand())/(float)(RAND_MAX)) < prob)
	{
	return true;
	}
    else return false;
}
/***************************************************************************
 ***************************************************************************
 ***************************************************************************
 ***************************************************************************/
