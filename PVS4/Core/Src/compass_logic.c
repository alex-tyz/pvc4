#include "compass_logic.h"
#include "compass_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

void Compass_Compute(CompassReading *r, const int32_t raw[3], CompassScales scales)
{
  r->scales = scales;
  r->x = raw[0] * scales.k[0] / 100;
  r->y = raw[1] * scales.k[1] / 100;
  r->z = raw[2] * scales.k[2] / 100;
  float right = COMPASS_SIGN_X * (float)(COMPASS_SWAP_XY ? r->y : r->x);
  float up = COMPASS_SIGN_Y * (float)(COMPASS_SWAP_XY ? r->x : r->y);
  float length = sqrtf(right * right + up * up);
  r->right = r->up = 0;
  r->direction = -1;
  if (!r->valid || length < 1.0f) return;
  r->right = right / length;
  r->up = up / length;
  r->direction = ((int32_t)lroundf(atan2f(right, up) * 57.2957795f) + 360) % 360;
}

CompassCommand Compass_Parse(const char *input, CompassScales *scales)
{
  char text[49];
  size_t n = strlen(input);
  if (n > 48) return CMD_SYNTAX;
  for (size_t i=0; i<=n; ++i) text[i]=(char)toupper((unsigned char)input[i]);
  if (!n) return CMD_EMPTY;
  if (!strcmp(text,"PING")) return CMD_PING;
  if (!strcmp(text,"GET")) return CMD_GET;
  if (!strcmp(text,"STATS")) return CMD_STATS;
  if (strncmp(text,"SCALE ",6)) return CMD_UNKNOWN;
  CompassScales next;
  const char *p=text+6;
  bool range=false;
  for (unsigned i=0; i<3; ++i) {
    while (*p==' ') ++p;
    if (!(*p=='+' || *p=='-' || (*p>='0' && *p<='9'))) return CMD_SYNTAX;
    char *end;
    errno=0;
    long value=strtol(p,&end,10);
    if (p==end || (*end && *end!=' ')) return CMD_SYNTAX;
    if (errno==ERANGE || value<0 || value>200) range=true;
    next.k[i]=(value>=0 && value<=200) ? (int32_t)value : 0;
    p=end;
  }
  while (*p==' ') ++p;
  if (*p) return CMD_SYNTAX;
  if (range) return CMD_RANGE;
  *scales=next;  /* All three coefficients change together. */
  return CMD_SCALE;
}

bool Compass_Feed(CompassLine *line, uint8_t byte, CompassCommand *command, CompassScales *scales)
{
  if (byte=='\r' || byte=='\n') {
    line->text[line->used]=0;
    *command=line->discard ? CMD_SYNTAX : Compass_Parse(line->text,scales);
    line->used=0; line->discard=false;
    return *command!=CMD_EMPTY;
  }
  if (byte<32 || byte>126 || line->used>=48) line->discard=true;
  if (!line->discard) line->text[line->used++]=(char)byte;
  return false;
}

void Compass_Format(const CompassReading *r, char *text, size_t size)
{
  if (!r->valid) { snprintf(text,size,"ERR SENSOR"); return; }
  snprintf(text,size,"DATA %ld %ld %ld %ld %ld %ld %ld",(long)r->x,(long)r->y,
    (long)r->z,(long)r->direction,(long)r->scales.k[0],(long)r->scales.k[1],(long)r->scales.k[2]);
}
