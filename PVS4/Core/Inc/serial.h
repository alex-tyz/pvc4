#ifndef SERIAL_H
#define SERIAL_H
#include <stdbool.h>
#include <stdint.h>
void Serial_Init(void);
void Serial_Start(void);
bool Serial_ReadByte(uint8_t *byte);
bool Serial_RecoverOverflow(void);
void Serial_SendLine(const char *text);
uint32_t Serial_GetRxErrors(void);
#endif
