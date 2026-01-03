/*
** Copyright 2010 Logitech. All Rights Reserved.
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/if.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


// search first 4 interfaces returned by IFCONF - same method used by squeezelite
char *platform_get_mac_address() {
    struct ifconf ifc;
    struct ifreq *ifr, *ifend;
    struct ifreq ifreq;
    struct ifreq ifs[4];
    char *utmac;
    uint8_t mac[6];

    mac[0] = mac[1] = mac[2] = mac[3] = mac[4] = mac[5] = 0;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
 
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;

    if (ioctl(s, SIOCGIFCONF, &ifc) == 0) {
		ifend = ifs + (ifc.ifc_len / sizeof(struct ifreq));

		for (ifr = ifc.ifc_req; ifr < ifend; ifr++) {
			if (ifr->ifr_addr.sa_family == AF_INET) {

				strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
				if (ioctl (s, SIOCGIFHWADDR, &ifreq) == 0) {
					memcpy(mac, ifreq.ifr_hwaddr.sa_data, 6);
					if (mac[0]+mac[1]+mac[2] != 0) {
						break;
					}
				}
			}
		}
	}

    close(s);

    char *macaddr = malloc(18);

    utmac = getenv("UTMAC");
    if (utmac)
    {
        if ( strlen(utmac) == 17 )
        {
            sscanf(utmac,"%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
        }
    }

    sprintf(macaddr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return macaddr;
}

