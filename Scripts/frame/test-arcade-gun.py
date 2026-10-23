#!/usr/bin/env python3
"""Check the packaged geometry, including its actual optical-emitter vertices."""
import hashlib
import json
import math
from pathlib import Path
import sys
assets = Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'Assets'
meta=json.loads((assets/'FrameArcadeGun.json').read_text())
vertices=[]; normals=[]; faces=[]; used=set()
for line in (assets/'FrameArcadeGun.obj').read_text().splitlines():
    fields=line.split()
    if not fields: continue
    if fields[0]=='v': vertices.append(tuple(map(float,fields[1:])))
    if fields[0]=='vn': normals.append(tuple(map(float,fields[1:])))
    if fields[0]=='f': faces.append([tuple(map(int,p.split('//'))) for p in fields[1:]])
    if fields[0]=='usemtl': used.add(fields[1])
assert len(vertices)==13149 and len(normals)==13149 and len(faces)==17108
assert all(len(f)==3 and all(1<=v<=len(vertices) and 1<=n<=len(normals) for v,n in f) for f in faces)
assert all(len(p)==3 and all(math.isfinite(x) for x in p) for p in vertices+normals)
# Source part boundaries are in the independently supplied TC2 conversion manifest.
# Verify the lens geometry itself, not just the output manifest's declared muzzle.
lens=vertices[13024:13149]
for axis in (0,1):
    center=(min(v[axis] for v in lens)+max(v[axis] for v in lens))/2
    assert abs(center)<1e-6, (axis,center)
assert -.169<min(v[2] for v in lens)<-.167
assert -.165<max(v[2] for v in lens)<-.163
assert abs(min(v[2] for v in vertices)-meta['muzzle_m'][2])<1e-7
assert meta['muzzle_m'][:2]==[0,0] and meta['barrel_direction']==[0,0,-1]
assert meta['license']=='CC-BY-4.0' and meta['author']=='Martoscar'
materials={line.split()[1] for line in (assets/'FrameArcadeGun.mtl').read_text().splitlines() if line.startswith('newmtl ')}
assert used==materials and len(used)==12
for name,digest in meta['files'].items():
    assert hashlib.sha256((assets/name).read_bytes()).hexdigest()==digest, name
assert 'Creative Commons Attribution 4.0' in (assets/'FrameArcadeGun-NOTICE.txt').read_text()
print('Arcade gun: 17,108 triangles, material coverage/hashes, optical emitter centered on aim axis, attribution passed')
