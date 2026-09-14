#!/usr/bin/env python3
"""Render a LifelongTA debug trace as a standalone browser replay."""
import argparse
import json
from pathlib import Path

TEMPLATE = r'''<!doctype html><meta charset="utf-8"><title>LifelongTA Trace</title>
<style>body{margin:0;font:14px system-ui;background:#18212b;color:#edf2f7;display:grid;grid-template-columns:1fr 300px;height:100vh}main{padding:16px;overflow:auto}canvas{image-rendering:pixelated;background:#f8fafc;max-width:100%;border:1px solid #465466}aside{padding:18px;background:#111820;overflow:auto}input{width:100%}button{margin:8px 4px 8px 0}code{color:#a7f3d0}.event{padding:6px 0;border-bottom:1px solid #263443}</style>
<main><canvas id="map"></canvas><div><button id="play">Play</button><button id="prev">Prev</button><button id="next">Next</button><input id="time" type="range"></div></main>
<aside><h2 id="title"></h2><div id="stats"></div><h3>Agent</h3><input id="agent" type="number" min="0" value="0"><div id="detail"></div><h3>Assignment changes</h3><div id="events"></div></aside>
<script>const trace=__TRACE__;const frames=trace.frames,cv=document.querySelector('#map'),ctx=cv.getContext('2d'),slider=document.querySelector('#time'),agent=document.querySelector('#agent');let i=0,timer;
const scale=Math.max(4,Math.min(18,Math.floor(760/Math.max(trace.rows,trace.cols))));cv.width=trace.cols*scale;cv.height=trace.rows*scale;slider.max=frames.length-1;agent.max=trace.teamSize-1;document.querySelector('#title').textContent=`${trace.rows} x ${trace.cols} replay`;
function xy(loc){return [loc%trace.cols,Math.floor(loc/trace.cols)]}function paint(){const f=frames[i],a=+agent.value||0;ctx.fillStyle='#fff';ctx.fillRect(0,0,cv.width,cv.height);for(let n=0;n<trace.map.length;n++)if(trace.map[n]){let [x,y]=xy(n);ctx.fillStyle='#334155';ctx.fillRect(x*scale,y*scale,scale,scale)}for(const p of f.agents){let[x,y]=xy(p.location);ctx.fillStyle=p.id===a?'#dc2626':'#2563eb';ctx.fillRect(x*scale+1,y*scale+1,scale-2,scale-2);if(p.goal>=0){let[gx,gy]=xy(p.goal);ctx.strokeStyle='#16a34a';ctx.strokeRect(gx*scale+1,gy*scale+1,scale-2,scale-2)}}const p=f.agents.find(x=>x.id===a);document.querySelector('#stats').innerHTML=`<p>Step <b>${f.timestep}</b> (${i+1}/${frames.length})</p><p>Agents ${f.agents.length}; active tasks ${f.tasks.length}</p>`;document.querySelector('#detail').innerHTML=p?`<p><code>#${p.id}</code> location <code>${p.location}</code></p><p>task <code>${p.task}</code>, goal <code>${p.goal}</code>, action <code>${p.action}</code></p>`:'Not captured';document.querySelector('#events').innerHTML=f.assignmentChanges.map(e=>`<div class=event>agent ${e.agent}: ${e.before} &rarr; ${e.after}</div>`).join('')||'No changes';slider.value=i}
function step(d){i=Math.max(0,Math.min(frames.length-1,i+d));paint()}slider.oninput=()=>{i=+slider.value;paint()};agent.oninput=paint;document.querySelector('#prev').onclick=()=>step(-1);document.querySelector('#next').onclick=()=>step(1);document.querySelector('#play').onclick=()=>{if(timer){clearInterval(timer);timer=null;play.textContent='Play'}else{timer=setInterval(()=>{if(i===frames.length-1)step(-i);else step(1)},180);play.textContent='Pause'}};paint();</script>'''

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('trace', type=Path)
    parser.add_argument('-o','--output', type=Path)
    args=parser.parse_args()
    trace=json.loads(args.trace.read_text(encoding='utf-8'))
    output=args.output or args.trace.with_suffix('.html')
    output.write_text(TEMPLATE.replace('__TRACE__',json.dumps(trace,separators=(',',':'))),encoding='utf-8')
    print(output)
if __name__=='__main__': main()
