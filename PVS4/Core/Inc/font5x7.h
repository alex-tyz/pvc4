#ifndef __FONT5X7_H__
#define __FONT5X7_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FONT5X7_WIDTH   5U
#define FONT5X7_HEIGHT  7U
#define FONT5X7_FIRST   0x20U
#define FONT5X7_LAST    0x7EU

extern const uint8_t Font5x7[FONT5X7_LAST - FONT5X7_FIRST + 1U][FONT5X7_WIDTH];

#ifdef __cplusplus
}
#endif

#endif
