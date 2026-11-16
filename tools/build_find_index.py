#!/usr/bin/env python3
"""Additive immutable prefix metadata bound to exact catalog2 generation/raw IDs."""
from pathlib import Path
import argparse,importlib.util,struct
TOOLS=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('find_catalog_parser',TOOLS/'build_launch_metadata.py')
parser=importlib.util.module_from_spec(spec);spec.loader.exec_module(parser)
CATEGORY_ROOTS={"01 HuCard":1,"02 CD":1,"03 SuperGrafx":1,
 "04 Translations":2,"05 Homebrew":3,"07 Museum":4,"06 Tools":5,"08 Unlicensed":6}
def category_for_path(path):
 root,separator,tail=path.partition('/')
 if not separator or not tail or root not in CATEGORY_ROOTS:
  raise ValueError('no curated category for path: '+path)
 return CATEGORY_ROOTS[root]
def build_find(catalog, categories=False, *, category_by_path=None):
 rows,generation=parser.parse_catalog(catalog)
 if category_by_path is not None:
  if set(category_by_path)!={row['path'] for row in rows}:
   raise ValueError('category mapping must cover exactly the catalog paths')
  if any(type(value) is not int or not 1<=value<=6 for value in category_by_path.values()):
   raise ValueError('category mapping values must be 1..6')
  categories=True
 output=bytearray(struct.pack('<4sBBHII',b'TPF1',2 if categories else 1,20,len(rows),generation,0))
 for row in rows:
  raw=row['raw'];title=raw[:88].split(b'\0',1)[0]
  if any(c<32 or c>126 for c in title):raise ValueError('prefix titles must use the printable ASCII catalog representation')
  prefix=title[:12].upper().ljust(12,b'\0')
  category=category_by_path[row['path']] if category_by_path is not None else category_for_path(row['path']) if categories else 0
  output.extend(bytes([raw[248]|(category<<2),raw[253],raw[255],raw[254]])+raw[249:253]+prefix)
 return bytes(output)
def main():
 cli=argparse.ArgumentParser(description=__doc__);cli.add_argument('catalog',type=Path);cli.add_argument('output',type=Path);cli.add_argument('--categories',action='store_true',help='include exact curated collection categories (v2)');args=cli.parse_args()
 data=build_find(args.catalog.read_bytes(),categories=args.categories)
 if args.output.exists() or args.output.is_symlink():raise ValueError('new find index output required; no overwrite')
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(data)
 print(f'Prepared {len(data)} bytes for generation-bound title search')
if __name__=='__main__':main()
