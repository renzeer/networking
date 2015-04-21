//
//  pinggen6.c
//
//  Renzee Reyes


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

static void error (char * s)
{
    perror (s);
    exit (1);
}

static void usage (char * program)
{
    printf ("usage: %s <local-udp> <remote-udp> <ipv6-source> <ipv6-dest>\n", program);
    exit (1);
}

struct icmp {
    uint8_t type;
    uint32_t checksum;
    uint16_t id;
    uint16_t seq;
};

struct ipv6_packet {
    unsigned int 
        version,
        traffic_class,
        flow_label;
    uint16_t length;
    uint8_t next_header;
    uint8_t hop_limit;
    struct in6_addr src;
    struct in6_addr dst;
    struct icmp payload;
};


int main (int argc, char ** argv) {

    // Check command line arguments
    if (argc != 5) {
        usage (argv [0]);
    }
    int local_port = atoi(argv[1]);
    int remote_port = atoi(argv[2]);
    char * server = "::1";

    // Setup socket
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error ("sock");
    }
    struct sockaddr_in6 sin6;
    socklen_t sin6_len = sizeof (sin6);
    socklen_t * sin6_lenp = &sin6_len;
    sin6.sin6_family = AF_INET6;
    if (! inet_pton (AF_INET6, server, &(sin6.sin6_addr))) {
        printf ("invalid server IP %s\n", server);
        exit (1);
    }

    // Bind to local port
    sin6.sin6_port = htons (local_port);
    struct sockaddr * local_sap = (struct sockaddr *) (&sin6);
    if (bind (sockfd, local_sap, sizeof (sin6)) != 0) {
        error ("bind");
    }
    sin6.sin6_port = htons(remote_port);

    // Build ipv6 header
    struct ipv6_packet * packet;
    packet = (struct ipv6_packet *) malloc(sizeof(struct ipv6_packet));
    packet->version = 6;
    packet->traffic_class = 0;
    packet->flow_label = 0;
    packet->length = 0;
    packet->next_header = 0;
    packet->hop_limit = 1;
    if (inet_pton (AF_INET6, argv [3], &packet->src) != 1) {
        printf ("source address %s is not a valid IPv6 address\n", argv [2]);
        return 1;
    }
    if (inet_pton (AF_INET6, argv [4], &packet->dst) != 1) {
        printf ("destination address %s is not a valid IPv6 address\n", argv [3]);
        return 1;
    }
    
    // Build icmp payload
    int seq = 0;
    struct icmp * icmp_echo;
    icmp_echo = (struct icmp *) malloc(sizeof(struct icmp));
    icmp_echo->type = 128;
    icmp_echo->checksum = 0;
    icmp_echo->id = getpid();
    icmp_echo->seq = seq++;

    // Attach paylod to ipv6 packet
    packet->payload = *icmp_echo;

    // Send read bytes
    int sent = sendto(sockfd, packet, sizeof(struct ipv6_packet), 0, (struct sockaddr *) &sin6, sin6_len);
    if (sent < 0) {
        error("sendto");
    }

    // Setup packet
    struct ipv6_packet * in_packet;
    in_packet = (struct ipv6_packet *) malloc(sizeof(struct ipv6_packet));
    int rcvd;

    // Recvfrom and store into data
    rcvd = recvfrom(sockfd, in_packet, sizeof(struct ipv6_packet), 0, (struct sockaddr *) &sin6, sin6_lenp);

    char src_buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(in_packet->src), src_buf, INET6_ADDRSTRLEN);
    char dst_buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(in_packet->dst), dst_buf, INET6_ADDRSTRLEN); 
    printf("Received packet!\n");
    printf("SOURCE: %s\nDEST: %s\nPAYLOAD: %d\n", src_buf, dst_buf, in_packet->payload.type);
    free(icmp_echo);
    free(packet);
    free(in_packet);
    close(sockfd);


}