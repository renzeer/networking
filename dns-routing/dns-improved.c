//
//  dns-improved.c
//
//
//  GROUP MEMEBERS:
//  Renzee Reyes
//  Victor Gong
//  Garry Hwang

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static void error (char * s)
{
    perror (s);
    exit (1);
}

static void usage (char * program)
{
    printf ("usage: %s domain [dns]\n", program);
    exit (1);
}

void print_buffer (char * buffer, int num_bytes) {
    for (int i = 0; i < num_bytes; i++) {
        printf ("%02x ", (unsigned char) buffer[i]);
    }
    printf("\n");
}

void handler(int signum){
  printf("Hello World!\n");
}

// Assumes 0 <= max <= RAND_MAX
// Returns in the half-open interval [0, max]
long random_at_most(long max) {
  unsigned long
    // max <= RAND_MAX < ULONG_MAX, so this is okay.
    num_bins = (unsigned long) max + 1,
    num_rand = (unsigned long) RAND_MAX + 1,
    bin_size = num_rand / num_bins,
    defect   = num_rand % num_bins;

  long x;
  do {
   x = random();
  }
  // This is carefully written not to overflow
  while (num_rand - defect <= (unsigned long)x);

  // Truncated division is intentional
  return x/bin_size;
}

struct dns_header {
    unsigned short id;
    unsigned short flags;
    unsigned short qcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
};

int main (int argc, char ** argv) {
    int port = 53;
    //int server_socket = socket (AF_INET, SOCK_STREAM, 0);
    int server_socket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        error ("socket");
    }
    
    if ((argc != 2) && (argc != 3)) {
        usage (argv [0]);
    }
    char * url = argv [1];
    // Had issues with UH DNS server, so using Google's DNS server
    char * server = "8.8.8.8";
    //char * server = "128.171.3.13";
    //char * server = "127.0.0.1";
    if (argc == 3) {
        server = argv [2];
    }
    
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof (sin);
    socklen_t * sin_lenp = &sin_len;
    sin.sin_family = AF_INET;
    if (! inet_aton (server, &(sin.sin_addr))) {
        printf ("invalid server IP %s\n", server);
        exit (1);
    }
    sin.sin_port = htons (port);
    
    struct dns_header *header;
    header = malloc(sizeof(struct dns_header));
    header->id = htons(rand() % 256);
    header->flags = htons(0x0100);
    header->qcount = htons(0x0001);
    header->ancount = htons(0x0000);
    header->nscount = htons(0x0000);
    header->arcount = htons(0x0000);
    
    char buf[1000];
    int buf_len = 0;

    memcpy(buf, header, 12);
    buf_len += 12;

    
    int l;
    char * b;

    while (1){
        l = strcspn(url, ".");
        if (l != 0) {
            b = (char *) malloc(l+1);
            if (b == NULL) {
                printf("malloc error!");
                exit(1);
            }
            snprintf(b, l+1, "%s", url);
            b[l] = '\0';

            memset(buf+buf_len, l, 1);
            buf_len += 1;
            memcpy(buf+buf_len, b, l);
            buf_len += sizeof(char) * l;

            free(b);
        }
        if (!strchr(url, '.')) {
            memset(buf+buf_len, 0, 1);
            buf_len += 1;
            break;
        }
        url += l+1;
        
    }

    unsigned short qtype = htons(0x001c);
    unsigned short qclass = htons(0x0001);
    
    memcpy(buf+buf_len, &qtype, 2);
    buf_len += 2;
    memcpy(buf+buf_len, &qclass, 2);
    buf_len += 2;
    sendto (server_socket, buf, buf_len, 0, (struct sockaddr *) &sin, sin_len);
    
    struct sigaction action;
    action.sa_handler = handler;
    sigaction(SIGALRM, &action, NULL);

    alarm(5);
    char recv_buffer[1000];
    int rcvd = recvfrom(server_socket, recv_buffer, sizeof(recv_buffer)-1, 0, (struct sockaddr *) &sin, sin_lenp);

    recv_buffer[rcvd] = '\0';
    print_buffer (recv_buffer, rcvd);


    close(server_socket);

}