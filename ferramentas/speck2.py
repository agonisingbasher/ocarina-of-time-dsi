import struct, zlib, sys
def read_png(path):
    d=open(path,'rb').read(); i=8; idat=b''
    W=H=ct=0; plte=None
    while i<len(d):
        ln=struct.unpack('>I',d[i:i+4])[0]; tag=d[i+4:i+8]; data=d[i+8:i+8+ln]; i+=12+ln
        if tag==b'IHDR': W,H,bit,ct=struct.unpack('>IIBB',data[:10])
        elif tag==b'IDAT': idat+=data
        elif tag==b'PLTE': plte=data
    raw=zlib.decompress(idat); ch=3 if ct==2 else 4 if ct==6 else 1
    stride=W*ch; rows=[]; prev=bytearray(stride); o=0
    for y in range(H):
        f=raw[o]; o+=1; line=bytearray(raw[o:o+stride]); o+=stride
        for x in range(stride):
            a=line[x-ch] if x>=ch else 0; b=prev[x]; c=prev[x-ch] if x>=ch else 0
            if f==1: line[x]=(line[x]+a)&255
            elif f==2: line[x]=(line[x]+b)&255
            elif f==3: line[x]=(line[x]+(a+b)//2)&255
            elif f==4:
                p=a+b-c; pa=abs(p-a); pb=abs(p-b); pc=abs(p-c)
                line[x]=(line[x]+(a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)))&255
        prev=line; rows.append(line)
    return W,H,ct,rows,plte
def lum(l,i,ch):
    if ch==3: return (l[i]*30+l[i+1]*59+l[i+2]*11)//100
    return l[i]
W,H,ct,rows,plte=read_png(sys.argv[1])
ch=3 if ct==2 else 4 if ct==6 else 1
# the capture is a 2x nearest upscale of the 256x192 panel: dedupe
gr=[rows[y] for y in range(0,H,2)]
W2=W
d=0.0; n=0; mx=0
for y in range(len(gr)):
    l=gr[y]
    for x in range(W2-1):
        a=lum(l,x*ch,ch); b=lum(l,(x+1)*ch,ch)
        if a<8 and b<8: continue
        d+=abs(a-b); n+=1
        if abs(a-b)>mx: mx=abs(a-b)
v=abs(d/n)
print("%s native %dx%d  mean|dLum| %.2f  max %d  pairs %d"%(sys.argv[1],W2,len(gr),v,mx,n))
