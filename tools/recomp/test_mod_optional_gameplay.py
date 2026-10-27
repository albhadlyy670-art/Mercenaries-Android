"""Opt-in gameplay configuration, Lua behavior, and real disposable save I/O."""
from pathlib import Path
import subprocess,tempfile,unittest,sys
ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'ports/mercenaries/src'
sys.path.insert(0,str(ROOT))
class OptionalGameplayTests(unittest.TestCase):
 def test_slot_tracking(self):
  source=r"""
#include <cassert>
#include <fstream>
#include "mod_saves.cpp"
#include "kernel/kernel.h"
static const char *test_profile="je3";
extern "C" const char *recomp_mod_save_profile(){return test_profile;}
static xbox_file_write_observer title_write_observer;
static xbox_file_close_observer title_close_observer;
extern "C" void xbox_file_observe(xbox_file_write_observer w,xbox_file_close_observer c){title_write_observer=w;title_close_observer=c;}
static void invalidate_read_cache_for_handle(HANDLE){}
static void cleanup_read_cache_for_handle(HANDLE){}
static void cleanup_dir_context_for_handle(HANDLE){}
#undef XBOX_TRACE
#define XBOX_TRACE(...) ((void)0)
KERNEL_SAVE_IO

static std::vector<unsigned char> read_file(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
 fs::path base=fs::absolute(argv[1]);fs::create_directories(base/L"UserData"/L"slotdir");
 auto file=base/L"UserData"/L"slotdir"/L"Mercenaries Saves";
 std::vector<unsigned char> bytes(container_size,0x42);
 for(unsigned slot=0;slot<8;++slot){bytes[0x64+slot*32]='A'+slot;bytes[0x65+slot*32]=0;}
 auto hcrc=crc(bytes.data(),512);memcpy(bytes.data(),&hcrc,4);
 assert(atomic_write(file,bytes.data(),container_size));
 recomp_mod_saves_init(base.u8string().c_str());
 auto save=[&](unsigned slot){
  HANDLE h=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(h!=INVALID_HANDLE_VALUE);
  auto offset=512+slot*block_size;bytes[offset+9]++;auto c=crc(bytes.data()+offset,block_size);memcpy(bytes.data()+offset,&c,4);
  bytes[0x24+slot*8]++;auto ref=crc(bytes.data()+offset+4,0x400c)^crc(bytes.data()+offset+4+0x400c,0x9c5c)^crc(bytes.data()+offset+4+0x400c+0x9c5c,0x54);memcpy(bytes.data()+4+slot*4,&ref,4);c=crc(bytes.data(),512);memcpy(bytes.data(),&c,4);
  LARGE_INTEGER o{};XBOX_IO_STATUS_BLOCK status{};
  // Retail header-first order, each completed block closes its own handle.
  o.QuadPart=0;assert(SetFilePointerEx(h,o,nullptr,FILE_BEGIN));assert(xbox_NtWriteFile(h,nullptr,nullptr,nullptr,&status,bytes.data(),512,&o)==STATUS_SUCCESS);assert(status.Information==512);assert(xbox_NtClose(h)==STATUS_SUCCESS);
  h=CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);assert(h!=INVALID_HANDLE_VALUE);
  o.QuadPart=offset;assert(SetFilePointerEx(h,o,nullptr,FILE_BEGIN));assert(xbox_NtWriteFile(h,nullptr,nullptr,nullptr,&status,bytes.data()+offset,block_size,nullptr)==STATUS_SUCCESS);assert(status.Information==block_size);assert(xbox_NtClose(h)==STATUS_SUCCESS);
 };
 save(2);save(5);
 auto before=read_file(file);assert(fs::exists(marker));
 test_profile="other";recomp_mod_saves_init(base.u8string().c_str());assert(recomp_mod_delete_latest_save()==1);assert(read_file(file)==before);
 test_profile="je3";recomp_mod_saves_init(base.u8string().c_str());assert(recomp_mod_delete_latest_save()==1);
 auto after=read_file(file);assert(after.size()==before.size());assert(!after[0x64+5*32]);assert(after[0x64+2*32]=='C');
 assert(u32(after.data())==crc(after.data(),512));
 for(size_t i=4;i<after.size();++i)if(!((i>=4+5*4&&i<8+5*4)||(i>=0x24+5*8&&i<0x2c+5*8)||(i>=0x64+5*32&&i<0x84+5*32)))assert(after[i]==before[i]);
 assert(recomp_mod_delete_latest_save()==1);assert(read_file(file)==after);
 bytes=after;save(1);before=read_file(file);bytes[0x24+1*8]++;hcrc=crc(bytes.data(),512);memcpy(bytes.data(),&hcrc,4);assert(atomic_write(file,bytes.data(),container_size));
 assert(recomp_mod_delete_latest_save()==0);assert(read_file(file)==bytes);
 puts("PASS: latest slot only; older/other-profile slots preserved; restart record; changed-slot refusal; valid header.");
}
"""
  with tempfile.TemporaryDirectory(prefix='merc-permadeath-') as temp:
   kernel=(ROOT/'src/kernel/kernel_file.c').read_text()
   body=kernel[kernel.index('NTSTATUS __stdcall xbox_NtWriteFile('):kernel.index('NTSTATUS __stdcall xbox_NtDeleteFile(')]
   source=source.replace('KERNEL_SAVE_IO',body)
   d=Path(temp);(d/'test.cpp').write_text(source,encoding='utf-8')
   from tools.recomp.test_mod_loader_native import msvc
   import shutil
   env=msvc()
   subprocess.run([shutil.which('cl',path=env['PATH']),'/nologo','/std:c++17','/EHsc','/DNOMINMAX','/I'+str(SRC),'/I'+str(ROOT/'src'),str(d/'test.cpp'),'/Fe:'+str(d/'test.exe')],cwd=d,env=env,check=True)
   subprocess.run([str(d/'test.exe'),str(d/'disposable-saves')],check=True)
 def test_lua_contracts_and_followers(self):
  lua=ROOT/'third_party/lua-5.0.3'
  runner=r"""
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
int main(int argc,char **argv){lua_State *L=lua_open();luaopen_base(L);luaopen_table(L);lua_settop(L,0);if(lua_dofile(L,argv[1])){fprintf(stderr,"%s\n",lua_tostring(L,-1));return 1;}lua_close(L);return 0;}
"""
  fixture=r"""
RecompModConfig={followers=true,contracts={sw_allies3=true}}
local mem={}
function Utility_WriteNumberToScribbleMemory(k,v) mem[k]=v end
function Utility_SavePlayerData() saved=true end
function Utility_LoadUtilityScript(s) end
function util_IsAceMission() return ace end
function util_IsPlayableIntro() return intro end
function Retry(v) assert(v==false) retried=true end
function mission_MissionCleanup() cleaned=true end
function mission_MissionFailure2() originalFail=true end
function mission_HeroKilled2() originalDeath=true end
local faction={player0='prokat',ally='prokat',enemy='nk',mission='prokat'}
local alive={player0=true,ally=true,enemy=true,mission=true}
function Actor_IsAlive(a) return alive[a] end
function Utility_GetActor(a) return a end
function Ai_GetFaction(a) return faction[a] end
local registered={mission=true}
function follow_ActorData(a) return registered[a] end
function follow_Set(a,following) assert(following==false) registered[a]=true end
function follow_Disable(a) registered[a]=nil end
function Actor_GetPosition(a) return 0,0,0 end
function Utility_GetActorsInRange(kind,x,y,z,r) assert(r==40) return {'player0','ally','enemy','mission'} end
"""
  checks=r"""
sMissionAssetID='sw_allies3' sQuadrant='sw' sFaction='allies' iMission=2 sMemoryKey_ArcBookmark='current_allies_mission'
ace=true assert(not Recomp_FailContractPermanently()) ace=false
intro=true assert(not Recomp_FailContractPermanently()) intro=false
sMissionAssetID='sw_allies4' assert(not Recomp_FailContractPermanently()) sMissionAssetID='sw_allies3'
Recomp_InstallMissionHooks() Recomp_InstallMissionHooks()
mission_MissionFailure2()
assert(not originalFail and retried and cleaned and saved)
assert(mem.current_allies_mission==3 and mem.recomp_failed_sw_allies3==1)
assert(not mem.total_missions_completedallies and not mem.num_std_missions_cmpl_allies)
sMissionAssetID='sw_allies4' mission_MissionFailure2() assert(originalFail)
sMissionAssetID='sw_allies3'
Recomp_UpdateFollowers() assert(registered.ally and not registered.player0 and not registered.enemy and registered.mission)
faction.ally='nk' Recomp_UpdateFollowers() assert(not registered.ally and registered.mission)
print('PASS: allowlist, Ace/intro exclusions, sequence advancement without rewards, single application, faction-limited followers.')
"""
  with tempfile.TemporaryDirectory(prefix='merc-mod-lua-') as temp:
   d=Path(temp);(d/'runner.c').write_text(runner);(d/'test.lua').write_text(fixture+'\n'+(SRC/'mod_gameplay.lua').read_text()+'\n'+checks)
   subprocess.run(['C:/MinGW/bin/gcc.exe','-O1','-w','-I'+str(lua/'include'),'-I'+str(lua/'src'),str(d/'runner.c'),*map(str,(lua/'src').glob('*.c')),*map(str,(lua/'src/lib').glob('*.c')),'-o',str(d/'test.exe')],check=True)
   subprocess.run([str(d/'test.exe'),str(d/'test.lua')],check=True)
if __name__=='__main__':unittest.main()
