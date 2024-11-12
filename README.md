
Updated version of Arduino Ethernet library that adds support for AutoIP

The AutoIP code was ported from lwIP

Requires arduino-timer to be installed

Optionally accepts a pre-saved seed IP

Uses socket 0 on W5500 opened MACRAW to be able to send and receive ARPs

Doesn't send a gratuitous ARP announce, but does send 2 announcements as per the spec

Some of the AutoIP timeouts have been reduced


