"""Message windows own input through fades and retail context resets."""
from pathlib import Path
import ast
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
module = ast.parse((ROOT / 'tools/recomp/test_controls_native.py').read_text())
source = next(ast.literal_eval(n.value) for n in module.body
              if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'source' for t in n.targets))
source = source[:source.index('int main(')]
source = source.replace('/*DEVELOPER_HOTKEY*/', 'static int test_developer_hotkey(WPARAM w, LPARAM l){(void)w;(void)l;return 0;}')
source += r"""
static void popup(unsigned joy,unsigned sub,unsigned screen,unsigned phase,unsigned type) {
 reset_input();play(4);recomp_controls_substate(0);
 MEM32(0x35F798+0x5C4)=0x4140D8;
 MEM32(0x35F798+0x30)=phase;MEM32(0x35F798+0x4B4)=type;
 recomp_controls_state(0x4249D707,screen,joy,1);recomp_controls_substate(sub);
}
static void physical(XBOX_INPUT_STATE *state,int device,unsigned key) {
 memset(state,0,sizeof(*state));
 if(device){float channels[24]={0};channels[key]=1;pack(&state->Gamepad,channels);}
 else test_keys[key]=1;
}
int main(int argc,char **argv) {
 assert(argc==2);
 g_xbox_mem_offset=(ptrdiff_t)camera_memory-0x10000;
 snprintf(test_ini,sizeof(test_ini),"%s\\runtime.exe",argv[1]);
 recomp_controls_init((HWND)1);
 const unsigned screens[]={0xDDFB69D8,0xC2CBD863,0xC739FD0F};
 unsigned cases=0;
 for(unsigned custom=0;custom<2;++custom)for(unsigned joy=4;joy<=15;joy+=11)
 for(unsigned sub=0;sub<=5;++sub)for(unsigned screen=0;screen<3;++screen)
 for(unsigned phase=0;phase<3;++phase)for(unsigned type=0;type<2;++type)
 for(int device=0;device<2;++device) {
  g_binds[0][3][8]='R';g_binds[0][MENU_CONTEXT][8]=custom?'T':VK_SPACE;
  g_binds[1][3][8]=8;g_binds[1][MENU_CONTEXT][8]=custom?10:8;
  g_binds[0][MENU_CONTEXT][11]='F';g_binds[1][MENU_CONTEXT][11]=12;
  const unsigned family=device?XBOX_PROMPT_PLAYSTATION:XBOX_PROMPT_KEYBOARD;
  const unsigned confirm=resolved_binding(device,MENU_CONTEXT,8);
  const unsigned decline=resolved_binding(device,MENU_CONTEXT,11);
  popup(joy,sub,screens[screen],phase,type);
  assert(message_window_active());assert(mission_yes_no_active()==type);
  assert(recomp_controls_prompt_binding(8,family,-1)==confirm);
  if(type)assert(recomp_controls_prompt_binding(11,family,-1)==decline);
  /* Every physical key/channel: only Confirm, plus Decline for Yes/No.
   * Includes movement mapped to menu directions, support, mouse and sticks. */
  for(unsigned k=device?0:1;k<(device?24:256);++k){
   reset_input();XBOX_INPUT_STATE state;physical(&state,device,k);
   assert(mapper(&state,!device));float out[24];unpack(&state.Gamepad,out);
   for(unsigned a=0;a<24;++a)
    assert(out[a]==((a==8 && k==confirm || type && a==11 && k==decline)?1.f:0.f));
  }
  /* Even Confirm must not invoke Reload/Use in a restored joystick table. */
  assert(!recomp_controls_event_current(0x4249D707,screens[screen],joy,sub));
  recomp_controls_prompt_gameplay_body(1);
  assert(recomp_controls_prompt_binding(8,family,-1)==(device?8:'R'));
  recomp_controls_prompt_gameplay_body(0);
  /* A held Fire/Reload source is consumed across dismissal; movement resumes. */
  reset_input();XBOX_INPUT_STATE state;physical(&state,device,device?8:'R');
  assert(mapper(&state,!device));
  MEM32(0x35F798+0x30)=3;play(4);recomp_controls_substate(0);
  physical(&state,device,device?8:'R');assert(mapper(&state,!device));
  assert(!state.Gamepad.bAnalogButtons[0]);
  assert(recomp_controls_event_current(0x4249D707,0xC2CBD863,4,0));
  assert(recomp_controls_prompt_binding(8,family,-1)==(device?8:'R'));
  reset_input();physical(&state,device,device?8:'R');assert(mapper(&state,!device));
  assert(state.Gamepad.bAnalogButtons[0]==255);++cases;
 }
 /* An uninitialized/stale object cannot trap ordinary gameplay. */
 MEM32(0x35F798+0x5C4)=0;MEM32(0x35F798+0x30)=0;
 play(4);recomp_controls_substate(0);assert(!message_window_active());
 /* A higher-level pause/settings/result screen takes precedence. */
 MEM32(0x35F798+0x5C4)=0x4140D8;
 recomp_controls_state(0x4249D707,0x7084D38D,12,0);recomp_controls_substate(4);
 assert(!message_window_active());
 recomp_controls_state(0x4249D707,0x65872ADE,15,0);recomp_controls_substate(5);
 assert(!message_window_active());
 popup(4,0,0xDDFB69D8,1,0);recomp_controls_open();assert(!message_window_active());
 recomp_controls_close();recomp_controls_waiting_controller(1);assert(!message_window_active());
 recomp_controls_waiting_controller(0);
 printf("PASS: %u message cases; every key/controller channel, fade phases, reset contexts, custom/default Confirm/Decline, event isolation, held-input barrier and gameplay restoration.\n",cases);
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='merc-tutorial-context-') as folder:
    directory = Path(folder)
    (directory / 'test.c').write_text(source)
    command = ['C:/MinGW/bin/gcc.exe', '-std=c11', '-D_WIN32_WINNT=0x0601', '-O2']
    command += ['-I'+str(ROOT/p) for p in ['ports/mercenaries/src','src/input','src','include']]
    executable = directory / 'test.exe'
    subprocess.run(command+[str(directory/'test.c'),'-o',str(executable),'-luser32','-lm'],check=True)
    subprocess.run([str(executable),str(directory)],check=True)
