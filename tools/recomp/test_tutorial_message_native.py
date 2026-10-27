"""Run the retail message updater using packets from the production input mapper."""
from pathlib import Path
import ast, os, shutil, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[2]
module = ast.parse((ROOT/'tools/recomp/test_controls_native.py').read_text())
source = next(ast.literal_eval(n.value) for n in module.body if isinstance(n, ast.Assign)
              and any(isinstance(t, ast.Name) and t.id == 'source' for t in n.targets))
source = source[:source.index('static void test_free_camera(void)')]
source = source.replace('/*DEVELOPER_HOTKEY*/', '').replace('camera_memory[0x420000]', 'camera_memory[0x700000]')
source += r"""
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_seh_ebp;
float g_xmm0[4],g_xmm1[4],g_xmm2[4];
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define esi g_esi
#define xmm0 g_xmm0[0]
#define xmm1 g_xmm1[0]
#define xmm2 g_xmm2[0]
#define xmm0v g_xmm0
#define xmm1v g_xmm1
#define xmm2v g_xmm2
#undef RECOMP_TRACE_FUNC
#define RECOMP_TRACE_FUNC(a) ((void)0)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(a) abort()
static unsigned callbacks,unpauses;
void sub_00178950(void){eax=MEM32(0x413F68);esp+=4;}
void sub_0017A0A0(void){assert(MEM32(esp+8)==0xC2CBD863);MEM32(0x413F68)=MEM32(esp+8);++unpauses;esp+=4;}
void sub_00152580(void){esp+=8;}
void sub_00113A50(void){++callbacks;esp+=8;}
"""
retail = (ROOT/'ports/mercenaries/src/recomp/gen/recomp_0004.c').read_text()
for name in ['sub_000F1D80', 'sub_00100590']:
    start = retail.index('void '+name+'(void)')
    source += retail[start:retail.index('\n}\n', start)+3]
source += r"""
static void update(unsigned bits){
 MEM32(0x12000+0xC4)=bits;MEM32(0x4140D8)+=100;
 ecx=0x35F798;esp=0x20000;esi=0xABCD;
 sub_00100590();assert(esp==0x20008 && esi==0xABCD);
}
static unsigned packet(unsigned key){
 XBOX_INPUT_STATE state={0};reset_input();test_keys[key]=1;
 assert(mapper(&state,TRUE));
 assert(!state.Gamepad.wButtons && !state.Gamepad.sThumbLX && !state.Gamepad.sThumbLY);
 assert(!recomp_controls_event_current(0x4249D707,0xDDFB69D8,4,0));
 return (state.Gamepad.bAnalogButtons[0]?1u<<6:0) | (state.Gamepad.bAnalogButtons[3]?1u<<4:0);
}
int main(int argc,char**argv){
 assert(argc==2);g_xbox_mem_offset=(ptrdiff_t)camera_memory-0x10000;
 snprintf(test_ini,sizeof(test_ini),"%s\\runtime.exe",argv[1]);recomp_controls_init((HWND)1);
 MEM32(0x413F90)=0x12000;MEMF(0x6921A0)=.001f;MEMF(0x2EB710)=1.f;MEMF(0x2DC08C)=1.f;
 const unsigned jumps[]={0x100678,0x1006B1,0x100616,0x1005E2};
 for(unsigned i=0;i<4;++i)MEM32(0x100730+i*4)=jumps[i];
 unsigned confirm=resolved_binding(0,MENU_CONTEXT,8),decline=resolved_binding(0,MENU_CONTEXT,11);
 /* Two sequential Continue pages, then both outcomes of a Yes/No request. */
 for(unsigned page=0;page<4;++page){
  MEM32(0x413F68)=0xDDFB69D8;MEM32(0x35F798+0x30)=0;MEM32(0x35F798+0x4B4)=page>=2;
  MEM32(0x35F798+0x5C4)=0x4140D8;MEM32(0x35F798+0x5C0)=MEM32(0x4140D8);
  MEM32(0x35F798+0x5BC)=0x13000;MEMF(0x35F798+0x2C)=0;
  play(4);recomp_controls_state(0x4249D707,0xDDFB69D8,4,0);recomp_controls_substate(0);
  for(unsigned i=0;i<12;++i)update(0);
  assert(MEM32(0x35F798+0x30)==1);
  for(unsigned key=1;key<256;++key)if(key!=confirm && (page<2 || key!=decline)){
   update(packet(key));assert(MEM32(0x35F798+0x30)==1);
   assert(unpauses==page && callbacks==page);
   assert(recomp_controls_prompt_binding(8,XBOX_PROMPT_KEYBOARD,-1)==confirm);
  }
  update(packet(page==3?decline:confirm));assert(MEM32(0x35F798+0x30)==2 && callbacks==page+1);
  for(unsigned i=0;i<12;++i)update(0);
  assert(MEM32(0x35F798+0x30)==3 && unpauses==page+1 && callbacks==page+1);
  play(4);recomp_controls_substate(0);assert(!message_window_active());
 }
 puts("PASS: production mapper + retail updater: two Continue pages, Yes/No, blocked unrelated keys, original fades, exactly-once callback/unpause and guest stack preservation.");
}
"""
with tempfile.TemporaryDirectory(prefix='merc-message-retail-') as folder:
    d=Path(folder)
    if len(sys.argv)>1:
        shutil.copyfile(sys.argv[1], d/'mercenaries_recomp.ini')
    (d/'test.c').write_text(source)
    command=['C:/MinGW/bin/gcc.exe','-std=c11','-D_WIN32_WINNT=0x0601','-O2']
    command += ['-I'+str(ROOT/p) for p in ['ports/mercenaries/src','src/input','src','include']]
    subprocess.run(command+[str(d/'test.c'),'-o',str(d/'test.exe'),'-luser32','-lm'],check=True)
    env=dict(os.environ);env.pop('MERCENARIES_CONFIG_ROOT',None)
    subprocess.run([str(d/'test.exe'),str(d)],check=True,env=env)
