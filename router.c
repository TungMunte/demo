#include "queue.h"
#include "list.h"
#include "skel.h"

void info(struct ether_header *eth, struct iphdr *ip_hdr, struct arp_header *ARP_header, struct icmphdr *icmp_hdr, packet *m)
{
	printf("\n\n\n");
	printf("-----  ETH header   -----\n");
	printf("ETH type : %X\n", ntohs(eth->ether_type));
	printf("ETH source : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", eth->ether_shost[i]);
	}
	printf("\n");
	printf("ETH destin : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", eth->ether_dhost[i]);
	}
	printf("\n");
	printf("------  IP header   -----\n");
	printf("IP version : %d\n", ip_hdr->version);
	printf("IP TTL     : %d\n", ip_hdr->ttl);
	printf("IP source  : %X\n", ip_hdr->saddr);
	printf("IP destin  : %X\n", ip_hdr->daddr);
	printf("------  ARP header  -----\n");
	printf("ARP htype      : %d\n", ntohs(ARP_header->htype));
	printf("ARP ptype      : %d\n", ntohs(ARP_header->ptype));
	printf("ARP hlen       : %d\n", ARP_header->hlen);
	printf("ARP plen       : %d\n", ARP_header->plen);
	printf("ARP op         : %d\n", ntohs(ARP_header->op));
	printf("ARP ip  source : %X\n", ARP_header->spa);
	printf("ARP mac source : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", ARP_header->sha[i]);
	}
	printf("\n");
	printf("ARP ip  destin : %X\n", ARP_header->tpa);
	printf("ARP mac destin : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", ARP_header->tha[i]);
	}
	printf("\n");
	printf("-----  ICMP header  -----\n");
	printf("ICMP type : %d\n", icmp_hdr->type);
	printf("ICMP code : %d\n", icmp_hdr->code);
	printf("-----    outside    -----\n");
	char *mac_string = get_interface_ip(m->interface);
	uint8_t *mac_int = (uint8_t *)malloc(sizeof(uint8_t) * 6);
	get_interface_mac(m->interface, mac_int);
	printf("first mac  : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", mac_string[i]);
	}
	printf("\n");
	printf("second mac : ");
	for (size_t i = 0; i < ETH_ALEN; i++)
	{
		printf("%X:", mac_int[i]);
	}
	printf("\n");
}

struct route_table_entry *find_in_routing_table(struct route_table_entry *rtable01, struct route_table_entry *rtable02, uint32_t ip_dest)
{
	int index01 = -1, index02 = -1;
	for (size_t i = 0; i < 64273; i++)
	{
		if ((ip_dest & rtable01[i].mask) == rtable01[i].prefix)
		{
			if (index01 == -1)
			{
				index01 = i;
			}
			else if (rtable01[index01].mask < rtable01[i].mask)
			{
				index01 = i;
			}
		}
	}
	if (index01 == -1)
	{
		for (size_t i = 0; i < 64275; i++)
		{
			if ((ip_dest & rtable02[i].mask) == rtable02[i].prefix)
			{
				if (index02 == -1)
				{
					index02 = i;
				}
				else if (rtable02[index02].mask < rtable02[i].mask)
				{
					index02 = i;
				}
			}
		}
		if (index02 == -1)
		{
			return NULL;
		}
		else
		{
			return &rtable02[index02];
		}
	}
	else
	{
		for (size_t i = 0; i < 64275; i++)
		{
			if ((ip_dest & rtable02[i].mask) == rtable02[i].prefix)
			{
				if (index02 == -1)
				{
					index02 = i;
				}
				else if (rtable02[index02].mask < rtable02[i].mask)
				{
					index02 = i;
				}
			}
		}
		if (index02 == -1)
		{
			return &rtable01[index01];
		}
		else
		{
			if (rtable01[index01].mask <= rtable02[index02].mask)
			{
				return &rtable02[index02];
			}
			else
			{
				return &rtable01[index01];
			}
		}
	}
}

struct arp_entry *find_in_cache(struct queue *cache, uint32_t next_hop)
{
	struct queue *restore = queue_create();
	while (queue_empty(cache) == 0)
	{
		struct arp_entry *temp = (struct arp_entry *)queue_deq(cache);
		queue_enq(restore, (struct arp_entry *)temp);
		if (temp->ip == (unsigned int)next_hop)
		{
			while (queue_empty(restore) == 0)
			{
				queue_enq(cache, (struct arp_entry *)queue_deq(restore));
			}
			return temp;
		}
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	packet m;
	int rc;

	init(argc - 2, argv + 2);
	struct queue *packet_wait_to_send = queue_create();
	struct queue *cache = queue_create();

	struct route_table_entry *rtable01 = (struct route_table_entry *)malloc(sizeof(struct route_table_entry) * 64273);
	struct route_table_entry *rtable02 = (struct route_table_entry *)malloc(sizeof(struct route_table_entry) * 64275);
	read_rtable("rtable0.txt", rtable01);
	read_rtable("rtable1.txt", rtable02);

	while (1)
	{
		rc = get_packet(&m);
		DIE(rc < 0, "get_packet");

		struct ether_header *eth = (struct ether_header *)m.payload;
		struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct ether_header));
		struct icmphdr *icmp_hdr = (struct icmphdr *)(m.payload + sizeof(struct ether_header) + sizeof(struct iphdr));

		// 2.1 : Verificarea pachete proprii
		if (ntohs(eth->ether_type) == ETHERTYPE_IP && ip_hdr->version == 4)
		{
			// 2.2 : Verificare checksum
			if (ip_checksum((void *)ip_hdr, sizeof(struct iphdr)) != 0)
				continue;

			// 2.3 : Verificare si actualizare TTL
			if (ip_hdr->ttl == (uint8_t)0 || ip_hdr->ttl == (uint8_t)1)
			{
				memcpy(eth->ether_dhost, eth->ether_shost, ETH_ALEN);
				get_interface_mac(m.interface, eth->ether_shost);

				uint32_t swap = ip_hdr->daddr;
				ip_hdr->daddr = ip_hdr->saddr;
				ip_hdr->saddr = swap;

				icmp_hdr->type = ICMP_TIME_EXCEEDED;
				icmp_hdr->code = (uint8_t)0;
				icmp_hdr->checksum = icmp_checksum((void *)icmp_hdr, sizeof(struct icmphdr));

				send_packet(&m);
				continue;
			}
			ip_hdr->ttl--;

			// 2.4 : Cautare în tabela de rutare
			struct route_table_entry *best_route = find_in_routing_table(rtable01, rtable02, ip_hdr->daddr);
			if (best_route == NULL)
			{
				memcpy(eth->ether_dhost, eth->ether_shost, ETH_ALEN);
				get_interface_mac(m.interface, eth->ether_shost);

				uint32_t swap = ip_hdr->daddr;
				ip_hdr->daddr = ip_hdr->saddr;
				ip_hdr->saddr = swap;

				icmp_hdr->type = ICMP_DEST_UNREACH;
				icmp_hdr->code = (uint8_t)0;
				icmp_hdr->checksum = icmp_checksum((void *)icmp_hdr, sizeof(struct icmphdr));

				send_packet(&m);
				continue;
			}

			// 2.5 : Actualizare checksum
			ip_hdr->check = 0;
			ip_hdr->check = ip_checksum((void *)ip_hdr, sizeof(struct iphdr));

			// 2.6 : Rescriere adrese L2
			// protocol ARP
			// 3.1 : Cautare în cache
			struct arp_entry *arp_mac_found = find_in_cache(cache, best_route->next_hop);
			if (arp_mac_found != NULL)
			{
				memcpy(eth->ether_shost, eth->ether_dhost, ETH_ALEN);
				ip_hdr->saddr = ip_hdr->daddr;
				memcpy(eth->ether_dhost, arp_mac_found->mac, ETH_ALEN);
				ip_hdr->daddr = htonl((uint32_t)arp_mac_found->ip);
				send_packet(&m);
			}
			else
			{
				// adaugat într-o coada
				memcpy(eth->ether_shost, eth->ether_dhost, ETH_ALEN);
				ip_hdr->saddr = ip_hdr->daddr;
				ip_hdr->daddr = htonl(best_route->next_hop);
				queue_enq(packet_wait_to_send, (packet *)&m);

				// Generare ARP request
				packet arp_request;
				struct ether_header *eth_arp = (struct ether_header *)arp_request.payload;
				struct arp_header *ARP_header = (struct arp_header *)(arp_request.payload + sizeof(struct ether_header));

				// Antetul Ethernet
				eth_arp->ether_type = htons(ETHERTYPE_ARP);
				get_interface_mac(best_route->interface, eth_arp->ether_shost);
				for (size_t i = 0; i < ETH_ALEN; i++)
				{
					eth_arp->ether_dhost[i] = 0xFF;
				}

				// Antetul ARP
				ARP_header->htype = htons(ARPHRD_ETHER);
				ARP_header->ptype = htons(2048);
				ARP_header->op = htons(ARPOP_REQUEST);
				ARP_header->hlen = (uint8_t)6;
				ARP_header->plen = (uint8_t)4;
				memcpy(ARP_header->sha, eth_arp->ether_shost, 6);
				memcpy(ARP_header->tha, eth_arp->ether_dhost, 6);
				ARP_header->spa = best_route->next_hop;
				ARP_header->tpa = inet_addr(get_interface_ip(best_route->interface));

				arp_request.len = sizeof(struct ether_header) + sizeof(struct arp_header);
				arp_request.interface = best_route->interface;

				send_packet(&arp_request);
			}
		}
		else if (ntohs(eth->ether_type) == ETHERTYPE_ARP)
		{
			struct arp_header *ARP_header = (struct arp_header *)(m.payload + sizeof(struct ether_header));
			if (ntohs(ARP_header->op) == ARPOP_REPLY)
			{
				struct arp_entry *arp_current = (struct arp_entry *)malloc(sizeof(struct arp_entry));
				arp_current->ip = (unsigned int)ARP_header->spa;
				memcpy(arp_current->mac, ARP_header->sha, ETH_ALEN);
				queue_enq(cache, arp_current);

				struct queue *restore = queue_create();

				while (queue_empty(packet_wait_to_send) == 0)
				{
					packet *temp = (packet *)queue_deq(packet_wait_to_send);
					struct iphdr *ip_hdr_current = (struct iphdr *)(temp->payload + sizeof(struct ether_header));
					struct ether_header *eth_current = (struct ether_header *)temp->payload;
					if (ip_hdr_current->daddr == htonl((uint32_t)arp_current->ip))
					{
						memcpy(eth_current->ether_dhost, arp_current->mac, ETH_ALEN);
						send_packet(temp);
					}
					else
					{
						queue_enq(restore, (packet *)temp);
					}
				}
				while (queue_empty(restore) == 0)
				{
					queue_enq(packet_wait_to_send, (packet *)queue_deq(restore));
				}
				continue;
			}
		}
	}
}