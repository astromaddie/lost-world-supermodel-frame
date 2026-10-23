#!/usr/bin/env python3
"""Convert the approved, independently licensed TC2 Namco mesh to our existing OBJ path.
Usage: import-arcade-gun.py /path/to/namco-gun.tcgun [output Assets directory]
No ROM, texture extraction, or additional Python dependencies are used.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

EXPECTED = '4a8231dcc6ba717d61ab5783156c3a0d0367572ddb54537b0b283415da12fab8'

def convert(source, output):
    data = source.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED:
        raise ValueError('Input is not the approved Namco TCGUN001 asset')
    if data[:8] != b'TCGUN001':
        raise ValueError('Unsupported model format')
    nv, ni, width, height = struct.unpack_from('<4I', data, 8)
    muzzle = struct.unpack_from('<3f', data, 24)
    if ni % 3 or len(data) != 36 + nv*32 + ni*4 + width*height*4:
        raise ValueError('Invalid geometry or atlas size')
    verts = list(struct.iter_unpack('<8f', data[36:36+nv*32]))
    indices = struct.unpack_from(f'<{ni}I', data, 36+nv*32)
    if max(indices) >= nv or not all(math.isfinite(x) for vertex in verts for x in vertex):
        raise ValueError('Invalid vertex or triangle')
    atlas = data[36+nv*32+ni*4:]
    def sample(u,v):
        x = max(0,min(width-1,round(u*(width-1))))
        y = max(0,min(height-1,round(v*(height-1))))
        offset = (y*width+x)*4
        return atlas[offset:offset+3]
    faces, palette = [], {}
    for i in range(0,ni,3):
        face = indices[i:i+3]
        uv = [(verts[j][6],verts[j][7]) for j in face]
        # Sample inside each triangle (avoid neighbouring atlas islands at shared UV edges).
        samples = [sample(sum(p[0] for p in uv)/3,sum(p[1] for p in uv)/3)]
        for j in range(3):
            samples.append(sample(sum(uv[k][0]*(0.6 if k==j else 0.2) for k in range(3)),
                                  sum(uv[k][1]*(0.6 if k==j else 0.2) for k in range(3))))
        color = tuple(round(sum(p[c] for p in samples)/len(samples)/8)*8 for c in range(3))
        color = tuple(min(255,c) for c in color)
        if color not in palette:
            palette[color] = len(palette)
        faces.append((face,palette[color]))
    output.mkdir(parents=True,exist_ok=True)
    lines = ['# Namco Arcade Gun by Martoscar, CC-BY-4.0. See FrameArcadeGun-NOTICE.txt.',
             '# Metres; -Z bore; +Y up. Source bore translated onto the OpenXR aim ray.',
             'mtllib FrameArcadeGun.mtl']
    # Translation only: topology, scale, orientation, normals, and bore direction stay intact.
    for vertex in verts:
        lines.append('v '+' '.join(f'{vertex[j] - (muzzle[j] if j<2 else 0):.9g}' for j in range(3)))
    for vertex in verts:
        lines.append('vn '+' '.join(f'{x:.9g}' for x in vertex[3:6]))
    last=None
    for face,material in faces:
        if material!=last:
            lines.append(f'usemtl atlas{material}')
            last=material
        lines.append('f '+' '.join(f'{j+1}//{j+1}' for j in face))
    (output/'FrameArcadeGun.obj').write_text('\n'.join(lines)+'\n')
    materials=[]
    for color,number in palette.items():
        materials.extend([f'newmtl atlas{number}','Kd '+' '.join(f'{c/255:.6f}' for c in color),'Ks 0.20 0.20 0.20','Ns 32'])
    (output/'FrameArcadeGun.mtl').write_text('\n'.join(materials)+'\n')
    metadata={
        'name':'Namco Arcade Gun','author':'Martoscar','license':'CC-BY-4.0',
        'license_url':'https://creativecommons.org/licenses/by/4.0/',
        'source_url':'https://sketchfab.com/3d-models/namco-arcade-gun-15fbd5b9add94a34b5c21746e3dd32be',
        'original_glb_sha256':'989b25eaa2d22a25c424c6bb3d3830eda0a88724099019bf21fe2989d2fe1a02',
        'tc2_asset_sha256':digest,'vertices':nv,'triangles':ni//3,'materials':len(palette),
        'original_grip_m':[0,0,0],'original_muzzle_m':list(muzzle),
        'model_translation_m':[-muzzle[0],-muzzle[1],0],
        'aim_origin_m':[0,0,0],'muzzle_m':[0,0,muzzle[2]],'barrel_direction':[0,0,-1],
        'modifications':'TC2 normalized geometry retained; translated bore onto aim ray; atlas baked to per-triangle RGB materials with 8-level color quantization; existing lightweight lighting replaces PBR.',
        'files':{name:hashlib.sha256((output/name).read_bytes()).hexdigest() for name in ['FrameArcadeGun.obj','FrameArcadeGun.mtl']}}
    (output/'FrameArcadeGun.json').write_text(json.dumps(metadata,indent=2,sort_keys=True)+'\n')
    print(f'Converted {nv} vertices, {ni//3} triangles, {len(palette)} materials; bore at (0,0,{muzzle[2]:.6f}) m')

if __name__=='__main__':
    if len(sys.argv) not in (2,3):
        sys.exit(__doc__)
    convert(Path(sys.argv[1]),Path(sys.argv[2]) if len(sys.argv)==3 else Path(__file__).resolve().parents[2]/'Assets')
