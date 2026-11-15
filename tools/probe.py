import numpy as np
from pedalboard import load_plugin
P=load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3'))
print('loaded', P.name, 'instrument', P.is_instrument, 'params', len(P.parameters))
print(list(P.parameters.keys()))
SR=44100
out=P([(bytes([0x90,60,110]),0.0),(bytes([0x80,60,0]),1.5)],duration=2.0,sample_rate=SR,num_channels=2)
x=out.mean(axis=0); print('peak',float(np.abs(x).max()),'rms',float(np.sqrt((x[SR//2:SR]**2).mean())))
