#pragma once

// Maybe try and expand the num of buffers in the future
#define NUM_BUFFERS 512

#define CLIENT_CH 7

/* CLIENT CONFIG OPTIONS
    Specify how to dump sample buffers
    0 - Print over serial
    1 - Use the xmodem protocol to send over serial
*/
#define CLIENT_CONFIG 1