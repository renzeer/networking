//
//  receiver.c
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
  exit(1);
}

int file_exists(char *filename)
{
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

int main (int argc, char ** argv) {

    // Check args for filename
    if (argc != 2) {
        usage (argv [0]);
    }
    char * filename = argv[1];

    // Check if file exists
    if (file_exists(filename)) {
        error("exists");
    }

    // Create file
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        error("file");
    }

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
    struct sockaddr * sap = (struct sockaddr *) (&sin);
    if (bind (sockfd, sap, sizeof (sin)) != 0) {
        error ("bind");
    }

    // Setup vars
    int seq_counter = 0;
    char data[101];
    char ack[1];
    int rcvd;

    // Repeatedly read packets until all data is received
    while (1) {
        // Recvfrom and store into data
        rcvd = recvfrom(sockfd, data, sizeof(data), 0, (struct sockaddr *) &sin, sin_lenp);

        // Store seq of data
        int seq = (int) (data[0]-'0');

        // Check if seq matches seq_counter, indicating next new packet
        if ((seq_counter%2) == seq) {

            // Write new packet into file
            fwrite(data+1, sizeof(char), rcvd-1, fp);

            // Update seq_counter to prepare for next new packet
            seq_counter++;
        }

        // Send acks for all packets, new and retransmitted
        ack[0] = (char)(((int)'2')+(seq%2));
        sendto(sockfd, ack, sizeof(ack), 0, (struct sockaddr *) &sin, sin_len);

        // Check if rcvd < 100, indicating EOF
        if (rcvd < 100) {
            break;
        }
    }
    fclose(fp);
    close(sockfd);

}