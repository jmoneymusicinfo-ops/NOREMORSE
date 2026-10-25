"""Generate original factory data and matching compiled JSON. No external models."""
import json,math,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
names=['J Money — Natural Rap Lead','Bay Area — Forward and Clear','Deep Voice — Keep the Body','Bedroom Recording — Mud Control','Harsh Vocal — Smooth Presence','Sibilant Vocal — Controlled S','Thin Vocal — Gentle Warmth','Melodic Rap — Smooth and Airy','Fast Rap — Word Clarity','Rap Doubles — Behind the Lead','Ad-Libs — Bright and Tucked','Vocal Bus — Gentle Glue EQ','Beat — Make Room for Lead','Conservative — Cleanup Only']
# Rumble,mud,nasal,presence,harshness,sibilance,air,body. Each macro maps to documented real filters.
macros=[ [.25,.25,.1,.25,.2,.2,.1,.15],[.25,.3,.15,.5,.3,.2,.1,.1],[.1,.2,.1,.2,.15,.2,0,.4],[.3,.65,.2,.2,.2,.2,0,.15],[.2,.2,.1,.1,.7,.35,0,.2],[.2,.2,.1,.15,.2,.75,0,.2],[.1,.1,.1,.1,.1,.2,0,.7],[.2,.2,.1,.15,.35,.3,.35,.15],[.2,.3,.25,.45,.35,.3,.1,.15],[.4,.3,.1,0,.25,.25,.1,0],[.5,.4,.2,.25,.2,.3,.5,0],[.1,.1,.05,.1,.15,.15,.1,.1],[0,0,0,0,0,0,0,0],[.15,.1,0,0,.1,.1,0,.1] ]
result=[]
for n,(name,values) in enumerate(zip(names,macros)):
 p=dict(zip(['rumble','mud','nasal','presence','harshness','sibilance','air','body'],values))
 p.update(learn_amount=.25 if n!=12 else 0,learn_max_cut=3,learn_max_boost=1,learn_low=90 if n!=2 else 65,learn_high=12000,learn_smoothing=1,group_tier=2 if n in [9,10,12] else 0,correction_mode=2 if n==12 else 0,group_max_cut=2)
 # Conservative manually enabled HP only for profiles where requested, cutoff never imposed on every voice.
 p.update(band1_enabled=n not in [2,6,11,12],band1_type=3,band1_frequency=[55,65,45,65,55,55,40,55,60,90,110,35,25,45][n],band1_q=.707)
 target=[]
 for i in range(32):
  f=60*(16000/60)**(i/31)
  body=(1.5 if n in [2,6] else .5)*math.exp(-.5*(math.log2(f/160)/.8)**2)
  presence=(.8 if n in [1,8] else .2)*math.exp(-.5*(math.log2(f/2600)/.8)**2)
  target.append(round(-3*math.log2(f/500)+body+presence,4))
 avg=sum(target)/32;target=[round(x-avg,4) for x in target]
 result.append(dict(name=name,parameters=p,target_db=target,intended_use='Grouped beat: requires foreground lead instance; EQ only.' if n==12 else 'EQ starting point; adjust to recording. No compression.'))
(root/'presets/factory.json').write_text(json.dumps(result,ensure_ascii=False,indent=2))
(root/'Source/FactoryData.h').write_text('#pragma once\ninline constexpr const char* factoryJson=R"JM('+json.dumps(result,ensure_ascii=False)+')JM";\n')
