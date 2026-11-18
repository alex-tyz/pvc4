#ifndef COMPASS_LOGIC_H
#define COMPASS_LOGIC_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t k[3]; } CompassScales;
typedef struct {
  int32_t x, y, z, direction;
  float right, up;
  bool valid;
  CompassScales scales;
  char name[8];
  uint32_t sampled_ms;
} CompassReading;
typedef enum { CMD_EMPTY, CMD_PING, CMD_GET, CMD_SCALE, CMD_STATS,
               CMD_SYNTAX, CMD_RANGE, CMD_UNKNOWN } CompassCommand;
typedef struct { char text[49]; uint8_t used; bool discard; } CompassLine;
void Compass_Compute(CompassReading *r, const int32_t raw[3], CompassScales scales);
CompassCommand Compass_Parse(const char *line, CompassScales *scales);
bool Compass_Feed(CompassLine *line, uint8_t byte, CompassCommand *command, CompassScales *scales);
void Compass_Format(const CompassReading *r, char *text, size_t size);
#endif
