import json, pathlib
root=pathlib.Path(__file__).resolve().parents[1]
data=json.loads((root/'event_tracks_zh.json').read_text(encoding='utf-8'))
out=root/'src'/'events_generated.h'
rows=[]
track_index=0
for cat in data.get('categories',[]):
    cname=cat.get('name','')
    for tr in cat.get('tracks',[]):
        tname=tr.get('name','')
        calc=tr.get('base_time_calculator','local_day_start')
        visible=bool(tr.get('visible',True))
        height=float(tr.get('height',20))
        for s in tr.get('schedules',[]):
            color=s.get('color',[0.4,0.7,0.9,1.0])
            color=(color+[1,1,1,1])[:4]
            rows.append((track_index,cname,tname,calc,visible,height,s.get('name',''),int(s.get('offset',0)),int(s.get('interval',0)),int(s.get('duration',0)),*map(float,color)))
        track_index += 1

def esc(s):
    return s.replace('\\','\\\\').replace('"','\\"')
lines=['#pragma once','#include <cstddef>','',
'''struct EventSchedule {\n    int track_index;\n    const char* category;\n    const char* track;\n    const char* calculator;\n    bool visible;\n    float height;\n    const char* name;\n    int offset_minutes;\n    int interval_minutes;\n    int duration_minutes;\n    float r,g,b,a;\n};''','', 'static constexpr EventSchedule kEventSchedules[] = {']
for r in rows:
    ti,c,t,calc,vis,h,n,o,i,d,rr,gg,bb,aa=r
    lines.append(f'    {{{ti}, "{esc(c)}", "{esc(t)}", "{esc(calc)}", {str(vis).lower()}, {h:.1f}f, "{esc(n)}", {o}, {i}, {d}, {rr:.6f}f, {gg:.6f}f, {bb:.6f}f, {aa:.6f}f}},')
lines += ['};', 'static constexpr std::size_t kEventScheduleCount = sizeof(kEventSchedules)/sizeof(kEventSchedules[0]);','']
out.write_text('\n'.join(lines),encoding='utf-8')
print(f'wrote {len(rows)} schedules / {track_index} tracks')
