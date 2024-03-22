#pragma once

#define SERVER_PRINT_CHANNEL 9
#define SERVER_GETCHAR_CHANNEL 11

int serial_server_printf(char *string);
char get_char();
int serial_server_scanf(char* buffer);
void init_serial(void);