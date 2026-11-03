# Freesound CC0 후보 수집: SFX 당 여러 검색어 × 2쪽, 중복 제거, 메타데이터 저장
import json, re, sys, html, urllib.request, urllib.parse
# 방향: 묵직한 SF 임팩트 (사용자 선택). 이전에 쓴 소리는 모두 제외
Q={
 "ui_hover":[0.35,["sci-fi ui hover","futuristic interface beep","hud blip","sci-fi button","interface low beep","digital blip short"]],
 "ui_confirm":[0.7,["sci-fi ui confirm","futuristic button select","hud accept","sci-fi interface select","power on beep","ui deep confirm","ui confirm","button select","menu select","interface accept","confirm beep"]],
 "ui_back":[0.7,["sci-fi ui back","futuristic cancel","hud close","interface power down short","sci-fi deselect","ui low cancel","ui back","menu back","cancel button","ui close","deselect","back button"]],
 "ui_deny":[0.7,["sci-fi error","futuristic denied","hud error buzz","access denied sci-fi","error low buzz","alarm short"]],
 "aug_flip":[0.6,["sci-fi swoosh short","hologram swipe","futuristic swipe","data transfer short","sci-fi slide","interface whoosh","swipe","whoosh short","card flip","slide","swish"]],
 "sfx_shoot":[0.4,["sci-fi gun heavy","laser blast heavy","plasma cannon","energy weapon shot","blaster heavy","pulse rifle","laser shot","blaster","sci-fi shot","laser","gun sci fi"]],
 "enemy_hit":[0.35,["sci-fi impact","energy hit","heavy punch","impact thud","body hit heavy","punch bag"]],
 "sfx_kill":[0.6,["sci-fi impact heavy","energy explosion small","heavy impact thud","punching bag","impact boom short","bass impact"]],
 "sfx_pickup_stardust":[0.6,["sci-fi pickup","energy pickup","power orb collect","futuristic collect","item pickup sci-fi","pickup low"]],
 "sfx_chain_zap":[0.6,["electric zap heavy","plasma zap","energy zap","electric shock","tesla coil","arc discharge","zap","electric","spark","electricity","shock"]],
 "sfx_dash":[0.6,["sci-fi whoosh","energy whoosh","heavy whoosh","dash sci-fi","teleport whoosh","swoosh bass"]],
 "skill_cast":[1.0,["sci-fi power up","energy charge","sci-fi activate","shield activate","power surge","ability sci-fi"]],
 "sfx_hurt":[0.6,["sci-fi hit damage","player damage sci-fi","heavy punch impact","body impact","shield hit","impact damage"]],
 "sfx_levelup":[1.5,["sci-fi level up","power up heavy","upgrade sci-fi","energy rise","achievement sci-fi","level up bass","level up","power up","upgrade","achievement","success"]],
 "sfx_explode_m":[1.5,["sci-fi explosion","heavy explosion","energy explosion","bass explosion","explosion impact","plasma explosion"]],
 "sfx_death":[2.5,["power down","sci-fi power down","game over heavy","shutdown","energy collapse","death sci-fi"]]}
# 지금까지 쓴 Freesound 소리 (Freesound 처리본·칩튠 외부·펀치·후보풀 1차)
EXCLUDE={"584187","584184","423169","442943","571581","521999","168984","584171","512471","396193","584186",
         "414888","404747","464908","405539","490769","676462","244513","361636",
         "215162","569495","871619","580308","382310","184650","803773","264779","563465","253172","478336",
         "253168","476817","338229","404150","369960"}
ATTR=re.compile(r'<div([^>]*data-mp3[^>]*)>', re.S)
def attrs(s): return dict(re.findall(r'data-([\w-]+)="([^"]*)"', s))
out={}
for k,(maxd,qs) in Q.items():
    m={}
    for q in qs:
        for page in (1,2):
            url="https://freesound.org/search/?"+urllib.parse.urlencode({"q":q,"f":f'license:"Creative Commons 0" duration:[0.03 TO {maxd}]',"s":"Downloads (most first)","g":"0","page":page})
            req=urllib.request.Request(url,headers={"User-Agent":"Mozilla/5.0"})
            try: txt=urllib.request.urlopen(req,timeout=20).read().decode("utf-8","ignore")
            except Exception as e: print("fail",q,e); continue
            for a in ATTR.findall(txt)[:8]:
                d=attrs(a)
                if "sound-id" not in d: continue
                sid=d["sound-id"]
                if sid not in m and sid not in EXCLUDE:
                    m[sid]=dict(id=sid,user=d.get("username"),title=html.unescape(d.get("title",""))[:50],dur=float(d.get("duration",0)),
                                url=d["mp3"].replace("-lq.mp3","-hq.mp3"),downloads=int(d.get("num-downloads",0)),query=q)
        if len(m)>=30: break
    out[k]=list(m.values())
    print(k,len(out[k]))
json.dump(out,open(sys.argv[1],"w",encoding="utf-8"),ensure_ascii=False,indent=0)
u={c["id"] for v in out.values() for c in v}
print("unique",len(u))
