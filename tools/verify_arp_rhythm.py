"""Direct event harness plus plugin audio/state checks for the original rhythm lane."""
from pathlib import Path
import struct, subprocess, sys, tempfile
import xml.etree.ElementTree as ET
import numpy as np
from pedalboard import load_plugin
from state_tools import edit_state, unwrap, rewrap
ROOT = Path(__file__).resolve().parents[1]
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
SR = 44100
rows = []
def check(name, ok):
    rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name}", flush=True)
with tempfile.TemporaryDirectory(prefix='relic-rhythm-') as tmp:
    binary = str(Path(tmp) / 'rhythm')
    subprocess.run(['clang++','-std=c++17','-O1',str(ROOT / 'tools/dsp_arp_rhythm_test.cpp'),'-o',binary],check=True)
    result = subprocess.run([binary],capture_output=True,text=True)
    print(result.stdout,flush=True)
    check('direct engine harness',result.returncode == 0)

def plugin(types=(0,), velocity=1, enabled=True, **changes):
    p = load_plugin(PATH)
    values = dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=0,
                  arp_order=0, arp_grouping=0, arp_octaves=1, arp_gate=.5,
                  arp_swing=0, arp_human=0, chord_on=0, waveform=0,
                  filter_on=0, synth_mix=1, master=1, osc_level=.3,
                  unison_voices=1, voices=16, attack_ms=.5, decay_ms=0,
                  sustain=1, release_ms=6, arp_rhythm_on=int(enabled),
                  arp_rhythm_length=len(types))
    for i in range(1,17):
        values[f'arp_rhythm_{i}_type'] = types[(i-1) % len(types)]
        values[f'arp_rhythm_{i}_velocity'] = velocity
    values.update(changes)
    p.raw_state = edit_state(p.raw_state,values)
    return p

def render(p):
    ev=[(bytes([0x80,n,0]),i*.00001) for i,n in enumerate(range(128))]
    ev += [(bytes([0x90,n,100]),.75+i*.00001) for i,n in enumerate([60,64,67])]
    ev += [(bytes([0x80,n,0]),3.2+i*.00001) for i,n in enumerate([60,64,67])]
    return p(ev,duration=3.5,sample_rate=SR,num_channels=2).mean(axis=0)

def rms(x,a,b): return float(np.sqrt(np.mean(x[int(a*SR):int(b*SR)]**2)))
def onsets(x):
    e=np.sqrt(np.convolve(x*x,np.ones(176)/176,'same')); threshold=e.max()*.2
    if threshold < 1e-8: return []
    out=[]
    for i in np.where((e[1:]>threshold)&(e[:-1]<=threshold))[0]+1:
        t=float(i/SR)
        if t>.7 and (not out or t-out[-1]>.05): out.append(t)
    return out

def pitches(x,times):
    result=[]; freq=np.fft.rfftfreq(16384,1/SR)
    for t in times:
        signal=x[int((t+.02)*SR):int((t+.07)*SR)]
        spec=np.abs(np.fft.rfft(signal*np.hanning(len(signal)),16384))
        result.append(max([60,64,67],key=lambda n:spec[np.argmin(abs(freq-440*2**((n-69)/12)))]))
    return result

def values(raw):
    _,c,n=unwrap(bytes(raw)); root=ET.fromstring(c[8:8+n].rstrip(b'\0'))
    return {e.get('id'):float(e.get('value')) for e in root.findall('PARAM') if e.get('id','').startswith('arp_rhythm_')}

p=load_plugin(PATH); names=p.parameters
check('host exposes rhythm enable, length, cell types and velocities',all(k in names for k in ['arp_rhythm_on','arp_rhythm_length','arp_rhythm_1_type','arp_rhythm_16_velocity']))
v=values(p.raw_state)
check('new instances default to a disabled 16-cell all-hit lane',v['arp_rhythm_on']==0 and v['arp_rhythm_length']==16 and all(v[f'arp_rhythm_{i}_type']==0 and v[f'arp_rhythm_{i}_velocity']==1 for i in range(1,17)))
full=render(plugin()); bypass=render(plugin(enabled=False))
check('all hits at full velocity reproduce lane bypass audio',np.allclose(full,bypass,atol=1e-7))
x=render(plugin((0,1)))
check('rests silence steps without skipping pattern notes',pitches(x,onsets(x))==[60,64,67,60,64])
x=render(plugin((0,2,1)))
check('ties sustain without retrigger and use the final tied cell gate',len(onsets(x))==4 and rms(x,1.06,1.1)>.01 and rms(x,1.20,1.24)<1e-7)
x=render(plugin(velocity=.5))
ratio=rms(x,.82,.86)/rms(full,.82,.86)
check('half velocity produces half the sine amplitude',abs(ratio-.5)<.025)
x=render(plugin(velocity=0))
check('zero velocity produces a six-percent hit and consumes pitches',abs(rms(x,.82,.86)/rms(full,.82,.86)-.06)<.005 and len(onsets(x))==len(onsets(full)))
x=render(plugin((0,2,2),arp_run=0))
check('STOP plays all three hits and carries final ties to the boundary',len(onsets(x))==3 and rms(x,2.9,2.95)>.01 and rms(x,3.1,3.18)<1e-7)
x=render(plugin((0,1),arp_run=0))
check('STOP completes the full pitch pass through rests',pitches(x,onsets(x))==[60,64,67] and rms(x,2.1,3.1)<1e-7)
x=render(plugin((1,),arp_on=0))
check('ARP bypass ignores an all-rest lane',rms(x,1.5,2)>.01)
p=plugin((0,1,2),velocity=.37); saved=bytes(p.raw_state); q=plugin(); q.raw_state=saved
check('all lane parameters survive state round trip',values(saved)==values(q.raw_state))
_,c,n=unwrap(saved); root=ET.fromstring(c[8:8+n].rstrip(b'\0'))
for e in list(root):
    if e.get('id','').startswith('arp_rhythm_'): root.remove(e)
inner=ET.tostring(root)+b'\0'; q.raw_state=rewrap(saved,b'VC2!'+struct.pack('<I',len(inner))+inner+c[8+n:])
v=values(q.raw_state)
check('old project over an edited lane restores disabled all-hit defaults',v['arp_rhythm_on']==0 and v['arp_rhythm_length']==16 and all(v[f'arp_rhythm_{i}_type']==0 and v[f'arp_rhythm_{i}_velocity']==1 for i in range(1,17)))
print(f'RESULT: {sum(rows)} of {len(rows)} arp rhythm groups/checks passed')
sys.exit(0 if all(rows) else 1)
