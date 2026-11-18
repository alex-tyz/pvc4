#include "compass_logic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

int main(void) {
  CompassScales scales={{100,100,100}}, old=scales;
  assert(Compass_Parse("scale 0 100 200",&scales)==CMD_SCALE);
  assert(scales.k[0]==0 && scales.k[1]==100 && scales.k[2]==200);
  scales=old;
  const char *bad[]={"SCALE -1 100 100","SCALE 100 201 100","SCALE 100 100 999999999999999999999999999"};
  for (unsigned i=0; i<sizeof(bad)/sizeof(*bad); ++i) {
    assert(Compass_Parse(bad[i],&scales)==CMD_RANGE);
    assert(!memcmp(&scales,&old,sizeof(old)));
  }
  const char *syntax[]={"SCALE 1 2","SCALE 1 2 3 4","SCALE 1.5 2 3","SCALE + 1 2","SCALE 1\t2 3"};
  for (unsigned i=0; i<sizeof(syntax)/sizeof(*syntax); ++i) {
    assert(Compass_Parse(syntax[i],&scales)==CMD_SYNTAX);
    assert(!memcmp(&scales,&old,sizeof(old)));
  }
  assert(Compass_Parse("PING",&scales)==CMD_PING);
  assert(Compass_Parse("GET",&scales)==CMD_GET);
  assert(Compass_Parse("STATS",&scales)==CMD_STATS);
  assert(Compass_Parse("OTHER",&scales)==CMD_UNKNOWN);
  CompassLine line={0}; CompassCommand cmd=CMD_EMPTY;
  const char *packet="SCALE 100 0 200\r\n";
  int events=0; CompassCommand last=CMD_EMPTY;
  for (const char *p=packet; *p; ++p) if (Compass_Feed(&line,(uint8_t)*p,&cmd,&scales)) { ++events; last=cmd; }
  assert(events==1 && last==CMD_SCALE && scales.k[1]==0);
  for (unsigned i=0; i<60; ++i) assert(!Compass_Feed(&line,'A',&cmd,&scales));
  assert(Compass_Feed(&line,'\n',&cmd,&scales) && cmd==CMD_SYNTAX);
  Compass_Feed(&line,'S',&cmd,&scales); Compass_Feed(&line,0,&cmd,&scales);
  assert(Compass_Feed(&line,'\n',&cmd,&scales) && cmd==CMD_SYNTAX);
  CompassReading r={.valid=true};
  const int32_t axes[][3]={{0,100,12},{100,0,12},{0,-100,12},{-100,0,12}};
  for (unsigned i=0; i<4; ++i) {
    Compass_Compute(&r,axes[i],old);
    assert(r.direction==(int32_t)i*90);
    assert(fabsf(r.right*r.right+r.up*r.up-1.0f)<0.001f);
  }
  int32_t raw[]={-101,203,-45}; scales=(CompassScales){{200,0,50}};
  Compass_Compute(&r,raw,scales);
  assert(r.x==-202 && r.y==0 && r.z==-22 && r.direction==270);
  scales=(CompassScales){{0,0,0}};
  Compass_Compute(&r,raw,scales); assert(r.direction==-1);
  char output[96]; Compass_Format(&r,output,sizeof(output));
  assert(!strcmp(output,"DATA 0 0 0 -1 0 0 0"));
  r.valid=false; Compass_Format(&r,output,sizeof(output)); assert(!strcmp(output,"ERR SENSOR"));
  puts("Compass C tests passed");
  return 0;
}
