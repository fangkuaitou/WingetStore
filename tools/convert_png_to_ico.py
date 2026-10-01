# -*- coding: utf-8 -*-
# 把用户提供的 PNG 转成多尺寸 ICO（无第三方依赖）
import struct, zlib, math, sys

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
            assert bitdepth == 8 and colortype == 2, "只支持 8-bit RGB PNG"
        elif t == b"IDAT":
            idat += chunk
        elif t == b"IEND":
            break
        i += 12 + l
    raw = zlib.decompress(idat)
    bpp = 3  # RGB
    stride = width * bpp
    pixels = []
    for y in range(height):
        filter_byte = raw[y * (stride + 1)]
        row = list(raw[y * (stride + 1) + 1 : y * (stride + 1) + 1 + stride])
        if filter_byte == 0:
            pass
        elif filter_byte == 1:
            for x in range(bpp, len(row)):
                row[x] = (row[x] + row[x - bpp]) & 0xFF
        elif filter_byte == 2:
            if y > 0:
                for x in range(stride):
                    row[x] = (row[x] + pixels[y-1][x]) & 0xFF
        elif filter_byte == 3:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = pixels[y-1][x] if y > 0 else 0
                row[x] = (row[x] + (a + b) // 2) & 0xFF
        elif filter_byte == 4:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = pixels[y-1][x] if y > 0 else 0
                c = pixels[y-1][x - bpp] if y > 0 and x >= bpp else 0
                p = a + b - c
                pa = abs(p - a); pb = abs(p - b); pc = abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                row[x] = (row[x] + pr) & 0xFF
        else:
            raise ValueError("unknown filter " + str(filter_byte))
        pixels.append(row)
    rgba = []
    for y in range(height):
        row = []
        for x in range(width):
            row.append((pixels[y][x*3], pixels[y][x*3+1], pixels[y][x*3+2], 255))
        rgba.append(row)
    return rgba, width, height

def scale(src, src_w, src_h, dst_w, dst_h):
    sx = src_w / dst_w
    sy = src_h / dst_h
    dst = [[(0,0,0,0) for _ in range(dst_w)] for _ in range(dst_h)]
    for y in range(dst_h):
        for x in range(dst_w):
            x0 = int(x * sx)
            x1 = int((x + 1) * sx)
            y0 = int(y * sy)
            y1 = int((y + 1) * sy)
            r=g=b=a=n=0
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    if 0 <= yy < src_h and 0 <= xx < src_w:
                        p = src[yy][xx]
                        r+=p[0]; g+=p[1]; b+=p[2]; a+=p[3]; n+=1
            if n:
                dst[y][x] = (r//n, g//n, b//n, a//n)
    return dst

def encode_png(rgba, size):
    # 手写 PNG 编码（RGBA8，filter 0）
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            r, g, b, a = rgba[y][x]
            raw += bytes((r, g, b, a))

    def chunk(t, d):
        c = t + d
        return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def write_ico(path, src, src_w, src_h, sizes=[16, 32, 48]):
    count = len(sizes)
    header = struct.pack("<HHH", 0, 1, count)
    offset = 6 + 16 * count
    dir_entries = b""
    data = b""
    for s in sizes:
        img = scale(src, src_w, src_h, s, s)
        if s >= 256:
            # 256x256 用 PNG 压缩存储（Vista+ 支持，体积小很多）
            img_data = encode_png(img, s)
            dir_entries += struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(img_data), offset)
            data += img_data
            offset += len(img_data)
            continue
        bi = struct.pack("<IiiHHIIiiII", 40, s, s*2, 1, 32, 0, s*s*4, 0, 0, 0, 0)
        xor_bgra = b""
        and_mask = b""
        row_bytes = ((s + 31) // 32) * 4
        for y in range(s-1, -1, -1):
            row = b""
            bits = 0
            bitcount = 0
            bytecount = 0
            for x in range(s):
                p = img[y][x]
                row += bytes((p[2], p[1], p[0], p[3]))
                bit = 1 if p[3] < 128 else 0
                bits = (bits << 1) | bit
                bitcount += 1
                if bitcount == 8:
                    and_mask += bytes((bits,))
                    bits = 0
                    bitcount = 0
                    bytecount += 1
            if bitcount:
                bits <<= (8 - bitcount)
                and_mask += bytes((bits,))
                bytecount += 1
            while bytecount < row_bytes:
                and_mask += b"\x00"
                bytecount += 1
            xor_bgra += row
        img_data = bi + xor_bgra + and_mask
        sz = len(img_data)
        dir_entries += struct.pack("<BBBBHHII", s if s < 256 else 0, s if s < 256 else 0, 0, 0, 1, 32, sz, offset)
        data += img_data
        offset += sz
    open(path, "wb").write(header + dir_entries + data)

src, w, h = decode_png(r"E:\F\新建项目 (1).png")
write_ico("icon.ico", src, w, h)
print("icon.ico generated from user PNG, sizes=16/32/48")
