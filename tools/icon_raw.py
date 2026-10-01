# -*- coding: utf-8 -*-
# 把用户 PNG 转成单个 48x48 图标资源（BITMAPINFOHEADER + XOR + AND），供 CreateIconFromResourceEx 使用
import struct, zlib

def decode_png(path):
    f = open(path, "rb")
    data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    i = 8
    width = height = bitdepth = colortype = None
    idat = b""
    while i < len(data):
        l = struct.unpack(">I", data[i:i+4])[0]
        t = data[i+4:i+8]
        chunk = data[i+8:i+8+l]
        if t == b"IHDR":
            width, height, bitdepth, colortype, cm, fm, it = struct.unpack(">IIBBBBB", chunk)
            assert bitdepth == 8 and colortype == 2
        elif t == b"IDAT":
            idat += chunk
        elif t == b"IEND":
            break
        i += 12 + l
    raw = zlib.decompress(idat)
    bpp = 3
    stride = width * bpp
    pixels = []
    for y in range(height):
        fbyte = raw[y * (stride + 1)]
        row = list(raw[y * (stride + 1) + 1 : y * (stride + 1) + 1 + stride])
        if fbyte == 0: pass
        elif fbyte == 1:
            for x in range(bpp, len(row)): row[x] = (row[x] + row[x - bpp]) & 0xFF
        elif fbyte == 2:
            if y > 0:
                for x in range(stride): row[x] = (row[x] + pixels[y-1][x]) & 0xFF
        elif fbyte == 3:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = pixels[y-1][x] if y > 0 else 0
                row[x] = (row[x] + (a + b) // 2) & 0xFF
        elif fbyte == 4:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = pixels[y-1][x] if y > 0 else 0
                c = pixels[y-1][x - bpp] if y > 0 and x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                row[x] = (row[x] + pr) & 0xFF
        else: raise ValueError("filter " + str(fbyte))
        pixels.append(row)
    rgba = []
    for y in range(height):
        rgba.append([(pixels[y][x*3], pixels[y][x*3+1], pixels[y][x*3+2], 255) for x in range(width)])
    return rgba, width, height

def scale(src, sw, sh, dw, dh):
    sx, sy = sw/dw, sh/dh
    dst = [[(0,0,0,0) for _ in range(dw)] for _ in range(dh)]
    for y in range(dh):
        for x in range(dw):
            x0, x1 = int(x*sx), int((x+1)*sx)
            y0, y1 = int(y*sy), int((y+1)*sy)
            r=g=b=a=n=0
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    if 0 <= yy < sh and 0 <= xx < sw:
                        p = src[yy][xx]
                        r+=p[0]; g+=p[1]; b+=p[2]; a+=p[3]; n+=1
            if n: dst[y][x] = (r//n, g//n, b//n, a//n)
    return dst

def write_icon_resource(path, src, sw, sh, size=48):
    img = scale(src, sw, sh, size, size)
    # BITMAPINFOHEADER
    out = struct.pack("<IiiHHIIiiII", 40, size, size*2, 1, 32, 0, size*size*4, 0, 0, 0, 0)
    # XOR mask (BGRA, bottom-up)
    and_mask = bytearray()
    row_bytes = ((size + 31) // 32) * 4
    for y in range(size-1, -1, -1):
        for x in range(size):
            r, g, b, a = img[y][x]
            out += bytes((b, g, r, a))
        bits = 0
        bc = 0
        bytecount = 0
        for x in range(size):
            bit = 1 if img[y][x][3] < 128 else 0
            bits = (bits << 1) | bit
            bc += 1
            if bc == 8:
                and_mask.append(bits)
                bits = 0
                bc = 0
                bytecount += 1
        if bc:
            bits <<= (8 - bc)
            and_mask.append(bits)
            bytecount += 1
        while bytecount < row_bytes:
            and_mask.append(0)
            bytecount += 1
    out += bytes(and_mask)
    # C array
    lines = ["    " + ",".join(str(b) for b in out[i:i+16]) + "," for i in range(0, len(out), 16)]
    open(path, "w").write("static const unsigned char ICON_DATA[] = {\n" + "\n".join(lines) + "\n};\nstatic const int ICON_SIZE = sizeof(ICON_DATA);\n")
    print(f"icon_data.c generated ({size}x{size}), {len(out)} bytes")

src, w, h = decode_png(r"E:\F\新建项目 (1).png")
write_icon_resource("icon_data.c", src, w, h, 48)
