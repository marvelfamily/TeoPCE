#!/usr/bin/env python3
"""Build a new teo-menu3 catalog/index/launch stage from preserved ROM sources.

No ROM, source library, installed metadata or SD writes. For owned diagnostics,
fixture_metadata(v1_catalog_bytes, {launch_path: rom_bytes}) returns the same
three-file schema without changing runtime control flow.
"""
from pathlib import Path,PurePosixPath
import argparse
import hashlib
import importlib.util
import json
import os
import re
import struct
import sys
import tempfile
import zlib
import wave
sys.dont_write_bytecode=True
TOOLS=Path(__file__).resolve().parent
PROJECT=TOOLS.parent
BUILD=PROJECT/"build"
HEADER=struct.Struct("<4sBBHII")
RECORD=struct.Struct("<III")
MAX_ROM=8*1024*1024
MAX_CUE=65536
SOURCE_CATALOG_SHA="70ae379a82a3ba137f296f332de856c9f8fe2426cafd75d27c62a76c002f1d91"
PROPOSALS_SHA="1f86c5e3165980fe84adc6a723d27abab3f5f9c56c8de69a52292745ae1341dc"
PRESERVATION_SHA="aa239f768880f7c57ad29d92ca84995c846854433591a7105f47f3227584ca87"
FILENAMES={"catalog2.bin","index.bin","launch.bin"}
def require(value,message):
 if not value:raise ValueError(message)
def sha(data):return hashlib.sha256(data).hexdigest()
def encoded(data):return (json.dumps(data,ensure_ascii=False,indent=2,sort_keys=True)+"\n").encode()
def _module(name,filename):
 spec=importlib.util.spec_from_file_location(name,TOOLS/filename);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
pack=_module("launch_metadata_pack","build_index.py")
sources=_module("launch_metadata_sources","build_cheat_view.py")

def parse_catalog(data):
 require(type(data) is bytes,"catalog must be exact bytes")
 records,generation=sources._catalog(data)
 rows=[]
 for i,row in enumerate(records.values()):
  raw=data[16+i*256:16+(i+1)*256]
  genre=sources._field(raw[216:236],"genre");region=sources._field(raw[236:248],"region")
  require(genre in pack.GENRES and raw[253]==pack.GENRES[genre],"catalog genre string/enum mismatch")
  require(raw[254]==pack.REGIONS.get(region,4),"catalog region string/enum mismatch")
  first=row["title"][:1];letter=ord(first.upper()) if "A"<=first<="Z" or "a"<=first<="z" else ord("#")
  require(raw[255]==letter,"catalog initial mismatch")
  rows.append({**row,"index":i,"genre":genre,"genre_id":raw[253],"region":region,"raw":raw})
 return rows,generation

def eligible(row):return row["kind"]==1 and PurePosixPath(row["path"]).suffix.casefold()==".pce"

def launchable(row):return eligible(row) or (row["kind"]==2 and PurePosixPath(row["path"]).suffix.casefold()==".cue")

def encode_launch(catalog2_bytes,identities):
 """Bind index→(size,crc32) to every eligible row; unsupported rows retain key/0/0."""
 rows,generation=parse_catalog(catalog2_bytes)
 require(isinstance(identities,dict) and {r["index"] for r in rows if eligible(r)}<=set(identities)<={r["index"] for r in rows if launchable(r)},"launch identity index scope mismatch")
 require(all(type(i) is int for i in identities),"launch index must be integer")
 result=bytearray(HEADER.pack(b"TLA1",1,RECORD.size,len(rows),generation,0))
 for row in rows:
  size,crc=identities.get(row["index"],(0,0))
  if row["index"] in identities:
   require(type(size) is int and 1<=size<=(MAX_CUE if row["kind"]==2 else MAX_ROM),"launch target size outside media bounds")
   require(type(crc) is int and 0<=crc<=0xffffffff,"ROM CRC32 outside uint32")
  result.extend(RECORD.pack(row["key"],size,crc))
 return bytes(result)

def merge_genres(catalog2_bytes,proposals):
 """Change only reviewed Unknown fields; preserve record order and all other identity bytes."""
 rows,_=parse_catalog(catalog2_bytes);require(isinstance(proposals,list),"genre proposals must be a list")
 payload=bytearray(catalog2_bytes[16:]);seen=set()
 for p in proposals:
  i=p.get("index");require(type(i) is int and 0<=i<len(rows) and i not in seen,"invalid/duplicate genre index");seen.add(i)
  row=rows[i]
  require(row["kind"]!=3 and row["genre"]=="Unknown" and row["genre_id"]==20,"only visible Unknown genres may change")
  require((p.get("title"),p.get("launch_path"),p.get("kind"),p.get("stable_key"),p.get("genre"),p.get("genre_id"))==
          (row["title"],row["path"],row["kind"],f"{row['key']:08x}","Unknown",20),"genre target identity mismatch")
  genre=p.get("new_genre");require(genre in pack.GENRES and genre!="Unknown" and p.get("new_genre_id")==pack.GENRES[genre],"genre proposal enum mismatch")
  value=genre.encode("utf-8");require(len(value)<20,"genre field too long")
  payload[i*256+216:i*256+236]=value+bytes(20-len(value))
 full,index=pack.build_pack(struct.pack("<4sHH",b"TPCE",1,len(rows))+payload)
 changed={i for i in range(len(rows)) if full[16+i*256:16+(i+1)*256]!=catalog2_bytes[16+i*256:16+(i+1)*256]}
 require(changed==seen,"unexpected genre change set")
 for i,row in enumerate(rows):
  old=row["raw"];new=full[16+i*256:16+(i+1)*256]
  require(new[:216]==old[:216] and new[236:253]==old[236:253] and new[254:]==old[254:],"catalog ID/title/path/kind/key/region changed")
 return full,index

def verify_rom_sources(catalog2_bytes,library_root,preservation):
 """Read each eligible ROM once; require preservation size/SHA256 and stable open-file identity."""
 rows,_=parse_catalog(catalog2_bytes);root=Path(library_root).resolve(strict=True)
 require(root.is_dir(),"ROM library root is not a directory")
 require(isinstance(preservation,list),"preservation manifest must be a list")
 pins={}
 for p in preservation:
  path=p.get("path");require(isinstance(path,str) and path not in pins,"duplicate/invalid preservation path");pins[path]=p
 identities={};proof=[]
 for row in rows:
  if not eligible(row):continue
  pin=pins.get(row["path"]);require(pin is not None,"HuCard missing from preservation manifest: "+row["path"])
  size=pin.get("size");digest=pin.get("sha256")
  require(type(size) is int and 1<=size<=MAX_ROM and isinstance(digest,str) and re.fullmatch(r"[0-9a-f]{64}",digest),"preservation size/SHA invalid")
  path=sources._checked_source(root,row["path"])
  checksum=0;total=0;hasher=hashlib.sha256()
  with sources._open_regular(path) as stream:
   before=os.fstat(stream.fileno());require(before.st_size==size,"source ROM size differs from preservation: "+row["path"])
   for chunk in iter(lambda:stream.read(1024*1024),b""):
    total+=len(chunk);require(total<=MAX_ROM,"source ROM exceeds 8MiB")
    checksum=zlib.crc32(chunk,checksum);hasher.update(chunk)
   after=os.fstat(stream.fileno())
  statkey=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
  require(statkey(before)==statkey(after) and total==size,"source ROM changed during read")
  require(hasher.hexdigest()==digest,"source ROM SHA differs from preservation: "+row["path"])
  checksum&=0xffffffff;identities[row["index"]]=(total,checksum)
  proof.append({"index":row["index"],"path":row["path"],"title":row["title"],"stable_key":f"{row['key']:08x}","bytes":total,"crc32":f"{checksum:08x}","sha256":digest,"preservation_match":True,"preservation_verification":pin.get("verification"),"preservation_ledgers":pin.get("build_ledgers",[])})
  if len(proof)%50==0:print(json.dumps({"hucard_source_roms_verified":len(proof)}),flush=True)
 return identities,proof

def verify_launch_sources(catalog2_bytes,library_root,preservation):
 """Verify HuCards fully; bind original CUE bytes and validate preserved track geometry."""
 identities,proof=verify_rom_sources(catalog2_bytes,library_root,preservation)
 cd_identities,cd_proof=verify_cd_sources(catalog2_bytes,library_root,preservation)
 return identities|cd_identities,sorted(proof+cd_proof,key=lambda p:p['index'])

def verify_cd_sources(catalog2_bytes,library_root,preservation):
 """Read original CUEs fully; check track geometry, with recorded track SHA authority.
 This function does not read HuCard content or rehash whole CD tracks.
 """
 identities={};proof=[]
 rows,_=parse_catalog(catalog2_bytes);root=Path(library_root).resolve(strict=True)
 require(root.is_dir(),'ROM library root is not a directory')
 require(isinstance(preservation,list),'preservation manifest must be a list');pins={}
 for pin in preservation:
  path=pin.get('path');require(isinstance(path,str) and path not in pins,'duplicate/invalid preservation path');pins[path]=pin
 def checked(relative,limit):
  pin=pins.get(relative);require(pin is not None,'CD source missing from preservation: '+relative)
  require(type(pin.get('size')) is int and 0<pin['size']<=limit and isinstance(pin.get('sha256'),str) and re.fullmatch(r'[0-9a-f]{64}',pin['sha256']),'invalid CD preservation pin')
  path=sources._checked_source(root,relative);return path,pin
 statkey=lambda s:(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)
 for row in rows:
  if row['kind']!=2:continue
  require(launchable(row),'CD kind/suffix mismatch')
  cue,pin=checked(row['path'],MAX_CUE)
  with sources._open_regular(cue) as f:
   before=os.fstat(f.fileno());raw=f.read(MAX_CUE+1);after=os.fstat(f.fileno())
  require(statkey(before)==statkey(after) and len(raw)==pin['size'] and sha(raw)==pin['sha256'],'CUE changed or differs from preservation')
  refs=[];tracks=[];active=None;track=None
  for line in raw.decode('utf-8-sig').splitlines():
   text=line.strip()
   if not text or re.match(r'^(REM|CATALOG|PERFORMER|TITLE|SONGWRITER|CDTEXTFILE|FLAGS|ISRC)(?:\s|$)',text,re.I):continue
   m=re.fullmatch(r'FILE\s+(?:"([^"\n]+)"|(\S+))\s+(BINARY|WAVE)',text,re.I)
   if m:
    name=m[1] or m[2];sources._relative(name);relative=str(PurePosixPath(row['path']).parent/PurePosixPath(name))
    path,filepin=checked(relative,0xffffffff)
    with sources._open_regular(path) as f:
     first=os.fstat(f.fileno());require(first.st_size==filepin['size'],'track size differs from preservation')
     sectors=None
     if m[3].upper()=='WAVE':
      with wave.open(f,'rb') as wav:
       require((wav.getnchannels(),wav.getsampwidth(),wav.getframerate(),wav.getcomptype())==(2,2,44100,'NONE'),'unsupported WAV geometry')
       frames=wav.getnframes();require(frames>0 and frames%588==0 and frames*4<=first.st_size-f.tell(),'WAV alignment or truncated data');sectors=frames//588
     last=os.fstat(f.fileno());require(statkey(first)==statkey(last),'track changed during validation')
    refs.append({'path':relative,'bytes':first.st_size,'sha256':filepin['sha256'],'preservation_size_match':True,'full_content_hash_read':False,'type':m[3].upper(),'sectors':sectors,'indices':[]})
    active=len(refs)-1;track=None;continue
   m=re.fullmatch(r'TRACK\s+(\d{2})\s+(AUDIO|MODE1/2352|MODE1/2048|MODE2/2352)',text,re.I)
   if m:
    require(active is not None and len(tracks)<99,'TRACK without FILE or too many tracks');mode=m[2].upper();ref=refs[active]
    if ref['type']=='WAVE':require(mode=='AUDIO','WAVE data track')
    else:
     sector=2048 if mode=='MODE1/2048' else 2352;require(ref['bytes']%sector==0,'binary sector misalignment');sectors=ref['bytes']//sector
     require(ref['sectors'] in (None,sectors),'mixed binary sector sizes');ref['sectors']=sectors
    track={'number':int(m[1]),'mode':mode,'file':ref['path'],'indices':{}};tracks.append(track);continue
   m=re.fullmatch(r'(INDEX\s+\d{2}|PREGAP|POSTGAP)\s+(\d+):(\d{2}):(\d{2})',text,re.I)
   require(m is not None and track is not None,'unknown or misplaced CUE statement')
   require(int(m[3])<60 and int(m[4])<75,'invalid CUE timestamp');value=(int(m[2])*60+int(m[3]))*75+int(m[4])
   if m[1].upper().startswith('INDEX'):
    index=int(m[1].split()[1]);require(index not in track['indices'] and 0<=value<refs[active]['sectors'],'duplicate or out-of-range INDEX')
    track['indices'][index]=value;refs[active]['indices'].append(value)
  require(tracks and [t['number'] for t in tracks]==list(range(1,len(tracks)+1)) and any(t['mode']!='AUDIO' for t in tracks),'missing/noncontiguous/data TRACKs')
  for t in tracks:require(1 in t['indices'] and list(t['indices'])==sorted(t['indices']) and list(t['indices'].values())==sorted(t['indices'].values()),'missing/unordered INDEX01')
  for ref in refs:require(ref['indices'] and ref['indices']==sorted(ref['indices']),'unreferenced/decreasing CUE FILE indices')
  identities[row['index']]=(len(raw),zlib.crc32(raw)&0xffffffff)
  proof.append({'index':row['index'],'path':row['path'],'title':row['title'],'stable_key':f"{row['key']:08x}",'bytes':len(raw),'crc32':f"{identities[row['index']][1]:08x}",'sha256':pin['sha256'],'preservation_match':True,'native_identity_basename':PurePosixPath(row['path']).name,'cue_track_count':len(tracks),'tracks':refs})
 return identities,sorted(proof,key=lambda p:p['index'])

def fixture_metadata(v1_catalog_bytes,roms_by_path):
 """Owned-test helper: exact ROM bytes use the production encoder, including CD/SGX zero rows."""
 full,index=pack.build_pack(v1_catalog_bytes);rows,_=parse_catalog(full)
 require(isinstance(roms_by_path,dict) and set(roms_by_path)=={r["path"] for r in rows if eligible(r)},"fixture ROM path scope mismatch")
 require(all(type(data) is bytes for data in roms_by_path.values()),"fixture ROMs must be exact bytes")
 identities={r["index"]:(len(roms_by_path[r["path"]]),zlib.crc32(roms_by_path[r["path"]])&0xffffffff) for r in rows if eligible(r)}
 return {"catalog2.bin":full,"index.bin":index,"launch.bin":encode_launch(full,identities)}

def write_stage(destination,files,report):
 destination=Path(destination)
 require(not destination.exists() and not destination.is_symlink(),"new metadata stage required; no overwrite")
 require(set(files)==FILENAMES and all(type(v) is bytes for v in files.values()),"exact three metadata payloads required")
 parent=destination.parent.resolve(strict=True);destination=parent/destination.name
 require(destination.name not in ("",".",".."),"invalid stage name")
 with tempfile.TemporaryDirectory(dir=parent,prefix=".launch-metadata-") as temp:
  temp=Path(temp);menu=temp/"teo-menu3";menu.mkdir()
  payloads={"teo-menu3/"+name:data for name,data in files.items()};payloads["private-provenance.json"]=encoded(report)
  for name,data in payloads.items():
   path=temp/name
   with path.open("xb") as output:output.write(data);output.flush();os.fsync(output.fileno())
   require(path.read_bytes()==data,"local metadata write readback failed")
  require(not destination.exists() and not destination.is_symlink(),"stage appeared during preparation")
  os.replace(temp,destination)
 return destination

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument("--catalog",type=Path,required=True)
 parser.add_argument("--genres",type=Path,required=True)
 parser.add_argument("--library-root",type=Path,required=True)
 parser.add_argument("--preservation",type=Path,required=True)
 parser.add_argument("--output",type=Path,required=True)
 args=parser.parse_args()
 output=args.output.absolute();parent=output.parent.resolve(strict=True)
 require(parent.is_relative_to(BUILD.resolve()) and output.name not in ("teo-menu2","card-v032","card-v032-descriptions-next"),"CLI output must be a new build stage")
 inputs={"catalog":args.catalog.resolve(strict=True),"genres":args.genres.resolve(strict=True),"preservation":args.preservation.resolve(strict=True)}
 blobs={}
 for name,path in inputs.items():
  with sources._open_regular(path) as stream:blobs[name]=stream.read(16*1024*1024+1)
  require(len(blobs[name])<=16*1024*1024,"metadata source exceeds bound")
 require(sha(blobs["catalog"])==SOURCE_CATALOG_SHA and sha(blobs["genres"])==PROPOSALS_SHA and sha(blobs["preservation"])==PRESERVATION_SHA,"reviewed production input authority changed")
 proposals=json.loads(blobs["genres"]);require(len(proposals)==207,"expected exact reviewed207 genre corrections")
 original_rows,_=parse_catalog(blobs["catalog"]);require(len(original_rows)==967,"expected exact raw967 catalog")
 full,index=merge_genres(blobs["catalog"],proposals)
 identities,proof=verify_launch_sources(full,args.library_root,json.loads(blobs["preservation"]))
 launch=encode_launch(full,identities);files={"catalog2.bin":full,"index.bin":index,"launch.bin":launch}
 for name,path in inputs.items():require(sha(path.read_bytes())==sha(blobs[name]),"metadata authority changed during ROM verification")
 rows,generation=parse_catalog(full)
 report={"status":"PASS_METADATA_STAGE_ONLY","metadata_root":"teo-menu3","raw_catalog_records":len(rows),"visible_records":sum(r["kind"]!=3 for r in rows),"hucard_launch_records":sum(eligible(r) for r in rows),"cd_launch_records":sum(r["kind"]==2 and r["index"] in identities for r in rows),"unsupported_zero_records":len(rows)-len(identities),"genre_changes":len(proposals),"known_genres_changed":0,"remaining_visible_unknown":sum(r["kind"]!=3 and r["genre_id"]==20 for r in rows),"catalog_generation":f"{generation:08x}","all_original_identity_bytes_preserved":True,"preservation_sha256_verified_for_all_hucards":True,"rom_bytes_read":sum(p["bytes"] for p in proof),"library_root":str(args.library_root.resolve()),"input_snapshots":{name:{"path":str(path),"bytes":len(blobs[name]),"sha256":sha(blobs[name])} for name,path in inputs.items()},"tool_sources_sha256":{p.name:sha(p.read_bytes()) for p in (Path(__file__),TOOLS/"build_index.py",TOOLS/"build_cheat_view.py")},"files":{name:{"bytes":len(data),"sha256":sha(data)} for name,data in files.items()},"roms":proof,"source_library_modified":False,"installed_teo_menu2_modified":False,"sd_or_usb_accessed":False,"hardware_tested":False,"user_games_launched":False}
 destination=write_stage(output,files,report)
 print(json.dumps({k:v for k,v in report.items() if k not in ("roms","input_snapshots","tool_sources_sha256")}|{"stage":str(destination)},indent=2))
 return 0
if __name__=="__main__":raise SystemExit(main())
