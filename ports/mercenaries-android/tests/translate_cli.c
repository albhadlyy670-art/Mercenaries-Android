#include "hlsl2glsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){ FILE*f=fopen(argv[1],"rb"); if(!f)return 2; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET); char*b=malloc(n+1); fread(b,1,n,f); b[n]=0;
 const char*defs[16]; int nd=0; for(int i=4;i+1<argc&&nd<8;i+=2){defs[nd*2]=argv[i];defs[nd*2+1]=argv[i+1];nd++;}
 HlslResult r; int rc=hlsl_translate_ex(b,n,argc>3?argv[3]:"main",strcmp(argv[2],"ps")==0,defs,nd,&r);
 if(rc){fprintf(stderr,"ERROR %s\n",r.error);return 1;} fputs(r.glsl,stdout);
 fprintf(stderr,"inputs=%d varyings=%d cbufs=%d samplers=%d targets=%d\n",r.n_inputs,r.n_varyings,r.n_cbufs,r.n_samplers,r.n_targets); return 0; }
