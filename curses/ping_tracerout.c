#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <time.h>

#define PING_COUNT 4

// --------------------- Checksum for IPv4 ICMP ---------------------
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >>16);
    result = ~sum;
    return result;
}

// --------------------- Combined IPv4/IPv6 Ping ---------------------
void ping_host(const char *host) {
    printf("\n=== Ping %s ===\n", host);

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_RAW;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        perror("getaddrinfo");
        return;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
            printf("\nIPv4 address: %s\n", inet_ntoa(addr->sin_addr));

            for (int seq = 1; seq <= PING_COUNT; seq++) {
                int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
                if (sockfd < 0) {
                    perror("socket");
                    continue;
                }

                struct icmp icmp_pkt;
                memset(&icmp_pkt, 0, sizeof(icmp_pkt));
                icmp_pkt.icmp_type = ICMP_ECHO;
                icmp_pkt.icmp_code = 0;
                icmp_pkt.icmp_id = getpid() & 0xFFFF;
                icmp_pkt.icmp_seq = seq;
                icmp_pkt.icmp_cksum = checksum(&icmp_pkt, sizeof(icmp_pkt));

                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);

                if (sendto(sockfd, &icmp_pkt, sizeof(icmp_pkt), 0,
                           (struct sockaddr*)addr, sizeof(*addr)) <= 0) {
                    perror("sendto");
                    close(sockfd);
                    continue;
                }

                char buf[1024];
                socklen_t addrlen = sizeof(*addr);
                if (recvfrom(sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr*)addr, &addrlen) <= 0) {
                    perror("recvfrom");
                    close(sockfd);
                    continue;
                }

                clock_gettime(CLOCK_MONOTONIC, &end);
                double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                             (end.tv_nsec - start.tv_nsec) / 1000000.0;

                printf("Reply from %s: seq=%d time=%.2f ms\n",
                       inet_ntoa(addr->sin_addr), seq, rtt);
                close(sockfd);
                sleep(1);
            }

        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)p->ai_addr;
            char ipstr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipstr, sizeof(ipstr));
            printf("\nIPv6 address: %s\n", ipstr);

            for (int seq = 1; seq <= PING_COUNT; seq++) {
                int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
                if (sockfd < 0) {
                    perror("socket");
                    continue;
                }

                struct icmp6_hdr icmp6_pkt;
                memset(&icmp6_pkt, 0, sizeof(icmp6_pkt));
                icmp6_pkt.icmp6_type = ICMP6_ECHO_REQUEST;
                icmp6_pkt.icmp6_code = 0;
                icmp6_pkt.icmp6_id = getpid() & 0xFFFF;
                icmp6_pkt.icmp6_seq = seq;

                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);

                if (sendto(sockfd, &icmp6_pkt, sizeof(icmp6_pkt), 0,
                           (struct sockaddr*)addr6, sizeof(*addr6)) <= 0) {
                    perror("sendto");
                    close(sockfd);
                    continue;
                }

                char buf[1024];
                socklen_t addrlen = sizeof(*addr6);
                if (recvfrom(sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr*)addr6, &addrlen) <= 0) {
                    perror("recvfrom");
                    close(sockfd);
                    continue;
                }

                clock_gettime(CLOCK_MONOTONIC, &end);
                double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                             (end.tv_nsec - start.tv_nsec) / 1000000.0;

                printf("Reply from %s: seq=%d time=%.2f ms\n", ipstr, seq, rtt);
                close(sockfd);
                sleep(1);
            }
        }
    }

    freeaddrinfo(res);
}

// --------------------- Local + External IP ---------------------
void print_local_and_external_ip() {
    printf("=== Local and External IP ===\n");

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
        struct addrinfo hints, *res, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
            for (p = res; p != NULL; p = p->ai_next) {
                char ipstr[INET6_ADDRSTRLEN];
                void *addr;
                if (p->ai_family == AF_INET) {
                    addr = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
                } else {
                    addr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
                }
                inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
                printf("Local IP: %s\n", ipstr);
            }
            freeaddrinfo(res);
        }
    }

    printf("External IP: ");
    fflush(stdout);
    system("curl -s https://api.ipify.org");
    printf("\n");
}

// --------------------- Traceroute (placeholder) ---------------------
void traceroute_host(const char *host) {
    printf("\n=== Traceroute to %s ===\n", host);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "traceroute %s", host);
    system(cmd);
}

// --------------------- Main ---------------------
int main() {
    print_local_and_external_ip();
    ping_host("www.bbc.co.uk");   // IPv4 + IPv6 automatic
    traceroute_host("www.etsy.co.uk");
    return 0;
}
