"""Production slope correction against 60 Hz plane-contact recurrence.
Synthetic terrain contact; not a live map traversal test. Uses real wrapper and
checks scope guards. Retail-x86 isolated reproduction is recorded separately.
"""
from pathlib import Path
import subprocess,tempfile,runpy
ROOT=Path(__file__).resolve().parents[2]
def main():
 src=ROOT/'ports/mercenaries/src'
 manual=(src/'recomp_manual.c').read_text(encoding='utf-8')
 wrapper=manual[manual.index('void recomp_slope_slide_prepare('):]
 wrapper=wrapper[:wrapper.index('\n}')+2]
 patches=runpy.run_path(str(ROOT/'ports/mercenaries/scripts/Patch-Generated.py'))['PATCHES']
 patch=next(p for p in patches if p.name=='Frame-rate independent direct-input slope sliding')
 generated=''.join(p.read_text(encoding='utf-8') for p in (src/'recomp/gen').glob('recomp_*.c'))
 assert generated.count(patch.after)==1
 code=r"""
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "slope_slide.h"
#define CHECK(c) do{if(!(c)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#c);exit(2);}}while(0)
static unsigned char mem[0x310000];
static ptrdiff_t g_xbox_mem_offset;
#define P 0x10000u
#define D 0x20000u
#define G 0x21000u
#define F(a) (*(float*)(mem+(a)))
"""+wrapper+r"""
static float azimuth;
static void init(float ny) {
 memset(mem,0,sizeof(mem));g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)mem;
 mem[P+0x61]=mem[P+0x63]=mem[P+0xA4]=1;
 F(0x30E8D0)=.64f;F(P+0xCC)=sqrtf(1-ny*ny)*cosf(azimuth);F(P+0xD0)=ny;F(P+0xD4)=sqrtf(1-ny*ny)*sinf(azimuth);
 F(G+4)=-9.8f;
}
static void simulate(int fps,float ny,float input_x,float input_z,int fixed,float result[3]) {
 init(ny);
 float *v=(float*)(mem+P+0xE4),*n=(float*)(mem+P+0xCC),*d=(float*)(mem+D),*g=(float*)(mem+G);
 const float dt=1.0f/fps;float last[3]={0};
 for(int frame=0;frame<fps*30;frame++) {
  d[0]=input_x;d[1]=v[1];d[2]=input_z;g[0]=g[2]=0;g[1]=-9.8f;
  if(fixed)recomp_slope_slide_prepare(P,dt,D,G);
  for(int i=0;i<3;i++)v[i]=d[i]+g[i]*dt*13;
  float dot=0;for(int i=0;i<3;i++)dot+=v[i]*n[i];
  if(dot<0)for(int i=0;i<3;i++)v[i]-=dot*n[i];
  if(frame>=fps*29)for(int i=0;i<3;i++)last[i]+=v[i]*dt;
 }
 memcpy(result,last,sizeof(last));
}
int main(void) {
 int rates[]={20,30,60,90,120,144,240,480};float slopes[]={.15f,.3f,.5f,.63f};unsigned cases=0;
 for(int angle=0;angle<3;angle++) {
 azimuth=angle*.78539816339f;
 for(unsigned s=0;s<4;s++)for(int input=0;input<3;input++) {
  float target[3],actual[3],old[3];float x=input==1?1.5f:0,z=input==2?2:0;
  simulate(60,slopes[s],x,z,0,target);
  for(unsigned r=0;r<8;r++) {
   simulate(rates[r],slopes[s],x,z,1,actual);
   for(int i=0;i<3;i++)CHECK(fabsf(actual[i]-target[i])<.0015f*fmaxf(1,fabsf(target[i])));
   if(rates[r]==60)CHECK(memcmp(actual,target,sizeof(target))==0);
   cases++;
  }
  if(!input){simulate(120,slopes[s],x,z,0,old);int axis=fabsf(target[0])>fabsf(target[2])?0:2;CHECK(fabsf(old[axis]/target[axis]-.5f)<.002f);}
 }
 }
 // Across-slope input is immediate even when a frame spans several 60 Hz ticks.
 azimuth=.78539816339f;
 for(int fps=20;fps<=240;fps*=2) {
  init(.5f);F(P+0xE4)=8;F(P+0xE8)=-10;F(P+0xEC)=3;
  F(D)=2;F(D+4)=-10;F(D+8)=-2;
  recomp_slope_slide_prepare(P,1.f/fps,D,G);
  CHECK(fabsf((F(D)-F(D+8))-4)<.00001f);
 }
 azimuth=0;
 // Guarded branches must be bitwise unchanged.
 for(int guard=0;guard<11;guard++) {
  init(.5f);float dt=1/120.0f;F(P+0xE4)=5;F(D)=0;F(D+4)=-4;
  if(guard==0)mem[P+0xA4]=0;
  if(guard==1)mem[P+0x61]=0;
  if(guard==2)mem[P+0x63]=0;
  if(guard==3)mem[P+0x62]=1;
  if(guard==4)mem[P+0x60]=1;
  if(guard==5)F(P+0xD0)=.8f;
  if(guard==6)F(P+0xC0)=1;
  if(guard==7)dt=1/60.0f;
  if(guard==8)dt=0;
  if(guard==9)dt=NAN;
  if(guard==10)dt=-.1f;
  unsigned char expected[24];memcpy(expected,mem+D,12);memcpy(expected+12,mem+G,12);
  recomp_slope_slide_prepare(P,dt,D,G);
  CHECK(!memcmp(expected,mem+D,12)&&!memcmp(expected+12,mem+G,12));
 }
 printf("PASS: %u slope/rate/input cases, 60 FPS parity, across-slope input, original 120 FPS slowdown, 11 unchanged-state guards\n",cases);
 return 0;
}
"""
 with tempfile.TemporaryDirectory(prefix='slope-native-') as tmp:
  tmp=Path(tmp);c=tmp/'test.c';exe=tmp/'test.exe';c.write_text(code,encoding='utf-8')
  subprocess.run(['C:/MinGW/bin/gcc.exe','-O2','-msse2','-mfpmath=sse','-fno-strict-aliasing','-std=c11','-I'+str(src),str(c),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()
