"""Native CD carrier switching with a real in-memory SDK filesystem boundary."""
from pathlib import Path
import re,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CDCheatTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not shutil.which("cc"):
   raise unittest.SkipTest("Host C compiler unavailable")
  cls.tmp=tempfile.TemporaryDirectory();cls.builds={}
  for label,define in [('legacy',None),('root','-DTEO_APP_ROOT="00TEOPCE"')]:
   exe=Path(cls.tmp.name)/label
   cmd=['cc','-std=c99','-Wno-unknown-pragmas','-Werror=implicit-function-declaration','-I',str(ROOT/'rom'),'-I',str(ROOT/'vendor/krikzz')]
   if define:cmd+=[define]
   cmd += [str(ROOT/'rom'/f) for f in ['card_io.c','pack.c','cheat_store_codec.c','cheat_store_io.c','cheat_cd.c','cd_guard.c','cd_guard_packet.c']]+[str(ROOT/'tests/cheat_cd_fake.c'),'-o',str(exe)]
   cls.builds[label]=(exe,subprocess.run(cmd,capture_output=True,text=True))
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,name):
  for variant,(exe,result) in self.builds.items():
   with self.subTest(variant=variant,case=name):
    self.assertEqual(result.returncode,0,result.stderr)
    r=subprocess.run([str(exe),name],capture_output=True,text=True,timeout=20)
    self.assertEqual(r.returncode,0,r.stderr)
 def test_cross_bank_sdk_path_bytes_remain_resident(self):
  cc=shutil.which('cc65') or '/tmp/teo-cc65/bin/cc65'
  if not Path(cc).is_file():self.skipTest('cc65 unavailable')
  with tempfile.TemporaryDirectory() as d:
   asm=Path(d)/'cd.s';r=subprocess.run([cc,'--cpu','HuC6280','-O','-I',str(ROOT/'vendor/krikzz'),'-o',str(asm),str(ROOT/'rom/cheat_cd.c')],capture_output=True,text=True);self.assertEqual(r.returncode,0,r.stderr)
   segment='';label=None;data={};segments={}
   for line in asm.read_text().splitlines():
    m=re.match(r'\.segment\s+"([^"]+)"',line)
    if m:segment=m[1]
    m=re.match(r'(\w+):$',line)
    if m:label=m[1];data[label]=bytearray();segments[label]=segment
    if label and re.match(r'\s*\.byte',line):data[label].extend(int(x,16) for x in re.findall(r'\$([0-9A-Fa-f]{2})',line))
   for text in [b'teo-menu2/user\0',b'teo-menu2/user/cd\0']:
    matches=[segments[k] for k,v in data.items() if v==text];self.assertTrue(matches,text);self.assertTrue(all(v=='RODATA' for v in matches),(text,matches))
 def test_consent_allows_only_bound_held_profile_and_still_checks_native_rows(self):self.case('risk')
 def test_active_held_profile_never_exports_but_disabled_history_can_clear(self):self.case('held')
 def test_no_selection_missing_profile_parent_launches_and_clears_previous_codes(self):self.case('no-profile')
 def test_fresh_stock_no_selection_performs_no_transaction(self):self.case('fresh-stock')
 def test_fresh_black_hole_passes_export_and_launch_preflight(self):self.case('fresh-bha-launch')
 def test_first_active_selection_creates_its_stock_parent(self):self.case('fresh-active')
 def test_recovers_old_first_launch_journal_with_absent_parent(self):self.case('legacy-fresh-txn')
 def test_a_b_no_pack_switches_native_codes_and_preserves_all_originals(self):self.case('switch')
 def test_every_io_fault_blocks_and_restart_recovers_old_or_complete_carrier(self):self.case('faults')
 def test_mismatched_identity_rom_codes_and_substantive_carrier_edits_never_write_native(self):self.case('invalid')
 def test_interrupted_quarantine_can_be_retried_without_losing_originals(self):self.case('double')
 def test_close_fault_blocks_all_further_io_until_reboot(self):self.case('close')
