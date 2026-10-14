# SPDX-License-Identifier: Apache-2.0
"""Independent original-edge Bellman-Ford benchmark replay; no runtime oracle reuse."""
import subprocess,sys,json,math,hashlib,pathlib
binary=pathlib.Path(sys.argv[1]);MASK=(1<<64)-1; INIT=1469598103934665603
class RNG:
 def __init__(self,s):self.x=s
 def next(self):
  x=self.x;x^=(x<<13)&0xffffffff;x^=x>>17;x^=(x<<5)&0xffffffff;self.x=x;return x

def h(x,y):return ((x^y)*1099511628211)&MASK

def oracle(family,side,queries,seed):
 n=side*side;r=RNG(seed);edges=[];gh=INIT;qh=INIT;ck=0
 for u in range(n):
  x,y=u%side,u//side
  if family=='corridor':ns=([u-1] if u else [])+([u+1] if u+1<n else [])
  elif family=='sparse':ns=([u+1] if u+1<n else [])+[r.next()%n for _ in range(3)]
  else:ns=([u-1] if x else [])+([u+1] if x+1<side else [])+([u-side] if y else [])+([u+side] if y+1<side else [])
  for v in ns:
   c=1+r.next()%31 if family in ('weighted','sparse') else 1
   if family=='disconnected' and ((u%side<side//2)!=(v%side<side//2)):continue
   edges.append((u,v,c));gh=h(h(h(gh,u),v),c)
 r=RNG(seed^0x9e3779b9);cache={};found=0
 for q in range(queries+5):
  s,t=r.next()%n,r.next()%n
  if family=='local':t=s//side*side+(s%side+1+q%4)%side
  if family=='disconnected':s=s//side*side+s%(side//2);t=t//side*side+side//2+t%(side//2)
  if s not in cache:
   # Bellman-Ford with Python unbounded arithmetic over original edge tuples.
   d=[None]*n;d[s]=0
   for _ in range(n-1):
    change=False
    for u,v,c in edges:
     if d[u] is not None and (d[v] is None or d[u]+c<d[v]):d[v]=d[u]+c;change=True
    if not change:break
   cache[s]=d
  d=cache[s][t]
  if q>=5:
   qh=h(h(qh,s),t);ck=h(ck,MASK if d is None else d);found+=d is not None
 return {'graph_hash':f'{gh:016x}','query_hash':f'{qh:016x}','cost_checksum':f'{ck:016x}','successful_routes':found,'nodes':n,'edges':len(edges)}
rows=[]
for seed in [123456789,17,0xffffffff]:
 for family in ['grid','weighted','local','sparse','disconnected','corridor']:
  expected=oracle(family,4,21,seed)
  for strategy in ['auto','dijkstra','astar']:
   for layout in ['row','morton']:
    x=json.loads(subprocess.check_output([str(binary.resolve()),family,strategy,layout,'4','21',str(seed)],text=True))
    for k,v in expected.items():assert x[k]==v,(family,strategy,layout,seed,k,x[k],v)
    assert len(x['samples_us'])==21 and x['query_allocations']==0
    vals=sorted(x['samples_us']);assert abs(x['median_us']-vals[10])<=1e-6
    assert abs(x['p95_us']-vals[19])<=1e-6 and abs(x['p99_us']-vals[20])<=1e-6
    assert all(math.isfinite(v) and v>=0 for v in x['samples_us'])
    rows.append({'family':family,'strategy':strategy,'layout':layout,'seed':seed,**expected})
print(json.dumps({'status':'PASS','cases':len(rows),'oracle':'independent Python Bellman-Ford on original generated edge tuples','rows_sha256':hashlib.sha256(json.dumps(rows,sort_keys=True).encode()).hexdigest(),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()},sort_keys=True))
