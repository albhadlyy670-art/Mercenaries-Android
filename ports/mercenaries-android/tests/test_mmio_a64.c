#include "mmio_a64.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("FAIL: %s\n",msg);fails++;} }while(0)
static mmio_a64_t D(uint32_t w,int *ok){ mmio_a64_t d; memset(&d,0,sizeof d); *ok=mmio_a64_decode(w,&d); return d; }
int main(void){
  int ok; mmio_a64_t d;
  /* encodings taken from the ARM ARM / real disassembly */
  d=D(0xB9000001,&ok); CHECK(ok&&d.is_store&&d.size==4&&d.rt==1&&d.rn==0&&!d.writeback,"str w1,[x0]");
  d=D(0xB9400001,&ok); CHECK(ok&&!d.is_store&&d.size==4&&!d.dest64,"ldr w1,[x0]");
  d=D(0xF9400001,&ok); CHECK(ok&&!d.is_store&&d.size==8&&d.dest64,"ldr x1,[x0]");
  d=D(0x39000001,&ok); CHECK(ok&&d.is_store&&d.size==1,"strb");
  d=D(0x39400001,&ok); CHECK(ok&&!d.is_store&&d.size==1&&!d.sign_extend,"ldrb");
  d=D(0x79000001,&ok); CHECK(ok&&d.is_store&&d.size==2,"strh");
  d=D(0x79400001,&ok); CHECK(ok&&!d.is_store&&d.size==2,"ldrh");
  d=D(0x39C00001,&ok); CHECK(ok&&d.sign_extend&&!d.dest64&&d.size==1,"ldrsb w1");
  d=D(0x39800001,&ok); CHECK(ok&&d.sign_extend&&d.dest64&&d.size==1,"ldrsb x1");
  d=D(0x79800001,&ok); CHECK(ok&&d.sign_extend&&d.dest64&&d.size==2,"ldrsh x1");
  d=D(0xB9800001,&ok); CHECK(ok&&d.sign_extend&&d.dest64&&d.size==4,"ldrsw x1");
  d=D(0xB9000401,&ok); CHECK(ok&&d.is_store&&d.size==4,"str w1,[x0,#4]");
  d=D(0xB8226801,&ok); CHECK(ok&&d.is_store&&d.size==4&&d.rn==0&&d.rt==1&&!d.writeback,"str w1,[x0,x2]");
  d=D(0xB81FC001,&ok); CHECK(ok&&d.is_store&&!d.writeback,"stur w1,[x0,#-4]");
  d=D(0xB8004C01,&ok); CHECK(ok&&d.is_store&&d.writeback&&d.wb_delta==4,"str w1,[x0,#4]!");
  d=D(0xB8404401,&ok); CHECK(ok&&!d.is_store&&d.writeback&&d.wb_delta==4,"ldr w1,[x0],#4");
  d=D(0xB85FCC01,&ok); CHECK(ok&&!d.is_store&&d.writeback&&d.wb_delta==-4,"ldr w1,[x0,#-4]!");
  d=D(0xB900001F,&ok); CHECK(ok&&d.is_store&&d.rt==31,"str wzr,[x0]");
  /* must be rejected */
  D(0xBD400001,&ok); CHECK(!ok,"ldr s1,[x0] (SIMD) rejected");
  D(0xA9400401,&ok); CHECK(!ok,"ldp rejected");
  D(0xF9800001,&ok); CHECK(!ok,"prfm rejected");
  D(0xD503201F,&ok); CHECK(!ok,"nop rejected");
  D(0x8B020001,&ok); CHECK(!ok,"add rejected");
  /* semantics */
  uint64_t x[31]; uint64_t sp=0x1000; memset(x,0xAA,sizeof x);
  d=D(0x39C00001,&ok); mmio_a64_finish(x,&sp,&d,0x80); CHECK(x[1]==0xFFFFFF80ull,"ldrsb w1 sign-extends to 32 only");
  d=D(0x39800001,&ok); mmio_a64_finish(x,&sp,&d,0x80); CHECK(x[1]==0xFFFFFFFFFFFFFF80ull,"ldrsb x1");
  d=D(0xB9400001,&ok); x[1]=~0ull; mmio_a64_finish(x,&sp,&d,0x12345678); CHECK(x[1]==0x12345678ull,"ldr w zero-extends upper half");
  d=D(0x39400001,&ok); x[1]=~0ull; mmio_a64_finish(x,&sp,&d,0x1FF); CHECK(x[1]==0xFFull,"ldrb masks");
  d=D(0xB8404401,&ok); x[0]=0x2000; mmio_a64_finish(x,&sp,&d,0); CHECK(x[0]==0x2004,"post-index writeback");
  d=D(0xB8004C01,&ok); x[0]=0x2000; x[1]=0xDEADBEEFCAFEF00Dull; CHECK(mmio_a64_store_value(x,&d)==0xCAFEF00Dull,"store truncates to 32 bits");
  d=D(0xB900001F,&ok); CHECK(mmio_a64_store_value(x,&d)==0,"xzr stores zero");
  d=D(0xB940001F,&ok); uint64_t x31=x[30]; mmio_a64_finish(x,&sp,&d,0x55); CHECK(x[30]==x31,"load to xzr discarded");
  printf(fails?"%d FAILURES\n":"all decoder tests passed\n",fails); return fails!=0; }
