import sys, json, struct
fn=sys.argv[1]
f=open(fn,'rb'); magic,ver,length=struct.unpack('<4sII',f.read(12))
clen,ctype=struct.unpack('<I4s',f.read(8)); js=json.loads(f.read(clen)); 
blen,btype=struct.unpack('<I4s',f.read(8))
print('file',fn.split('/')[-1],'len',length,'json',clen,'bin',blen)
print('asset',js.get('asset')); print('scenes',len(js.get('scenes',[])),'nodes',len(js['nodes']),'meshes',len(js['meshes']),'skins',len(js.get('skins',[])),'anims',len(js.get('animations',[])),'materials',len(js.get('materials',[])),'images',len(js.get('images',[])),'textures',len(js.get('textures',[])))
acc=js['accessors']; bv=js['bufferViews']
for mi,m in enumerate(js['meshes']):
    for p in m['primitives']:
        a=p['attributes']; print(' mesh',mi,m.get('name'),'verts',acc[a['POSITION']]['count'],'idx',acc[p['indices']]['count'] if 'indices' in p else None,'attrs',list(a.keys()),'mat',p.get('material'),'min',acc[a['POSITION']].get('min'),'max',acc[a['POSITION']].get('max'))
for i,im in enumerate(js.get('images',[])):
    v=bv[im['bufferView']] if 'bufferView' in im else None; print(' image',i,im.get('mimeType'),im.get('name'),v and v['byteLength'])
for i,m in enumerate(js.get('materials',[])): print(' material',i,json.dumps(m)[:300])
for i,s in enumerate(js.get('skins',[])): print(' skin',i,'joints',len(s['joints']),'skeleton',s.get('skeleton'))
if js.get('skins'):
    names=[js['nodes'][j].get('name') for j in js['skins'][0]['joints']]; print(' joints:',names)
for i,a in enumerate(js.get('animations',[])):
    print(' anim',i,a.get('name'),'channels',len(a['channels']),'samplers',len(a['samplers']), 'dur', max(acc[s['input']]['max'][0] for s in a['samplers']), 'frames', acc[a['samplers'][0]['input']]['count'])
# node tree
def tree(i,d=0):
    n=js['nodes'][i]; print('  '*d+str(i),n.get('name'),'mesh' if 'mesh' in n else '','skin' if 'skin' in n else '', 'T',[round(x,3) for x in n.get('translation',[])],'R',[round(x,3) for x in n.get('rotation',[])],'S',n.get('scale'))
    for c in n.get('children',[]): tree(c,d+1)
if len(sys.argv)>2:
    for r in js['scenes'][0]['nodes']: tree(r)
