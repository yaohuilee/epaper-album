#pragma once

/* Open provisioning hotspot: 192.168.7.0/24. */
#define EPAPER_PROVISION_IP "192.168.7.60"
#define EPAPER_PROVISION_NETMASK "255.255.255.0"
#define EPAPER_PROVISION_IP_OCTETS 192,168,7,60
#define EPAPER_PROVISION_URL "http://" EPAPER_PROVISION_IP "/setup"
#define EPAPER_PROVISION_CAPTIVE_URI "http://" EPAPER_PROVISION_IP "/api/captive-portal"
