//
//  sender.c
//
//  Renzee Reyes


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "hw6.h"

static void error (char * s)
{
    perror (s);
    exit (1);
}

static void usage (char * program)
{
    printf ("usage: %s <filename>\n", program);
    exit (1);
}

void handler(int signum){
  printf("Request timed out\n");
  //exit(1);
}

static long file_size (char * file_name)
{
  struct stat st;
  if (stat (file_name, &st) == 0)
    return st.st_size;
  return 0;
}

struct file_packet {
    unsigned char *header;
    char data[100];
};


int main (int argc, char ** argv) {

    // Check args for filename
    if (argc != 2) {
        usage (argv [0]);
    }
    char * filename = argv[1];

    // Get filesize and malloc file_arr
    long filesize = file_size(filename);
    char * file_arr = malloc(filesize);

    // Setup socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error ("sock");
    }    
    int port = PORT_NUMBER;
    char * server = "127.0.0.1";
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof (sin);
    socklen_t * sin_lenp = &sin_len;
    sin.sin_family = AF_INET;
    if (! inet_aton (server, &(sin.sin_addr))) {
        printf ("invalid server IP %s\n", server);
        exit (1);
    }
    sin.sin_port = htons (port);

    // Open file
    FILE * file;
    if((file = fopen(filename,"r")) < 0) {
      error("Error: Could not open file\n");
    }

    // Setup seq and data buffer
    int seq_counter = 0;
    char data[101];
    char ack[1];

    // Setup alarm and handler
    struct sigaction action;
    action.sa_handler = handler;
    sigaction(SIGALRM, &action, NULL);

    // Read 100 bytes of file and send
    while( !feof(file) ) {
        // Write the sequence
        data[0] = (char)(((int)'0')+(seq_counter%2));

        // Read 100 bytes of file
        int numread;
        if (errno != EINTR) {
            if ((numread = fread(data+1, sizeof(char), 100, file)) < 0) {
                error("fread");
            }
        }

        // Send read bytes
        int sent = sendto(sockfd, data, numread+1, 0, (struct sockaddr *) &sin, sin_len);
        if (sent < 0) {
            error("sendto");
        }

        // Receive ack
        errno = 0;
        alarm(2);
        int rcvd = recvfrom(sockfd, ack, sizeof(ack), 0, (struct sockaddr *) &sin, sin_lenp);

        // If ack received, increment seq
        if (errno != EINTR) {
            alarm(0);
            seq_counter++;

            // If numread < 100, indicates last packet
            if(numread < 100) {
                // Print out success message and stats
                printf("Last packet has been acknowledged!\nFile transmission successful\n");
                printf("Filesize: %ld\n# of Transmission: %d\n", filesize, seq_counter);
            }
        } 
    }
    close(sockfd);


}