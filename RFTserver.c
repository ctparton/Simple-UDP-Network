
#include "RFTheader.h"
#include <stdbool.h>
#include <time.h>

/**************************************************************************
**************************************************************************
 All functions' signature
**************************************************************************
**************************************************************************/
int checksum(char *content, int len);
void receive_file_normal(char * name, int size, int sockfd, struct sockaddr_in client);
void fatalerror(int line, char *msg);
bool isLost(float prob);

void receive_file_with_ackLost(char * name, int size, int sockfd, struct sockaddr_in client, float prob_loss );

/**************************************************************************
**************************************************************************
 Functions that you need to implement for Part 3 only
**************************************************************************
**************************************************************************/

void receive_file_with_ackLost(char * name, int size, int sockfd, struct sockaddr_in client, float prob_loss )
{
  int len = sizeof(client);
  segment data_msg, ack_msg;

  /*Open the file*/
  FILE *file;
  file=fopen(name, "wb");
  int count=0;
  int expectedSeq=0;
  int val=0;

  // NEW declarations and assignments
  bool ackLost;

  printf("------------------------------------------------\n");
  printf("------------------------------------------------\n");
  while(count < size) {

    ackLost = isLost(prob_loss);
    // Recieving data segment from client
    if ((val=recvfrom(sockfd,&data_msg, sizeof(data_msg), 0,(struct sockaddr *)&client, &len))<0)
      perror("SERVER: Reading Stream Message Error\n");
    else if (val==0)
      printf("SERVER: Ending connection\n");
    else
    {
      if (count==0)  {
        printf("SERVER: File copying start\n");
        printf("------------------------------------------------\n");
      }
      printf("SERVER: A Segment with sq (%d) is received:\n",data_msg.sq);
      printf("\tINFO: size %d, checksum %d, content (",data_msg.size,data_msg.checksum );
      int c = 0;
      for (c = 0;  c < TOTALCHAR; c++)
        if (data_msg.payload[c]=='\0') break;
        else printf("%c", data_msg.payload[c]);

      printf(")\n");

      int x = checksum(data_msg.payload, data_msg.size);

      /* If the calculated checksum is same as that of the recieved
      checksum then send corrosponding ack */
      if(x == data_msg.checksum) {
        printf("SERVER: Calculated checksum is (%d) --  VALID\n",x);
        ack_msg.sq = data_msg.sq;
        ack_msg.type= TYPE_ACK;

        if (data_msg.sq != expectedSeq) {
          printf("SERVER: This segment is a duplicate -- DETECTED\n");
          printf("SERVER: ACK of the previous message is SENT\n");
        } else {
            printf("SERVER: Sending an ACK with sq (%d)\n", ack_msg.sq);
        }

        if (!ackLost) {
          if ((val=sendto(sockfd, &ack_msg, sizeof(ack_msg), 0,(struct sockaddr *) &client, len))<0)
            perror("SERVER: Reading Stream Message Error\n");
          else if(val==0)
            printf("SERVER: Ending connection\n");
          else {
            if (data_msg.sq == expectedSeq) {
              fwrite(&data_msg.payload[0],data_msg.size,1,file);
              expectedSeq = (data_msg.sq+1)%2; // Next seq is inverse of current seq
              printf("\t\t>>>>>>> NETWORK: ACK is sent successfully <<<<<<<<<\n");
            }

            count += data_msg.size;
            printf("------------------------------------------------\n");
            printf("------------------------------------------------\n");
          }
        }
        else {
          // IF LOST, write data as normal, but don't send ACK
          if (data_msg.sq == expectedSeq) {
            expectedSeq = (data_msg.sq+1)%2;
            fwrite(&data_msg.payload[0],data_msg.size,1,file);
          }
          printf("\t\t>>>>>>> NETWORK: ACK is LOST <<<<<<<<<\n");
        }
      }
      else {
        printf("SERVER: Segment checksum is (%d) -- INVALID\n",x );
        printf("SERVER: Do NOT SEND ANY ACK\n");
        printf("************************************\n");
      }
    }
  }
printf("SERVER: File copying complete\n");
fclose(file);
}

/**************************************************************************
**************************************************************************
                  The main function
**************************************************************************
**************************************************************************/
int main(int argc,char *argv[])
{
	int sockfd ;
    struct sockaddr_in client, server;

	/* user needs to enter the port number */
	if(argc!=2)
	{
		fprintf(stderr, "Usage: <portNumber> \n");
		exit(1);
	}

	/* create a socket */
	sockfd =socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd <0)
	{
		perror("SERVER: Failed to open socket");
		close (sockfd );
		exit(1);
	}
	printf("------------------------------------------------------------------\n");
	printf ("SERVER: Socket is created\n");

    server.sin_family=AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(atoi(argv[1]));

	/* bind */
	if(bind(sockfd, (struct sockaddr *)&server,sizeof(server))){
		perror("SERVER: Bind Failed\n");
		close(sockfd);
		exit(1);
	}
	printf("SERVER: binding ... Ready to receive meta info from the client \n");
	printf("------------------------------------------------------------------\n");
	printf("------------------------------------------------------------------\n");

	meta_data  file_info;
	int len = sizeof(client);
	int val;

	if ((val=recvfrom(sockfd,&file_info,sizeof(file_info),0, (struct sockaddr *)&client, &len))<0){
		perror("SERVER: Reading Stream Message Error\n");
	} else if(val==0){
		printf("SERVER: Ending connection\n");
	} else {
		printf("SERVER: Meta info are received successfully: file name(%s), size(%d)\n",file_info.name, file_info.size);
	}

	/* transmission options */
	int choice=0;
    float loss=0;
	printf("------------------------------------------------------------------\n");
	printf("------------------------------------------------------------------\n");
	printf("SERVER: Waiting for the actual file ..\n");
	printf("------------------------------------------------------------------\n");
	printf("Choose one of the following transmission options (1 or 2):\n ");
	printf("1. Normal transmission (no Ack segment is lost) \n ");
	printf("2: Transmission with Lost Acknowledgements \n ");
    scanf("%d", &choice);

    switch(choice)
    {
        case 1:
            receive_file_normal(file_info.name, file_info.size, sockfd, client);
            break;

        case 2:
			printf("Enter the probability of a lost ack (between 0 and 1):\n ");
			scanf("%f", &loss);
			printf("SERVER: File copying start\n");
			receive_file_with_ackLost(file_info.name, file_info.size, sockfd, client, loss);
            break;

         default:
            printf("Error! enter 1 or 2 \n");
    }

	close(sockfd);
	return 0;
}

/*************************************************************************
**************************************************************************
 These functions are implemented for you .. Do NOT Change them
**************************************************************************
**************************************************************************/
int checksum(char *content, int len)
{
	int i;
	int sum = 0;
	for (i = 0; i < len; i++)
		sum += (int)(*content++);
	return sum;
}



void receive_file_normal(char * name, int size, int sockfd, struct sockaddr_in client )
{
    int len = sizeof(client);
    segment data_msg, ack_msg;

  	/*Open the file*/
	FILE *file;
	file=fopen(name, "wb");
	int count=0;
    int expectedSeq=0;
    int val=0;

 	printf("------------------------------------------------\n");
 	printf("------------------------------------------------\n");
	while(count < size) {

		if ((val=recvfrom(sockfd,&data_msg, sizeof(data_msg), 0,(struct sockaddr *)&client, &len))<0)
			perror("SERVER: Reading Stream Message Error\n");
		else if (val==0)
			printf("SERVER: Ending connection\n");
		else {
			if (count==0)
			{
			printf("SERVER: File copying start\n");
 	        printf("------------------------------------------------\n");
			}

			printf("SERVER: A Segment with sq (%d) is received:\n",data_msg.sq);
			printf("\tINFO: size %d, checksum %d, content (",data_msg.size,data_msg.checksum );
	     	int c = 0;
            for (c = 0;  c < TOTALCHAR; c++)
	        if (data_msg.payload[c]=='\0') break;
			   else printf("%c", data_msg.payload[c]);
			printf(")\n");

			int x = checksum(data_msg.payload, data_msg.size);

			/* If the calculated checksum is same as that of the recieved
			checksum then send corrosponding ack */
			if(x == data_msg.checksum){
				printf("SERVER: Calculated checksum is (%d) --  VALID\n",x);
				ack_msg.sq = data_msg.sq;
 				ack_msg.type= TYPE_ACK;
			    printf("SERVER: Sending an ACK with sq (%d)\n", ack_msg.sq);
				if ((val=sendto(sockfd, &ack_msg, sizeof(ack_msg), 0,(struct sockaddr *) &client, len))<0)
					    perror("SERVER: Reading Stream Message Error\n");
				else if(val==0)
				    	printf("SERVER: Ending connection\n");
				else
					{
					     fwrite(&data_msg.payload[0],data_msg.size,1,file);
						 expectedSeq = (data_msg.sq+1)%2;
					}

			    printf("\t\t>>>>>>> NETWORK: ACK is sent successfully <<<<<<<<<\n");
			    count += data_msg.size;
				printf("------------------------------------------------\n");
				printf("------------------------------------------------\n");
			}
			else
			{
				printf("SERVER: Segment checksum is (%d) -- INVALID\n",x );
				printf("SERVER: Do NOT SEND ANY ACK\n");
				printf("************************************\n");
			}
		}
	}
	printf("SERVER: File copying complete\n");
	fclose(file);
}

void fatalerror(int line, char *msg) {
    printf("Line %d : %s\n", line, msg);
    exit(0);
}

/* decide whether the ACK is lost or not given a certain probability */
bool isLost(float prob)
{
     //srand(time(NULL));

        if(((float)(rand())/(float)(RAND_MAX)) < prob){
                return true;
	}
        else return false;
}
