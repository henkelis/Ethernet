// AUTOIP Library v0.3
// Author:

#include <Arduino.h>
#include "Ethernet.h"
#include "AutoIP.h"
#include "utility/w5100.h"
#include <arduino-timer.h>

/**
 * Pseudo random macro based on mac.
 */
#define AUTOIP_RAND(mac) ( (((u32_t)((mac[5]) & 0xff) << 24) | \
                            ((u32_t)((mac[3]) & 0xff) << 16) | \
                            ((u32_t)((mac[2]) & 0xff) << 8) | \
                            ((u32_t)((mac[4]) & 0xff))) + \
                            (_tried_llipaddr))

/**
 * Macro that generates the initial IP address to be tried by AUTOIP.
 */
#define AUTOIP_CREATE_SEED_ADDR(mac) \
    AUTOIP_RANGE_START + ((u32_t)(((u8_t)(mac[4])) | \
                          ((u32_t)((u8_t)(mac[5]))) << 8))

#define MAX_TIMEOUT  0x7fffffff

int AutoIPClass::beginWithAUTOIP(uint8_t *mac, IPAddress ip, unsigned long timeout, unsigned long responseTimeout)
{
	_timeout = timeout;
	_responseTimeout = responseTimeout;
	_autoip_state = AUTOIP_STATE_OFF;
    _ttw = 0;
    _sent_num = 0;
    _lastconflict = 0;
    _tried_llipaddr = 0;
	_autoipSeedAddress = ip;

	// zero out _autoipMacAddr
	memset(_autoipMacAddr, 0, 6);
	memcpy((void*)_autoipMacAddr, (void*)mac, 6);
	// get an address to try
    int ret = create_AUTOIP_address();
    if (ret == 0) {
        return 0;
    }
    uint8_t  _SubnetMask[4] = {255, 255, 0, 0};
    uint8_t  _GatewayIp[4] = {0, 0, 0, 0};
    uint8_t  _DnsServerIp[4] = {0, 0, 0, 0};
    memcpy(_autoipSubnetMask, _SubnetMask, 4);
    memcpy(_autoipGatewayIp, _GatewayIp, 4);
    memcpy(_autoipDnsServerIp, _DnsServerIp, 4);

 	return request_AUTOIP_address();
}

// return:0 on error, 1 if address created
int AutoIPClass::create_AUTOIP_address()
{
    u32_t addr;
	// if an address is already set, use that as the seed
	if (_autoipSeedAddress == IPAddress(0, 0, 0, 0)) {
		/* Here we create an IP-Address out of range 169.254.1.0 to 169.254.254.255
		* compliant to RFC 3927 Section 2.1
		* We have 254 * 256 possibilities */
		addr = AUTOIP_CREATE_SEED_ADDR(_autoipMacAddr);
	} else {
		/* Hosts that are equipped with persistent storage MAY, for each
		* interface, record the IPv4 address they have selected.  On booting,
		* hosts with a previously recorded address SHOULD use that address as
    	* their first candidate when probing.  This increases the stability of
    	* addresses.  For example, if a group of hosts are powered off at
    	* night, then when they are powered on the next morning they will all
		* resume using the same addresses, instead of picking different
		* addresses and potentially having to resolve conflicts that arise */
		addr = htonl(uint32_t(_autoipSeedAddress));
	}
	addr += _tried_llipaddr;
    addr = AUTOIP_NET | (addr & 0xffff);
    /* Now, 169.254.0.0 <= addr <= 169.254.255.255 */

    if (addr < AUTOIP_RANGE_START) {
        addr += AUTOIP_RANGE_END - AUTOIP_RANGE_START + 1;
    }
    if (addr > AUTOIP_RANGE_END) {
        addr -= AUTOIP_RANGE_END - AUTOIP_RANGE_START + 1;
    }
    if ((addr < AUTOIP_RANGE_START) || (addr > AUTOIP_RANGE_END)) {
        return 0;
    }
    uint32_t ip = htonl(addr);
    memcpy(_autoipLocalIp, &ip, 4);

    return 1;
}

void AutoIPClass::autoip_start_probing()
{
  _autoip_state = AUTOIP_STATE_PROBING;
  _sent_num = 0;

  /* time to wait to first probe, this is randomly
   * chosen out of 0 to PROBE_WAIT seconds.
   * compliant to RFC 3927 Section 2.2.1
   */
  _ttw = (u16_t)(AUTOIP_RAND(_autoipMacAddr) % (PROBE_WAIT * AUTOIP_TICKS_PER_SECOND));

  /*
   * if we tried more then MAX_CONFLICTS we must limit our rate for
   * acquiring and probing address
   * compliant to RFC 3927 Section 2.2.1
   */
  if (_tried_llipaddr > MAX_CONFLICTS) {
    _ttw = RATE_LIMIT_INTERVAL * AUTOIP_TICKS_PER_SECOND;
  }
}

// return:0 on error, 1 if link-local address is found
int AutoIPClass::request_AUTOIP_address()
{
	uint8_t messageType = 0;

	_autoipUdpSocket.stop();

	if (_autoipUdpSocket.beginRaw(AUTOIP_PORT) == 0) {
		// Couldn't get a socket
		Serial.printf("Oops, couldn't get the raw socket!\n");
		return 0;
	}

	presend_AUTOIP();

	int result = 0;
	unsigned long startTime = millis();

	autoip_start_probing();

	autoip_timer_start();

	while (true) {
		_timer.tick();
		if (_autoip_state == AUTOIP_STATE_BOUND) {
			result = 1;
//			Serial.printf("AUTOIP complete\n");
			break;
		}
		if (result != 1 && ((millis() - startTime) > _timeout)) {
//			Serial.printf("AUTOIP timeout\n");
			break;
		}
	}

	autoip_timer_stop();

	// We're done with the socket now
	_autoipUdpSocket.stop();

	return result;
}

void AutoIPClass::autoip_tmr()
{
	uint8_t messageType = 0;

	if (_lastconflict > 0) {
		_lastconflict--;
	}

	if (_ttw > 0) {
		_ttw--;
	}

	switch (_autoip_state) {
	case AUTOIP_STATE_PROBING:
		if (_ttw == 0) {
			if (_sent_num >= PROBE_NUM) {
				/* Switch to ANNOUNCING: now we can bind to an IP address and use it */
				_autoip_state = AUTOIP_STATE_ANNOUNCING;
				_sent_num = 0;
				_ttw = ANNOUNCE_WAIT * AUTOIP_TICKS_PER_SECOND;
			} else {
				autoip_arp_probe();

				messageType = parseAUTOIPResponse(ARP_REPLY, _responseTimeout);
/*
				if (messageType == 0) {
					Serial.printf("got ARP, not probe response\n");
				} else if (messageType == 1) {
					Serial.printf("got probe response - IP in use\n");
				} else if (messageType == 255) {
					Serial.printf("probe response timed out\n");
				}
*/
				if (messageType == 1 || messageType == 255) {
					// set up to send another probe
					if (messageType == 1) {

						/* RFC3927, 2.5 "Conflict Detection and Defense" allows two options where
							a) means retreat on the first conflict and
							b) allows to keep an already configured address when having only one
								conflict in 10 seconds
							We use option a), as we're establishing an address and don't have
							any active connections
							(note that lwIP uses option b)) */

						/* ported lwIP code for option b) is below:
						// TODO: add flag to enable/disable this
						if (_lastconflict > 0) {
							// retreat, there was a conflicting ARP in the last DEFEND_INTERVAL seconds
							Serial.printf("ARP conflict: we are defending, but in DEFEND_INTERVAL, retreating\n"));
							// Active TCP sessions are aborted when removing the ip addresss
							// fall through, a new address will be assigned and another probe sent
						} else {
							Serial.printf("ARP conflict: we are defending, send ARP Announce\n"));
							_lastconflict = DEFEND_INTERVAL * AUTOIP_TICKS_PER_SECOND;
							_autoip_state = AUTOIP_STATE_ANNOUNCING;
							_sent_num = 0;
							_ttw = ANNOUNCE_WAIT * AUTOIP_TICKS_PER_SECOND;
							// exit case, will send an announcement
							break;
						}
						*/

						// restart probe count
						_sent_num = 0;
						// get another IP address to try
						_tried_llipaddr++;
						create_AUTOIP_address();
					} else {
						_sent_num++;
					}
					if (_sent_num == PROBE_NUM) {
						/* calculate time to wait to announce */
						_ttw = ANNOUNCE_WAIT * AUTOIP_TICKS_PER_SECOND;
					} else {
						/* calculate time to wait to next probe */
						_ttw = (u16_t)((AUTOIP_RAND(_autoipMacAddr) %
									((PROBE_MAX - PROBE_MIN) * AUTOIP_TICKS_PER_SECOND) ) +
									PROBE_MIN * AUTOIP_TICKS_PER_SECOND);
					}
				}
			}
		}
		break;

	case AUTOIP_STATE_ANNOUNCING:
		if (_ttw == 0) {
			autoip_arp_announce();
			_ttw = ANNOUNCE_INTERVAL * AUTOIP_TICKS_PER_SECOND;
			_sent_num++;

			if (_sent_num >= ANNOUNCE_NUM) {
				_autoip_state = AUTOIP_STATE_BOUND;
				_sent_num = 0;
				_ttw = 0;
			}
		}
		break;

	default:
		/* nothing to do in other states */
		break;
	}

}

void AutoIPClass::presend_AUTOIP()
{
}

int AutoIPClass::autoip_arp_probe()
{
//	Serial.printf("autoip_arp_probe\n");
	send_AUTOIP_MESSAGE(ARP_REQUEST, ARP_PROBE);
	return 1;
}

int AutoIPClass::autoip_arp_announce()
{
//	Serial.printf("autoip_arp_announce\n");
	send_AUTOIP_MESSAGE(ARP_REQUEST, ARP_ANNOUNCE);
	return 1;
}

void AutoIPClass::send_AUTOIP_MESSAGE(uint8_t messageType, uint8_t requestType)
{
	IPAddress dest_addr(255, 255, 255, 255); // Broadcast address

    uint8_t  destMacAddr[6];
	memset(destMacAddr, 255, 6);

	if (_autoipUdpSocket.beginPacket(dest_addr, AUTOIP_PORT) == -1) {
		Serial.printf("AUTOIP transmit error\n");
		// FIXME Need to return errors
		return;
	}

	// we will send MACRAW, so need to add an ethernet header

	struct eth_hdr ethhdr;
	memset(&ethhdr, 0, sizeof(struct eth_hdr));

	memcpy(&ethhdr.src,  _autoipMacAddr, ETH_HWADDR_LEN);
	memcpy(&ethhdr.dest, destMacAddr, ETH_HWADDR_LEN);
	ethhdr.type = htons(ETHTYPE_ARP);

	//put data in W5100 transmit buffer
	_autoipUdpSocket.write((uint8_t *)&ethhdr, sizeof(struct eth_hdr));

	// now add an ARP probe header

	struct etharp_hdr etharphdr;
	memset(&etharphdr, 0, sizeof(struct etharp_hdr));

	etharphdr.hwtype = htons(IANA_HWTYPE_ETHERNET);
	etharphdr.proto = htons(ETHTYPE_IP);
	etharphdr.hwlen = ETH_HWADDR_LEN;
	etharphdr.protolen = sizeof(u32_t);
	etharphdr.opcode = htons(messageType);
	memcpy(&(etharphdr.shwaddr), _autoipMacAddr, ETH_HWADDR_LEN);
	if (requestType == ARP_PROBE) {
		uint8_t  sourceIp[4];
		memset(sourceIp, 0, 4);
		memcpy(&(etharphdr.sipaddr), sourceIp, sizeof(u32_t));
	} else { // ARP_ANNOUNCE
		memcpy(&(etharphdr.sipaddr), _autoipLocalIp, sizeof(u32_t));
	}
	memcpy(&(etharphdr.dhwaddr), destMacAddr, ETH_HWADDR_LEN);
	memcpy(&(etharphdr.dipaddr), _autoipLocalIp, sizeof(u32_t));

/*
	printf("hwtype: %u\n", etharphdr.hwtype);
	printf("proto: %u\n", etharphdr.proto);
	printf("hwlen: %u\n", etharphdr.hwlen);
	printf("protolen: %u\n", etharphdr.protolen);
	printf("opcode: %u\n", etharphdr.opcode);
    printf("shwaddr: %6x\n", etharphdr.shwaddr);
    printf("dhwaddr: %6x\n", etharphdr.dhwaddr);
    printf("sipaddr: %4x\n", etharphdr.sipaddr);
    printf("dipaddr: %4x\n", etharphdr.dipaddr);
	printf("size: %u\n", sizeof(struct etharp_hdr));
*/

/* NOT NEEDED, ALTERNATE buffer CREATION
	uint8_t buffer[28];
	memset(buffer, 0, 28);

  	u16_t u16 = etharphdr.hwtype;
	memcpy(buffer + 0, &(u16), 2);
  	u16 = etharphdr.proto;
	memcpy(buffer + 2, &(u16), 2);
	buffer[4] = etharphdr.hwlen;
	buffer[5] = etharphdr.protolen;
  	u16 = etharphdr.opcode;
	memcpy(buffer + 6, &(u16), 2);
	memcpy(buffer + 8, _autoipMacAddr, 6);
	memcpy(buffer + 14, sourceIp, 4);
	memcpy(buffer + 18, destMacAddr, 6);
	memcpy(buffer + 24, _autoipLocalIp, 4);
*/

	//put data in W5100 transmit buffer
	_autoipUdpSocket.write((uint8_t *)&etharphdr, sizeof(struct etharp_hdr));

	_autoipUdpSocket.endPacket();
}

uint8_t AutoIPClass::parseAUTOIPResponse(uint8_t messageType, unsigned long responseTimeout)
{
	u16_t u16;
	unsigned long startTime = millis();
	int ret = 0;

	uint16_t available;
	while ((available = _autoipUdpSocket.parsePacketRaw()) <= 0) {
		if ((millis() - startTime) > responseTimeout) {
			return 255;
		}
		delay(50);
	}

	// start reading in the packet

	// as the socket is raw, the first 2 bytes are the data length (not included in the available length)
	uint8_t temp[2];
	ret = _autoipUdpSocket.read(temp, 2);
	memcpy(&(u16), temp, 2);
	u16_t length = htons(u16);

	if (length >= (sizeof(struct eth_hdr) + sizeof(struct etharp_hdr))) {

		uint8_t ethhdr[sizeof(struct eth_hdr)];
		memset(ethhdr, 0, sizeof(struct eth_hdr));

		ret = _autoipUdpSocket.read(ethhdr, sizeof(struct eth_hdr));
		length -= ret;

		memcpy(&(u16), ethhdr + 12, 2);

		if ((memcmp(ethhdr, _autoipMacAddr, ETH_HWADDR_LEN) == 0) &&
			(u16 == htons(ETHTYPE_ARP))) {
//			Serial.printf("is ARP for us\n");

			uint8_t etharphdr[sizeof(struct etharp_hdr)];
			memset(etharphdr, 0, sizeof(struct etharp_hdr));

			ret = _autoipUdpSocket.read(etharphdr, sizeof(struct etharp_hdr));
			length -= ret;

			memcpy(&(u16), etharphdr + 6, 2);
			if (u16 == htons(messageType)) {
//				Serial.printf("is ARP reply\n");
				ret = 1;
			}
		}
	}

	// discard any remaining data
	_autoipUdpSocket.read((uint8_t *)NULL, length);

	// Need to skip to end of the packet regardless here
	_autoipUdpSocket.flush(); // FIXME

	return ret;
}

IPAddress AutoIPClass::getLocalIp()
{
	return IPAddress(_autoipLocalIp);
}

IPAddress AutoIPClass::getSubnetMask()
{
	return IPAddress(_autoipSubnetMask);
}

IPAddress AutoIPClass::getGatewayIp()
{
	return IPAddress(_autoipGatewayIp);
}

IPAddress AutoIPClass::getDnsServerIp()
{
	return IPAddress(_autoipDnsServerIp);
}

void AutoIPClass::printByte(char * buf, uint8_t n )
{
	char *str = &buf[1];
	buf[0]='0';
	do {
		unsigned long m = n;
		n /= 16;
		char c = m - 16 * n;
		*str-- = c < 10 ? c + '0' : c + 'A' - 10;
	} while(n);
}

/*
 * timer stuff
 *
 * NOTE - assumes arduino-timer is installed
 *
 */

bool AutoIPClass::autoip_tmr_wrapper(void* context)
{
	reinterpret_cast<AutoIPClass*>(context)->autoip_tmr();
	return true;
}

void AutoIPClass::autoip_timer_start()
{
	_timer.every(AUTOIP_TMR_INTERVAL, autoip_tmr_wrapper, this);
}

void AutoIPClass::autoip_timer_stop()
{
	_timer.cancel();
}
