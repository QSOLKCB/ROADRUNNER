# SPDX-License-Identifier: Apache-2.0
"""Zero-resolution comparisons must remain complete, parseable and inconclusive."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("compare",root/"benchmarks/compare.py")
compare=importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)
assert compare.latency_ratio(2,1)==0.5
for a,b in [(0,0),(0,1),(1,0)]:assert compare.latency_ratio(a,b) is None
for a,b in [(-1,1),(1,float('inf')),(float('nan'),1)]:
    try:compare.latency_ratio(a,b)
    except ValueError:pass
    else:raise AssertionError((a,b))
assert compare.ratio_summary([0.5,1,2])['median_pair_ratio']==1
mixed=compare.ratio_summary([0.5,None,2])
assert mixed['median_pair_ratio'] is None and mixed['min_ratio'] is None
assert mixed['max_ratio'] is None and mixed['unmeasurable_pairs']==1
assert mixed['all_ratios']==[0.5,None,2]
with tempfile.TemporaryDirectory() as directory:
    output=Path(directory)/"zero.json"
    binary=str(Path(sys.argv[1]).resolve())
    subprocess.run([sys.executable,str(root/"benchmarks/compare.py"),
        "--baseline",binary,"--candidate",binary,"--baseline-commit","frozen-clock-test",
        "--candidate-commit","frozen-clock-test","--baseline-core",str(root/"src/roadrunner.c"),
        "--candidate-core",str(root/"src/roadrunner.c"),"--flags","frozen clock test binary",
        "--output",str(output),"--side","4","--queries","21","--pairs","3"],check=True,capture_output=True,text=True)
    data=json.loads(output.read_text())
    assert len(data['pairs'])==30 and len(data['summary'])==10
    assert all(p['median_ratio'] is None for p in data['pairs'])
    for summary in data['summary']:
        assert summary['status']=='inconclusive_clock_resolution'
        assert summary['unmeasurable_pairs']==3 and summary['all_ratios']==[None]*3
        assert summary['median_pair_ratio'] is None
    assert output.with_suffix('.txt').read_text().count('inconclusive:')==10
print("Benchmark comparison clock-resolution regressions passed")
