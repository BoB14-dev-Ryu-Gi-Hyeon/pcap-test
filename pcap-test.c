#include <pcap.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ETHER_HEADER_LEN 14
#define IP_HEADER_DEF_LEN 2
#define TCP_HEADER_DEF_LEN 1

#define ETHER_ADDR_LEN 6
#define IP_ADDR_LEN 4
#define TCP_ADDR_LEN 2

#define LIBNET_LIL_ENDIAN 1
#define LIBNET_BIG_ENDIAN 2

void usage() {
	printf("syntax: pcap-test <interface>\n");
	printf("sample: pcap-test wlan0\n");
}

typedef struct {
	char* dev_;
} Param;

Param param = {
	.dev_ = NULL
};

// --------------------------- libnet 구조체 시작

/*
 *  Ethernet II header
 *  Static header size: 14 bytes
 */
struct libnet_ethernet_hdr
{
    u_int8_t  ether_dhost[ETHER_ADDR_LEN];/* destination ethernet address */
    u_int8_t  ether_shost[ETHER_ADDR_LEN];/* source ethernet address */
    u_int16_t ether_type;                 /* protocol */
};

/*
 *  IPv4 header
 *  Internet Protocol, version 4
 *  Static header size: 20 bytes
 */
struct libnet_ipv4_hdr
{
#if (LIBNET_LIL_ENDIAN)
    u_int8_t ip_hl:4,      /* header length */
           ip_v:4;         /* version */
#endif

    u_int8_t ip_tos;       /* type of service */
#ifndef IPTOS_LOWDELAY
#define IPTOS_LOWDELAY      0x10
#endif
#ifndef IPTOS_THROUGHPUT
#define IPTOS_THROUGHPUT    0x08
#endif
#ifndef IPTOS_RELIABILITY
#define IPTOS_RELIABILITY   0x04
#endif
#ifndef IPTOS_LOWCOST
#define IPTOS_LOWCOST       0x02
#endif
    u_int16_t ip_len;         /* total length */
    u_int16_t ip_id;          /* identification */
    u_int16_t ip_off;
#ifndef IP_RF
#define IP_RF 0x8000        /* reserved fragment flag */
#endif
#ifndef IP_DF
#define IP_DF 0x4000        /* dont fragment flag */
#endif
#ifndef IP_MF
#define IP_MF 0x2000        /* more fragments flag */
#endif 
#ifndef IP_OFFMASK
#define IP_OFFMASK 0x1fff   /* mask for fragmenting bits */
#endif
    u_int8_t ip_ttl;          /* time to live */
    u_int8_t ip_p;            /* protocol */
    u_int16_t ip_sum;         /* checksum */
    struct in_addr ip_src, ip_dst; /* source and dest address */
};

/*
 *  TCP header
 *  Transmission Control Protocol
 *  Static header size: 20 bytes
 */
struct libnet_tcp_hdr
{
    u_int16_t th_sport;       /* source port */
    u_int16_t th_dport;       /* destination port */
    u_int32_t th_seq;          /* sequence number */
    u_int32_t th_ack;          /* acknowledgement number */
#if (LIBNET_LIL_ENDIAN)
    u_int8_t th_x2:4,         /* (unused) */
           th_off:4;        /* data offset */
#endif

    u_int8_t  th_flags;       /* control flags */
#ifndef TH_FIN
#define TH_FIN    0x01      /* finished send data */
#endif
#ifndef TH_SYN
#define TH_SYN    0x02      /* synchronize sequence numbers */
#endif
#ifndef TH_RST
#define TH_RST    0x04      /* reset the connection */
#endif
#ifndef TH_PUSH
#define TH_PUSH   0x08      /* push data to the app layer */
#endif
#ifndef TH_ACK
#define TH_ACK    0x10      /* acknowledge */
#endif
#ifndef TH_URG
#define TH_URG    0x20      /* urgent! */
#endif
#ifndef TH_ECE
#define TH_ECE    0x40
#endif
#ifndef TH_CWR   
#define TH_CWR    0x80
#endif
    u_int16_t th_win;         /* window */
    u_int16_t th_sum;         /* checksum */
    u_int16_t th_urp;         /* urgent pointer */
};

// --------------------------- libnet 구조체 끝

bool parse(Param* param, int argc, char* argv[]) {
	if (argc != 2) {
		usage();
		return false;
	}
	param->dev_ = argv[1];
	return true;
}

int main(int argc, char* argv[]) {
	if (!parse(&param, argc, argv))
		return -1;

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t* pcap = pcap_open_live(param.dev_, BUFSIZ, 1, 1000, errbuf);
	if (pcap == NULL) {
		fprintf(stderr, "pcap_open_live(%s) return null - %s\n", param.dev_, errbuf);
		return -1;
	}

	while (true) {
		struct pcap_pkthdr* header;
		const u_char* packet;
		int res = pcap_next_ex(pcap, &header, &packet);
		if (res == 0) continue;
		if (res == PCAP_ERROR || res == PCAP_ERROR_BREAK) {
			printf("pcap_next_ex return %d(%s)\n", res, pcap_geterr(pcap));
			break;
		}

		// 패킷을 통한 헤더 정의
		const struct libnet_ethernet_hdr* eth_hdr = (const struct libnet_ethernet_hdr*)packet;

		// 이더넷헤더로부터 IPv4 패킷 필터링
		if (ntohs(eth_hdr->ether_type) != 0x0800) {
			continue;
		}

		const struct libnet_ipv4_hdr* ip_hdr = (const struct libnet_ipv4_hdr*)(packet + sizeof(struct libnet_ethernet_hdr));
		
		const struct libnet_tcp_hdr* tcp_hdr = (const struct libnet_tcp_hdr*)(packet + sizeof(struct libnet_ethernet_hdr) + (ip_hdr->ip_hl * 4));

		// 헤더 길이
		int tcp_header_len = tcp_hdr->th_off * 4;
		int ip_header_len = ip_hdr->ip_hl * 4;
		
		// TCP 페이로드 정의, 페이로드 길이
		const u_char* tcp_payload = (const u_char*)packet + sizeof(struct libnet_ethernet_hdr) + (ip_hdr->ip_hl * 4) + tcp_header_len;
		int payload_len = header->caplen - sizeof(struct libnet_ethernet_hdr) - ip_header_len - tcp_header_len;

		// TCP 패킷 필터링
		if (ip_hdr->ip_p == IPPROTO_TCP) {

			printf("%u bytes captured\n\n", header->caplen);

			// 이더넷 출력
			u_int8_t src_mac[ETHER_ADDR_LEN];
			u_int8_t dest_mac[ETHER_ADDR_LEN];

			memcpy(src_mac, eth_hdr->ether_shost, ETHER_ADDR_LEN);
			memcpy(dest_mac, eth_hdr->ether_dhost, ETHER_ADDR_LEN);

			printf("Ethernet Address\n");
			printf("src MAC : ");
			for (int i=0; i<ETHER_ADDR_LEN; i++){
				printf("%02x", src_mac[i]);
				if (i < ETHER_ADDR_LEN - 1) {
            		printf(":");
				}
			}
			printf("\ndest MAC : ");
			for (int i=0; i<ETHER_ADDR_LEN; i++){
				printf("%02x", dest_mac[i]);
				if (i < ETHER_ADDR_LEN - 1) {
            		printf(":");
				}
			}
			printf("\n\n");

			// IP 출력
			const uint8_t* src_ip = (const uint8_t*)&ip_hdr->ip_src;
    		const uint8_t* dest_ip = (const uint8_t*)&ip_hdr->ip_dst;

			printf("IP Address\n");
			// printf("IP 헤더 사이즈 : %d\n", ip_hdr->ip_hl * 4);
			printf("src IP : ");
    		for (int i = 0; i < IP_ADDR_LEN; i++) {
        		printf("%d", src_ip[i]);
        		if (i < IP_ADDR_LEN - 1) {
            		printf(".");
        		}
    		}
			printf("\n");
			printf("dest IP : ");
    		for (int i = 0; i < IP_ADDR_LEN; i++) {
        		printf("%d", dest_ip[i]);
        		if (i < IP_ADDR_LEN - 1) {
        	    	printf(".");
        		}
    		}
    		printf("\n\n");

			// PORT 출력
			printf("PORT Address\n");
			printf("src PORT : %d\n", ntohs(tcp_hdr->th_sport));
			printf("dest PORT : %d\n", ntohs(tcp_hdr->th_dport));


			// TCP 페이로드 출력
			if (payload_len <= 0){
				printf("No data\n\n\n");
				continue;
			}
    		for (int i = 0; i < 20; i++) {
        		printf("%02x ", tcp_payload[i]);
    		}

			printf("\n\n\n");
		}

	}

	pcap_close(pcap);
}
