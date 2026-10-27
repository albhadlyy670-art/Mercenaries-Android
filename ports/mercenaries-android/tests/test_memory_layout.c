#define _GNU_SOURCE
#include "kernel/xbox_memory_layout.h"
#include "platform/win32_compat.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
extern ptrdiff_t g_xbox_mem_offset;
void xbox_kernel_set_thunk_address(uint32_t va, uint32_t n){ (void)va;(void)n; }
static int fails=0;
#define CHECK(c,m) do{ if(!(c)){printf("FAIL: %s\n",m);fails++;} else printf("ok: %s\n",m);}while(0)
#define G(va) ((volatile uint32_t*)((uintptr_t)(va)+g_xbox_mem_offset))
static sigjmp_buf jb; static volatile uintptr_t fault_addr;
static void segv(int s, siginfo_t *si, void *u){ (void)s;(void)u; fault_addr=(uintptr_t)si->si_addr; siglongjmp(jb,1);}
int main(void){
  /* minimal XBE: header, 1 section .text @0x11000 */
  static uint8_t xbe[0x2000]; memset(xbe,0,sizeof xbe);
  memcpy(xbe,"XBEH",4);
  *(uint32_t*)(xbe+0x104)=0x10000;            /* base */
  *(uint32_t*)(xbe+0x108)=0x1000;             /* header size */
  *(uint32_t*)(xbe+0x11C)=1;                  /* sections */
  *(uint32_t*)(xbe+0x120)=0x10000+0x200;      /* section headers VA */
  uint8_t *sh=xbe+0x200;
  *(uint32_t*)(sh+4)=0x11000; *(uint32_t*)(sh+8)=0x100; *(uint32_t*)(sh+0xC)=0x1000; *(uint32_t*)(sh+0x10)=0x100;
  *(uint32_t*)(sh+0x14)=0x10000+0x300; memcpy(xbe+0x300,".text\0",6);
  memset(xbe+0x1000,0xAB,0x100);
  BOOL ok=xbox_MemoryLayoutInit(xbe,sizeof xbe);
  CHECK(ok,"xbox_MemoryLayoutInit succeeds on POSIX");
  if(!ok){printf("%s\n",xbox_GetMemoryLayoutError());return 1;}
  CHECK(g_xbox_mem_offset!=0 && ((uintptr_t)g_xbox_mem_offset & 0xFFFF)==0,"arena base is 64KiB aligned");
  CHECK(*(volatile uint8_t*)((uintptr_t)0x11000+g_xbox_mem_offset)==0xAB,"XBE section copied to its guest VA");
  *G(0x00070000)=0xCAFEBABE;
  CHECK(*G(0x04070000)==0xCAFEBABE,"64 MiB mirror aliases RAM (read)");
  *G(0x0C070000)=0x12345678;
  CHECK(*G(0x00070000)==0x12345678,"mirror write lands in base RAM");
  CHECK(*G(0x80000000)==0 && (*G(0x80000000)=0xDEADBEEF, *G(0x80000000)==0xDEADBEEF),"D3D fence page at 0x80000000 writable");
  CHECK(*(volatile uint32_t*)((uintptr_t)0x8001003C+g_xbox_mem_offset)==0x80,"fake kernel PE e_lfanew at 0x8001003C");
  /* KSEG alias demand-mapping */
  *G(0x00123000)=0xA5A5A5A5;
  CHECK(xbox_MapPhysicalAlias(0x80123000),"xbox_MapPhysicalAlias(0x80123000)");
  CHECK(*G(0x80123000)==0xA5A5A5A5,"KSEG0 alias reads physical RAM");
  CHECK(xbox_MapPhysicalAlias(0x84123000),"second alias of same RAM maps");
  *G(0x84123000)=0x0F0F0F0F; CHECK(*G(0x00123000)==0x0F0F0F0F,"KSEG alias write visible at physical");
  /* unmapped MMIO range must fault with the right address */
  struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_sigaction=segv; sa.sa_flags=SA_SIGINFO|SA_NODEFER; sigaction(SIGSEGV,&sa,NULL);
  if(sigsetjmp(jb,1)==0){ volatile uint32_t v=*G(0xFD000040); (void)v; CHECK(0,"MMIO read should fault"); }
  else CHECK((uint32_t)(fault_addr-g_xbox_mem_offset)==0xFD000040,"MMIO read faults; si_addr maps back to guest VA 0xFD000040");
  if(sigsetjmp(jb,1)==0){ *G(0xFE800000)=1; CHECK(0,"APU write should fault"); }
  else CHECK((uint32_t)(fault_addr-g_xbox_mem_offset)==0xFE800000,"APU MMIO write faults at 0xFE800000");
  /* demand page via xbox_AllocFixed (used for framebuffer range) */
  if(sigsetjmp(jb,1)==0){ *G(0xF4000000)=1; CHECK(0,"unmapped should fault"); } else {
    CHECK(xbox_AllocFixed(fault_addr&~(uintptr_t)0xFFFF,0x10000)!=NULL,"xbox_AllocFixed demand page"); *G(0xF4000000)=7; CHECK(*G(0xF4000000)==7,"demand page usable"); }
  /* heap */
  uint32_t a=xbox_HeapAlloc(4096,4096); CHECK(a>=XBOX_HEAP_BASE && (a&4095)==0,"xbox_HeapAlloc aligned & in heap");
  *G(a)=42; CHECK(*G(a+0x04000000)==42,"heap block visible through mirror");
  xbox_MemoryLayoutShutdown();
  printf(fails?"%d FAILURES\n":"ALL MEMORY TESTS PASSED\n",fails); return fails!=0; }
