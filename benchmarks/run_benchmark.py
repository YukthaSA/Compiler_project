import os
import sys
import subprocess
import time
import re
import tracemalloc
import pandas as pd

# Automatically anchor current directory to the project/version root
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
os.chdir(BASE_DIR)

# Locate querylang executable
EXE_PATH = None
for cand in ["querylang_v2.exe", "querylang.exe"]:
    if os.path.exists(os.path.join(BASE_DIR, cand)):
        EXE_PATH = os.path.join(BASE_DIR, cand)
        break
if not EXE_PATH:
    EXE_PATH = "querylang.exe"

DATASETS = [
    ("100K Rows", 100000, "data/benchmark_100k.csv", "tests/bench_100k.ql"),
    ("250K Rows", 250000, "data/large_employees.csv", "tests/test_benchmark_agg.ql"),
    ("1M Rows",   1000000, "data/employees_1m.csv",   "tests/test_1m.ql")
]

def ensure_dataset(rows, filepath, ql_file):
    if not os.path.exists(filepath):
        sys.path.insert(0, os.path.join(BASE_DIR, "benchmarks"))
        from generate_large_dataset import generate_csv
        generate_csv(filepath, rows)
        
    os.makedirs(os.path.dirname(ql_file), exist_ok=True)
    if not os.path.exists(ql_file):
        with open(ql_file, "w") as f:
            f.write(f'LOAD "{filepath}" AS dataset;\n')
            f.write('SELECT department, salary;\n')
            f.write('FILTER department == "Engineering";\n')
            f.write('AGGREGATE AVG(salary);\n')

def run_querylang(ql_file, simd=True):
    flag = "--simd" if simd else "--scalar"
    cmd = [EXE_PATH, ql_file, flag, "-b"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    out = res.stdout
    
    parse_match = re.search(r"Parse Time\s*:\s*([\d\.]+)\s*ms", out)
    eval_match = re.search(r"Query Filter Time\s*:\s*([\d\.]+)\s*ms", out)
    total_match = re.search(r"Total Latency\s*:\s*([\d\.]+)\s*ms", out)
    tp_match = re.search(r"Scan Throughput\s*:\s*([\d\.]+)\s*MB/s", out)
    
    parse_time = float(parse_match.group(1)) if parse_match else 0.0
    eval_time = float(eval_match.group(1)) if eval_match else 0.0
    total_time = float(total_match.group(1)) if total_match else 0.0
    throughput_mb = float(tp_match.group(1)) if tp_match else 0.0
    
    return {
        "parse_time_ms": parse_time,
        "eval_time_ms": eval_time,
        "total_time_ms": total_time,
        "throughput_mb_s": throughput_mb
    }

def run_pandas(csv_file):
    tracemalloc.start()
    t0 = time.perf_counter()
    df = pd.read_csv(csv_file)
    t1 = time.perf_counter()
    
    t2 = time.perf_counter()
    filt = df[df["department"] == "Engineering"]
    avg = filt["salary"].mean()
    t3 = time.perf_counter()
    
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    fsize_mb = os.path.getsize(csv_file) / (1024 * 1024)
    total_time_ms = (t3 - t0) * 1000.0
    load_time_ms = (t1 - t0) * 1000.0
    eval_time_ms = (t3 - t2) * 1000.0
    throughput = fsize_mb / (total_time_ms / 1000.0) if total_time_ms > 0 else 0
    
    return {
        "parse_time_ms": load_time_ms,
        "eval_time_ms": eval_time_ms,
        "total_time_ms": total_time_ms,
        "peak_mem_mb": peak / (1024 * 1024),
        "throughput_mb_s": throughput
    }

def main():
    print("Running Comparative Performance Benchmark Suite...")
    print(f"{'Dataset':<12} | {'Engine':<20} | {'Parse (ms)':<10} | {'Total Latency (ms)':<18} | {'Throughput (MB/s)':<18} | {'Speedup':<8}")
    print("-" * 96)
    
    for label, rows, csv_file, ql_file in DATASETS:
        ensure_dataset(rows, csv_file, ql_file)
        
        simd_res = run_querylang(ql_file, simd=True)
        scalar_res = run_querylang(ql_file, simd=False)
        pd_res = run_pandas(csv_file)
        
        speedup_simd_vs_pd = (pd_res["total_time_ms"] / simd_res["total_time_ms"]) if simd_res["total_time_ms"] > 0 else 1.0
        speedup_scalar = (pd_res["total_time_ms"] / scalar_res["total_time_ms"]) if scalar_res["total_time_ms"] > 0 else 1.0
        
        print(f"{label:<12} | {'Python Pandas':<20} | {pd_res['parse_time_ms']:<10.2f} | {pd_res['total_time_ms']:<18.2f} | {pd_res['throughput_mb_s']:<18.2f} | {'1.00x':<8}")
        print(f"{'':<12} | {'Scalar Zero-Copy':<20} | {scalar_res['parse_time_ms']:<10.2f} | {scalar_res['total_time_ms']:<18.2f} | {scalar_res['throughput_mb_s']:<18.2f} | {speedup_scalar:<7.2f}x")
        print(f"{'':<12} | {'AVX2 SIMD Zero-Copy':<20} | {simd_res['parse_time_ms']:<10.2f} | {simd_res['total_time_ms']:<18.2f} | {simd_res['throughput_mb_s']:<18.2f} | {speedup_simd_vs_pd:<7.2f}x")
        print("-" * 96)

if __name__ == "__main__":
    main()
