# MergeShip2: pair each function in MM's player with its counterpart in OoT's player by body
# similarity (both descend from the same code). Output: build/player_func_map.tsv
import re, difflib, sys
def parse(path):
    lines=open(path,encoding='utf-8',errors='replace').read().split('\n')
    fre=re.compile(r'^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\(([^;]*)\)\s*\{\s*$')
    funcs={}; cur=None
    for i,l in enumerate(lines):
        m=fre.match(l)
        if m and not l.startswith(' ') and m.group(1) not in ('if','for','while','switch'):
            cur=m.group(1); funcs[cur]=[i,None]
        elif l.startswith('}') and cur:
            funcs[cur][1]=i; cur=None
    return {n:'\n'.join(lines[s:e+1]) for n,(s,e) in funcs.items() if e}, {n:s+1 for n,(s,e) in funcs.items() if e}
KW=set('if else for while switch case break return default do goto sizeof s32 u32 s16 u16 s8 u8 f32 f64 void static const struct'.split())
def norm(body, names):
    toks=re.findall(r'[A-Za-z_]\w*|\d+\.?\d*f?|0x[0-9A-Fa-f]+|[^\s\w]',body)
    out=[]
    for t in toks[1:]:
        if t in names: out.append('F')          # a call to a function of the same file
        elif re.match(r'(func|D|sp|var|temp|phi|arg|unk)_?\w*',t): out.append('I')
        elif re.match(r'[A-Za-z_]',t) and t not in KW: out.append('I')
        else: out.append(t)
    return out
mm,mml=parse('2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/z_player.c')
oot,ootl=parse('Shipwright/soh/src/overlays/actors/ovl_player_actor/z_player.c')
mmn={n:norm(b,mm) for n,b in mm.items()}
ootn={n:norm(b,oot) for n,b in oot.items()}
res=[]
for n,t in mmn.items():
    best=(0,None)
    for o,u in ootn.items():
        if abs(len(t)-len(u))>max(len(t),len(u))*0.6: continue
        sm=difflib.SequenceMatcher(None,t,u,autojunk=False)
        if sm.real_quick_ratio()<best[0] or sm.quick_ratio()<best[0]: continue
        r=sm.ratio()
        if r>best[0]: best=(r,o)
    res.append((n,mml[n],best[1],ootl.get(best[1],0),round(best[0],3),len(t)))
with open('build/player_func_map.tsv','w') as f:
    f.write('mm_func\tmm_line\toot_func\toot_line\tsimilarity\tmm_tokens\n')
    for r in sorted(res,key=lambda r:r[1]): f.write('\t'.join(map(str,r))+'\n')
import statistics
print('matched',len(res),'median sim',statistics.median(r[4] for r in res),'>=0.8:',sum(r[4]>=0.8 for r in res),'<0.5:',sum(r[4]<0.5 for r in res))
