#define _GNU_SOURCE
#include <pcap.h>
#include <time.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include "flow.h"
#include "decode.h"

int decode_ethernet(const struct pcap_pkthdr *h, const uint8_t *wirebits,
                    struct pkt_record *pkt, char *errstr)
{
	const struct hdr_ethernet *ethernet;
	int ret;

	pkt->timestamp.tv_sec = h->ts.tv_sec;
	pkt->timestamp.tv_usec = h->ts.tv_usec;
	pkt->len = h->len;

	ethernet = (struct hdr_ethernet *)wirebits;

	switch (ntohs(ethernet->type)) {
	case ETHERTYPE_IP:
		ret = decode_ip4(wirebits + HDR_LEN_ETHER, pkt, errstr);
		break;
	case ETHERTYPE_VLAN:
		ret = decode_ethernet(h, wirebits + HDR_LEN_ETHER_VLAN, pkt,
		                      errstr);
		break;
	case ETHERTYPE_IPV6:
		ret = decode_ip6(wirebits + HDR_LEN_ETHER, pkt, errstr);
		break;
	case ETHERTYPE_ARP:
		snprintf(errstr, DECODE_ERRBUF_SIZE, "%s", "ARP ignored");
		ret = -1;
		break;
	case ETHERTYPE_LLDP:
		snprintf(errstr, DECODE_ERRBUF_SIZE, "%s", "LLDP ignored");
		ret = -1;
		break;
	default:
		/* we don't know how to decode other types right now. */
		snprintf(errstr, DECODE_ERRBUF_SIZE,
		         "EtherType [0x%04x] ignored", ntohs(ethernet->type));
		ret = -1;
		break;
	}
	return ret;
}

int decode_ip6(const uint8_t *packet, struct pkt_record *pkt, char *errstr)
{
	int ret;
	const void *next = (uint8_t *)packet + sizeof(struct hdr_ipv6);
	const struct hdr_ipv6 *ip6_packet = (const struct hdr_ipv6 *)packet;

	pkt->flow.ethertype = ETHERTYPE_IPV6;
	pkt->flow.src_ip6 = (ip6_packet->ip6_src);
	pkt->flow.dst_ip6 = (ip6_packet->ip6_dst);

	/* Transport proto TCP/UDP/ICMP */
	switch (ip6_packet->next_hdr) {
	case IPPROTO_TCP:
		ret = decode_tcp(next, pkt, errstr);
		break;
	case IPPROTO_UDP:
		ret = decode_udp(next, pkt, errstr);
		break;
	case IPPROTO_ICMP:
		ret = decode_icmp(next, pkt, errstr);
		break;
	case IPPROTO_IGMP:
		ret = decode_igmp(next, pkt, errstr);
		break;
	case IPPROTO_ICMPV6:
		ret = decode_icmp6(next, pkt, errstr);
		break;
	default:
		snprintf(errstr, DECODE_ERRBUF_SIZE,
		         "*** Protocol [0x%02x] unknown", ip6_packet->next_hdr);
		ret = -1;
		break;
	}
	return ret;
}

int decode_ip4(const uint8_t *packet, struct pkt_record *pkt, char *errstr)
{
	int ret;
	const void *next;
	const struct hdr_ipv4 *ip4_packet = (const struct hdr_ipv4 *)packet;
	unsigned int size_ip = IP_HL(ip4_packet) * 4;
	if (size_ip < 20) {
		snprintf(errstr, DECODE_ERRBUF_SIZE,
		         "*** Invalid IP header length: %u bytes", size_ip);
		return -1;
	}
	next = ((uint8_t *)ip4_packet + size_ip);

	pkt->flow.ethertype = ETHERTYPE_IP;
	pkt->flow.src_ip = (ip4_packet->ip_src);
	pkt->flow.dst_ip = (ip4_packet->ip_dst);

	/* IP proto TCP/UDP/ICMP */
	switch (ip4_packet->ip_p) {
	case IPPROTO_TCP:
		ret = decode_tcp(next, pkt, errstr);
		break;
	case IPPROTO_UDP:
		ret = decode_udp(next, pkt, errstr);
		break;
	case IPPROTO_ICMP:
		ret = decode_icmp(next, pkt, errstr);
		break;
	case IPPROTO_IGMP:
		ret = decode_igmp(next, pkt, errstr);
		break;
	default:
		snprintf(errstr, DECODE_ERRBUF_SIZE,
		         "*** Protocol [0x%02x] unknown", ip4_packet->ip_p);
		ret = -1;
		break;
	}
	return ret;
}

int decode_tcp(const struct hdr_tcp *packet, struct pkt_record *pkt,
               char *errstr)
{
	unsigned int size_tcp = (TH_OFF(packet) * 4);

	if (size_tcp < 20) {
		snprintf(errstr, DECODE_ERRBUF_SIZE,
		         "*** Invalid TCP header length: %u bytes", size_tcp);
		return -1;
	}

	pkt->flow.proto = IPPROTO_TCP;
	pkt->flow.sport = ntohs(packet->sport);
	pkt->flow.dport = ntohs(packet->dport);
	return 0;
}

int decode_udp(const struct hdr_udp *packet, struct pkt_record *pkt,
               char *errstr)
{
	pkt->flow.proto = IPPROTO_UDP;
	pkt->flow.sport = ntohs(packet->sport);
	pkt->flow.dport = ntohs(packet->dport);
	return 0;
}

int decode_icmp(const struct hdr_icmp *packet, struct pkt_record *pkt,
                char *errstr)
{
	pkt->flow.proto = IPPROTO_ICMP;
	/* ICMP doesn't have ports, but we depend on that for the flow */
	pkt->flow.sport = 0;
	pkt->flow.dport = 0;
	return 0;
}

int decode_igmp(const struct hdr_icmp *packet, struct pkt_record *pkt,
                char *errstr)
{
	pkt->flow.proto = IPPROTO_IGMP;
	/* IGMP doesn't have ports, but we depend on that for the flow */
	pkt->flow.sport = 0;
	pkt->flow.dport = 0;
	return 0;
}

int decode_icmp6(const struct hdr_icmp *packet, struct pkt_record *pkt,
                 char *errstr)
{
	pkt->flow.proto = IPPROTO_ICMPV6;
	pkt->flow.sport = 0;
	pkt->flow.dport = 0;
	return 0;
}
