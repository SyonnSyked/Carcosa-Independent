"""Convert the supplied CC0 Quaternius axe OBJ to H2B (standard library only)."""
import math
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/source/throwing-axe'
positions, normals, groups = [], [], {}
material = None
for line in (SOURCE / 'Axe.obj').read_text().splitlines():
    parts = line.split()
    if not parts or parts[0].startswith('#'):
        continue
    kind, values = parts[0], parts[1:]
    if kind == 'v':
        positions.append(tuple(map(float, values[:3])))
    elif kind == 'vn':
        normals.append(tuple(map(float, values[:3])))
    elif kind == 'usemtl':
        material = ' '.join(values)
        groups.setdefault(material, [])
    elif kind == 'f':
        groups[material].append([tuple(int(v) if v else 0 for v in item.split('/')) for item in values])
materials = {}
for line in (SOURCE / 'Axe.mtl').read_text().splitlines():
    parts = line.split()
    if not parts or parts[0].startswith('#'):
        continue
    if parts[0] == 'newmtl':
        current = materials[' '.join(parts[1:])] = {}
    else:
        current[parts[0]] = list(map(float, parts[1:]))
minimum = [min(p[i] for p in positions) for i in range(3)]
maximum = [max(p[i] for p in positions) for i in range(3)]
center = [(lo+hi)/2 for lo,hi in zip(minimum,maximum)]
scale = 1.2 / max(hi-lo for lo,hi in zip(minimum,maximum))
def rotate(p):
    # Rotate 90 degrees about X so the axe lies flat in the X/Z ground plane.
    return p[0], -p[2], p[1]
def resolve(items,index):
    if index == 0:
        raise ValueError('OBJ indices must be nonzero')
    return items[index-1 if index>0 else index]
vertices, indices, batches = [], [], []
for name, faces in groups.items():
    first_index = len(indices)
    for face in faces:
        points = [resolve(positions,item[0]) for item in face]
        u = [points[1][i]-points[0][i] for i in range(3)]
        v = [points[2][i]-points[0][i] for i in range(3)]
        normal = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        length = math.sqrt(sum(n*n for n in normal))
        if length == 0:
            raise ValueError('Degenerate face')
        first_vertex = len(vertices)
        for item, point in zip(face,points):
            if len(item)>1 and item[1]:
                raise ValueError('This source uses material colors, not texture coordinates')
            position = rotate(tuple((point[i]-center[i])*scale for i in range(3)))
            n = resolve(normals,item[2]) if len(item)>2 and item[2] else tuple(n/length for n in normal)
            vertices.append((*position,0,0,0,*rotate(n)))
        for i in range(1,len(face)-1):
            indices.extend((first_vertex,first_vertex+i,first_vertex+i+1))
    batches.append((len(indices)-first_index,first_index))
output = ROOT / 'assets/models/ThrowingAxe.h2b'
with output.open('wb') as out:
    out.write(struct.pack('<4s4I',b'019d',len(vertices),len(indices),len(groups),len(groups)))
    for vertex in vertices:
        out.write(struct.pack('<9f',*vertex))
    out.write(struct.pack(f'<{len(indices)}I',*indices))
    for name in groups:
        m = materials[name]
        out.write(struct.pack('<19fI',*m['Kd'],m.get('d',[1])[0],*m['Ks'],m['Ns'][0],*m['Ka'],0,0,0,0,m['Ni'][0],*m['Ke'],int(m['illum'][0])))
        out.write(name.encode()+b'\0'+b'\0'*9)
    for batch in batches:
        out.write(struct.pack('<2I',*batch))
    for i,(name,batch) in enumerate(zip(groups,batches)):
        out.write(name.encode()+b'\0'+struct.pack('<3I',*batch,i))
print(f'Converted {output.name}: {len(vertices)} vertices, {len(indices)//3} triangles, {len(groups)} materials')
print('Bounds:',[min(v[i] for v in vertices) for i in range(3)],[max(v[i] for v in vertices) for i in range(3)])
