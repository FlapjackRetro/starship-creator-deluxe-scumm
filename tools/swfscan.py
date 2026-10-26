import re,glob,struct,collections,sys
base="/tmp/claude-0/sv/game/Starship Creator/"
files=glob.glob(base+"ShipMed/*_fls.cxt")+glob.glob(base+"ShipMain/*.dxr")+glob.glob(base+"ShipMain/*.cxt")
tags=collections.Counter(); vers=collections.Counter(); n=0
names={0:'End',1:'ShowFrame',2:'DefineShape',3:'FreeChar',4:'PlaceObject',5:'RemoveObject',6:'DefineBits',7:'DefineButton',8:'JPEGTables',9:'SetBackgroundColor',10:'DefineFont',11:'DefineText',12:'DoAction',13:'DefineFontInfo',14:'DefineSound',15:'StartSound',17:'DefineButtonSound',18:'SoundStreamHead',19:'SoundStreamBlock',20:'DefineBitsLossless',21:'DefineBitsJPEG2',22:'DefineShape2',23:'DefineButtonCxform',24:'Protect',26:'PlaceObject2',28:'RemoveObject2',32:'DefineShape3',33:'DefineText2',34:'DefineButton2',39:'DefineSprite',43:'FrameLabel'}
for f in files:
    d=open(f,'rb').read()
    for m in re.finditer(b'FWS',d):
        o=m.start(); ver=d[o+3]; ln=struct.unpack('<I',d[o+4:o+8])[0]
        if ln<20 or ln>2_000_000 or o+ln>len(d)+8: continue
        vers[ver]+=1; n+=1
        p=o+8
        nb=d[p]>>3; p+=1+((5+nb*4+7)//8)-1
        p+=4  # frame rate+count
        end=o+ln
        while p<end:
            h=struct.unpack('<H',d[p:p+2])[0]; p+=2
            t=h>>6; l=h&63
            if l==63: l=struct.unpack('<I',d[p:p+4])[0]; p+=4
            tags[names.get(t,t)]+=1
            p+=l
            if t==0: break
print(n,vers); print(tags.most_common())
