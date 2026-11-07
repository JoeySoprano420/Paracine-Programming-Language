"""Optional Linux-hosted Win64 ABI verifier for the prepared compiler sources.
Windows actual runtime tests must run on Windows.
"""
from pathlib import Path
import subprocess,json,re,sys
base=Path(__file__).resolve().parents[1]
host=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/mnt/data/Paracine_Stage1_Build_Run_2026/stage0/paracine_stage0_linux')
report=[]
def run(name,src,opt,expected):
 r=subprocess.run([str(host),'run',str(src),f'-O{opt}'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,cwd=base/'tests',timeout=90)
 m=re.search(r'isolated emitted x86-64 execution .*?exit=(\d+)',r.stdout)
 observed=int(m.group(1)) if m else None
 ok=(r.returncode==expected and observed==expected)
 report.append({'name':name,'optimization':opt,'expected':expected,'observed':observed,'passed':ok})
 print(('PASS' if ok else 'FAIL'),name,'O'+str(opt),'exit',observed,flush=True)
 if not ok:print(r.stdout)
for name in ['LegacyRegression','LexerRegression','ParserExpansionRegression']:
 for opt in [0,3]: run(name,base/'tests'/(name+'.pcn'),opt,0)
for src in sorted((base/'tests').glob('generated_[0-9]*.pcn')):
 for opt in [0,3]:run(src.stem,src,opt,42)
(base/'TEST_RESULTS.json').write_text(json.dumps({'tests':report,'passed':sum(t['passed'] for t in report),'total':len(report)},indent=2))
print('TOTAL',sum(t['passed'] for t in report),'/',len(report))
if not all(t['passed'] for t in report):sys.exit(1)
