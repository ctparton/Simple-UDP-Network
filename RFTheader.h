 #include <stdio.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h> // for sockaddr_in
 #include <stdlib.h>
 #include <unistd.h>
 #define NAMESTRING 100
 #define TOTALCHAR  15

 typedef struct {
      char  name[NAMESTRING];  //name of the file
      int size;                //size of the file
} meta_data;

enum s_type{
  TYPE_DATA,                   //data segment
  TYPE_ACK                     //Ack segment
};

typedef struct{
        int size;                //size of the payload
        int sq;                  //sequence number
	 	enum s_type type;        //segment type
        char payload[TOTALCHAR]; //data
        int checksum;            //checksum
}segment;
 
 