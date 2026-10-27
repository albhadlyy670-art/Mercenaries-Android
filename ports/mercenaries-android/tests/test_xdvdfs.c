#include "xdvdfs_extract.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
static int cb(uint64_t d,uint64_t t,const char*p,void*u){ (void)p;(void)u; static int last=-1; int pc=(int)(d*100/(t?t:1)); if(pc!=last && pc%25==0){printf("  %d%%\n",pc);last=pc;} return 0; }
int main(int argc,char**argv){
  int fd=open(argv[1],O_RDONLY); char err[256]; uint64_t tb; uint32_t fc;
  int rc=xiso_measure(fd,&tb,&fc); printf("measure rc=%d bytes=%llu files=%u\n",rc,(unsigned long long)tb,fc);
  rc=xiso_extract(fd,argv[2],cb,NULL,err,sizeof err); printf("extract rc=%d err='%s'\n",rc,err); return rc!=0; }
