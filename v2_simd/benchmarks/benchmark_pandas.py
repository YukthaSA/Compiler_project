import time
import os
import sys
import tracemalloc
import pandas as pd

def run_pandas_benchmark(csv_path):
    print(f"Benchmarking Python Pandas on: {csv_path}")
    file_size_mb = os.path.getsize(csv_path) / (1024 * 1024)
    print(f"Dataset Size: {file_size_mb:.2f} MB")
    
    tracemalloc.start()
    t_start = time.perf_counter()
    
    # 1. Ingestion / Parse Time
    t0 = time.perf_counter()
    df = pd.read_csv(csv_path)
    t1 = time.perf_counter()
    load_time_ms = (t1 - t0) * 1000.0
    
    # 2. Query execution: filter department == 'Engineering' and average salary
    t2 = time.perf_counter()
    filtered_df = df[df["department"] == "Engineering"]
    avg_salary = filtered_df["salary"].mean()
    t3 = time.perf_counter()
    query_time_ms = (t3 - t2) * 1000.0
    
    t_end = time.perf_counter()
    total_time_ms = (t_end - t_start) * 1000.0
    
    current_mem, peak_mem = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    total_rows = len(df)
    matched_rows = len(filtered_df)
    throughput_mb_s = file_size_mb / (total_time_ms / 1000.0)
    throughput_rows_s = total_rows / (total_time_ms / 1000.0)
    
    print("\n--- Pandas Results ---")
    print(f"Matched Rows       : {matched_rows} of {total_rows}")
    print(f"AVG(salary)        : {avg_salary:.2f}")
    print("\n--- Execution Telemetry (Python Pandas) ---")
    print(f"Engine Mode        : Python Pandas ({pd.__version__})")
    print(f"Dataset File Size  : {file_size_mb:.2f} MB")
    print(f"Total Rows Parsed  : {total_rows} rows")
    print(f"Parse / Load Time  : {load_time_ms:.3f} ms")
    print(f"Query Filter Time  : {query_time_ms:.3f} ms")
    print(f"Total Latency      : {total_time_ms:.3f} ms")
    print(f"Peak Memory Usage  : {peak_mem / (1024 * 1024):.2f} MB")
    print(f"Scan Throughput    : {throughput_mb_s:.2f} MB/s ({throughput_rows_s / 1e6:.2f} million rows/sec)")
    print()

if __name__ == "__main__":
    csv_file = sys.argv[1] if len(sys.argv) > 1 else "data/large_employees.csv"
    run_pandas_benchmark(csv_file)
