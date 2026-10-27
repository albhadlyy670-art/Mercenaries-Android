import os, struct, sys, hashlib, random
SECT=2048
def build(tree, part_off, out):
    # tree: dict name -> bytes | dict.  Directory = BST by case-insensitive name (balanced via sorted median).
    data=[]  # (sector, bytes)
    cursor=[34]  # first free sector after descriptor (32) + 1 spare
    def alloc(nbytes):
        s=cursor[0]; cursor[0]+= (nbytes+SECT-1)//SECT or 1; return s
    def layout_dir(d):
        names=sorted(d.keys(), key=lambda n:n.upper())
        # assign children first
        info={}
        for n in names:
            v=d[n]
            if isinstance(v,dict):
                s,sz=layout_dir(v); info[n]=(s,sz,0x10)
            else:
                s=alloc(len(v)); data.append((s,v)); info[n]=(s,len(v),0x20)
        # build balanced BST over names, compute offsets
        def entry_len(n): return (14+len(n.encode())+3)&~3
        order=[]
        def bst(lo,hi):
            if lo>hi: return None
            m=(lo+hi)//2
            return (m,bst(lo,m-1),bst(m+1,hi))
        root=bst(0,len(names)-1)
        # place entries in preorder, never straddling a sector
        pos=0; offs={}
        def place(node):
            nonlocal pos
            if node is None: return
            m,l,r=node
            el=entry_len(names[m])
            if (pos%SECT)+el>SECT: pos=(pos//SECT+1)*SECT
            offs[m]=pos; pos+=el
            place(l); place(r)
        place(root)
        size=((pos+SECT-1)//SECT)*SECT or SECT
        buf=bytearray(b'\xff'*size)
        def emit(node):
            if node is None: return 0
            m,l,r=node
            lo=emit(l); ro=emit(r)
            n=names[m]; s,sz,at=info[n]
            e=struct.pack('<HHIIBB',lo,ro,s,sz,at,len(n.encode()))+n.encode()
            e+=b'\xff'*(entry_len(n)-len(e))
            buf[offs[m]:offs[m]+len(e)]=e
            return offs[m]//4
        emit(root)
        s=alloc(size); data.append((s,bytes(buf)))
        return s,size
    rs,rsz=layout_dir(tree)
    vd=bytearray(SECT)
    vd[0:20]=b'MICROSOFT*XBOX*MEDIA'
    struct.pack_into('<II',vd,0x14,rs,rsz)
    vd[0x7EC:0x7EC+20]=b'MICROSOFT*XBOX*MEDIA'
    data.append((32,bytes(vd)))
    total=part_off+cursor[0]*SECT
    with open(out,'wb') as f:
        f.truncate(total)
        for s,b in data:
            f.seek(part_off+s*SECT); f.write(b)
random.seed(7)
big=bytes(random.getrandbits(8) for _ in range(5*1024*1024+123))
tree={
 'default.xbe': b'XBEH'+bytes(range(200))*50,
 'Media': {'intro.xmv': b'\x01\x02'*100000, 'Audio': {'a.xwb': b'WAVE'*4096, 'b.xwb':b'', 'Z.bin':b'z'}},
 'Data': {'world.big': big, 'x.txt': b'hello'},
 'zzz.cfg': b'cfg', 'AAA.cfg': b'a',
}
for i in range(60): tree['Data']['f%02d.dat'%i]=bytes([i])*(i*97)
build(tree, int(sys.argv[2],0), sys.argv[1])
def flat(d,p=''):
    for k,v in d.items():
        if isinstance(v,dict): yield from flat(v,p+k+'/')
        else: yield (p+k).lower(), hashlib.sha256(v).hexdigest()   # the importer lower-cases every name
with open(sys.argv[1]+'.manifest','w') as f:
    for p,h in sorted(flat(tree)): f.write(h+'  '+p+'\n')
