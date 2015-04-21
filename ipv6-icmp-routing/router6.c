//
//  router6.c
//
//  Renzee Reyes

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define IBUFSIZE 9999
#define RXBUFSIZE 100
#define RYBUFSIZE 50
#define BUFSIZE 9999

static void error (char * s)
{
    perror (s);
    exit (1);
}

static void usage (char * program)
{
    printf ("usage: %s <+interface/local-IPv6/mask/my-UDP-port/peer-UDP-port> (<destination/gateway/mask/interface>...)...\n", program);
    exit (1);
}

struct route {
    struct in6_addr dest;
    struct in6_addr gateway;
    int mask;
    char * interface;
};


struct interface {
    char * name;
    struct in6_addr local_IP;
    int mask;
    int local_port;
    int remote_port;
};

struct icmp {
    uint8_t type;
    uint32_t checksum;
    uint16_t id;
    uint16_t seq;
};

struct ipv6_packet
{
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

struct arguments {
  struct interface **arg_interfaces;
  struct route **arg_routes;
  int r_length;
  int i_length;
  int i_index;
};

pthread_mutex_t mutex;
void *do_work(void *arg);

int main (int argc, char ** argv) {

    // Check for command line arguments
    if (argc < 3) {
        usage (argv [0]);
    }
    int local_port = atoi(argv[1]);

    // Process command line arguments
    struct route *routes[BUFSIZE];
    struct interface *interfaces[BUFSIZE];
    int iSize = 0;
    int rSize = 0;
    for (int i = 1; i < argc; i++) {
        // If argument is interface definition
        if (argv[i][0] == '+') {
            interfaces[iSize] = (struct interface *) malloc(sizeof(struct interface));
            interfaces[iSize]->name = strtok(argv[i]+1, "/");
            inet_pton(AF_INET6, strtok(NULL, "/"), &(interfaces[iSize]->local_IP));
            interfaces[iSize]->mask = atoi(strtok(NULL, "/"));
            interfaces[iSize]->local_port = atoi(strtok(NULL, "/"));
            interfaces[iSize]->remote_port = atoi(strtok(NULL, "/"));

            // DEBUGGING PRINT STATEMENTS
            /*
            printf("INTERFACE:\n");
            printf("\tNAME: %s\n", interfaces[iSize]->name);
            char dl_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(interfaces[iSize]->local_IP), dl_buf, INET6_ADDRSTRLEN);
            printf("\tLOCAL: %s\n", dl_buf);
            printf("\tMASK: %d\n", interfaces[iSize]->mask);
            printf("\tLOCAL PORT: %d\n", interfaces[iSize]->local_port);
            printf("\tREMOTE PORT: %d\n", interfaces[iSize]->remote_port);
            */

            iSize++;
            
        // Else argument is route definition
        } else {
            // Assumes routes always come after interface declaration
            routes[rSize] = (struct route *) malloc(sizeof(struct route));
            inet_pton(AF_INET6, strtok(argv[i], "/"), &(routes[rSize]->dest));
            inet_pton(AF_INET6, strtok(NULL, "/"), &(routes[rSize]->gateway));
            routes[rSize]->mask = atoi(strtok(NULL, "/"));
            routes[rSize]->interface = strtok(NULL, "/");
            
            // DEBUGGING PRINT STATEMENTS
            /*
            printf("ROUTE:\n");
            char rd_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(routes[rSize]->dest), rd_buf, INET6_ADDRSTRLEN);
            printf("\tDEST: %s\n", rd_buf);
            char rg_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(routes[rSize]->gateway), rg_buf, INET6_ADDRSTRLEN);
            printf("\tGATE: %s\n", rg_buf);
            printf("\tMASK: %d\n", routes[rSize]->mask);
            printf("\tINTERFACE: %s\n", routes[rSize]->interface);
            */

            rSize++;
   
        }
        
    }

    // Initialize threads and arugments
    pthread_t worker_thread[iSize];
    pthread_mutex_init(&mutex, NULL);
    struct arguments *args[iSize];
    for (int j = 0; j < iSize; j++) {
        args[j] = (struct arguments *) malloc(sizeof(struct arguments));
        args[j]->arg_interfaces = interfaces;
        args[j]->arg_routes = routes;
        args[j]->r_length = rSize;
        args[j]->i_length = iSize;
        args[j]->i_index = j;
        
        // DEBUGGING PRINT STATEMENTS
        /*
        printf("ARGUMENTS FOR THREAD %d\n", j);
        printf("\tNAME: %s\n", args[j]->arg_interfaces[j]->name);
        printf("\tROUTE DESTINATIONS:\n");
        for (int k = 0; k < rSize; k++) {
            char temp[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(args[j]->arg_routes[k]->dest), temp, INET6_ADDRSTRLEN);
            printf("\t\t%s\n", temp);
        }
        printf("\tLENS: %d & %d\n", args[j]->r_length, args[j]->i_length);
        printf("\tINDEX: %d\n", args[j]->i_index);
        */
        

    }

    
    // For each interface, create thread to bind, recv, and process packet
    for (int i=0; i < iSize; i++) {
        if (pthread_create(&worker_thread[i], NULL, do_work, (void *)args[i])) {
            fprintf(stderr,"Error while creating thread #%d\n",i);
            exit(1);
        }
    }

    // Joining with child threads
    for (int i=0; i < iSize; i++) {
        if (pthread_join(worker_thread[i], NULL)) {
            fprintf(stderr,"Error while joining with child thread #%d\n",i);
            exit(1);
        }
    }
    
    //free(interfaces);
    //free(routes);
    
}

void *do_work(void *arg) {
    struct arguments *args;
    struct interface **myInterfaces;
    struct route **myRoutes;
    int r_length, i_length, i_index;

    args = (struct arguments*) arg;
    myInterfaces = args->arg_interfaces;
    myRoutes = args->arg_routes;
    r_length = args->r_length;
    i_length = args->i_length;
    i_index = args->i_index;

    // Setup socket
    char * server = "::1";
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error ("sock");
    }
    struct sockaddr_in6 sin6;
    sin6.sin6_family = AF_INET6;
    socklen_t sin6_len = sizeof (sin6);
    socklen_t * sin6_lenp = &sin6_len;
    if (! inet_pton (AF_INET6, server, &(sin6.sin6_addr))) {
        printf ("invalid server IP %s\n", server);
        exit (1);
    }

    // Bind to local port
    sin6.sin6_port = htons (myInterfaces[i_index]->local_port);
    struct sockaddr * sap = (struct sockaddr *) (&sin6);
    if (bind (sockfd, sap, sizeof (sin6)) != 0) {
        error ("bind");
    }

    
    while (1) {
        // Setup packet
        struct ipv6_packet * packet;
        packet = (struct ipv6_packet *) malloc(sizeof(struct ipv6_packet));
        int rcvd;

        // Recvfrom and store into data
        rcvd = recvfrom(sockfd, packet, sizeof(struct ipv6_packet), 0, (struct sockaddr *) &sin6, sin6_lenp);

        /*
        printf("ICMP PAYLOAD\n");
        printf("type: %d\n", packet->payload.type);
        printf("checksum: %d\n", packet->payload.checksum);
        printf("id: %d\n", packet->payload.id);
        printf("seq: %d\n\n", packet->payload.seq);
        */

        struct route matching_route;
        matching_route.mask = -1;

        for (int i = 0; i < r_length; i++) {
            // Bitmask for route destination
            struct in6_addr rdst_mask;
            for (int j = 0; j <= 16; j++) {
                if (j <= (myRoutes[i]->mask)/8) {
                    rdst_mask.s6_addr[j] = myRoutes[i]->dest.s6_addr[j];
                } else {
                    rdst_mask.s6_addr[j] = 0;
                }
            }
            
            /*
            char tmp1[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(rdst_mask.s6_addr), tmp1, INET6_ADDRSTRLEN);
            printf("ROUTE MASKED ADDRESS: %s\n", tmp1);
            */
            

            // Bitmask for destination
            struct in6_addr dst_mask;
            for (int k = 0; k <= 16; k++) {
                if (k <= (myRoutes[i]->mask)/8) {
                    dst_mask.s6_addr[k] = packet->dst.s6_addr[k];
                } else {
                    dst_mask.s6_addr[k] = 0;
                }
            }

            /*
            char tmp2[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(dst_mask.s6_addr), tmp2, INET6_ADDRSTRLEN);
            printf("DESTINATION MASKED ADRESS: %s\n", tmp2);
            */
            
            
            // Compare IPs and replace current match if needed
            if (memcmp(rdst_mask.s6_addr, dst_mask.s6_addr, sizeof(rdst_mask.s6_addr)) == 0) {
                if ((matching_route.mask < myRoutes[i]->mask) || matching_route.mask == -1) {
                    matching_route.dest = myRoutes[i]->dest;
                    matching_route.gateway = myRoutes[i]->gateway;
                    matching_route.mask = myRoutes[i]->mask;
                    matching_route.interface = myRoutes[i]->interface;
                }
            }  
        }

        // Prepare IPs for printing
        char src_buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(packet->src), src_buf, INET6_ADDRSTRLEN);
        char dst_buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(packet->dst), dst_buf, INET6_ADDRSTRLEN); 

        // Print results
        if (matching_route.mask != -1) {
            char hop_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(matching_route.gateway.s6_addr), hop_buf, INET6_ADDRSTRLEN);
            printf("Forwarding %s to next hop %s on %s\n", src_buf, hop_buf, matching_route.interface);
            //printf("SOURCE: %s\nDEST: %s\nNEXTHOP: %s\nINTERFACE: %s\n", src_buf, dst_buf, hop_buf, matching_route.interface);

            // Send packet over appropriate interface
            for (int i = 0; i < i_length; i++) {
                if (strcmp(myInterfaces[i]->name, matching_route.interface) == 0) {
                    sin6.sin6_port = htons(myInterfaces[i]->remote_port);
                    //printf("PORT: %d\n", myInterfaces[i]->remote_port);
                    int sent = sendto(sockfd, packet, sizeof(struct ipv6_packet), 0, (struct sockaddr *) &sin6, sin6_len);
                    if (sent < 0) {
                        error("sendto");
                    }
                }
            }

            
        } else {
            printf("destination %s not found in routing table, dropping packet\n", dst_buf);
        }
    }

    return NULL;
}
