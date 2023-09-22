/* File to connect _outbyte and _inbyte functions with an sDDF serial driver */

#pragma once

/* Send the raw byte over the serial port */
void _outbyte(int c);

/* Transmit buffer */
void _outbuff(char *buff, unsigned int len);

/* Wait to read a byte within the timeout param. Timeout in msec. */
int _inbyte(unsigned int timeout);


