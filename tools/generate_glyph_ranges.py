import pathlib, re
root=pathlib.Path(__file__).resolve().parents[1]
texts=[]
for p in [root/'src'/'entry.cpp', root/'src'/'events_generated.h']:
    texts.append(p.read_text(encoding='utf-8'))
chars=sorted({ord(ch) for text in texts for ch in text if ord(ch)>=0x80 and ord(ch)<=0xFFFF})
# Add ASCII/Latin range because this font is pushed for the whole window.
points=list(range(0x20,0x100))+chars
points=sorted(set(points))
ranges=[]
start=prev=points[0]
for cp in points[1:]:
    if cp==prev+1:
        prev=cp
    else:
        ranges.append((start,prev)); start=prev=cp
ranges.append((start,prev))
out=['#pragma once','#include <imgui.h>','', 'static const ImWchar kChineseTimerGlyphRanges[] = {']
for a,b in ranges:
    out.append(f'    0x{a:04X}, 0x{b:04X},')
out += ['    0,','};','']
(root/'src'/'glyph_ranges_generated.h').write_text('\n'.join(out),encoding='utf-8')
print(f'{len(points)} codepoints, {len(ranges)} ranges')
