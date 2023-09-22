#pragma once

/**
 * Transmit a buffer over serial using the xmodem protocol.
 * 
 * @param src Buffer that we want to transmit
 * @param srcsz Size of the buffer we want to transmit
 * 
 * @return -1 cancelled by remote, -2 sync error, -4 transmit error, else len of packet
*/
int xmodemTransmit(unsigned char *src, int srcsz);


/** 
 * Receive a buffer using the xmodem protocol
 * 
 * @param dest Destination we want to save received buffer to
 * @param destsz Size of the destination buffer
 * 
 * @return -1 cancelled by remote, -2 sync error, -3 too many retry error, len on success
*/
int xmodemReceive(unsigned char *dest, int destsz);
