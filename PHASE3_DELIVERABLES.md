# QueryLang: A Custom Domain-Specific Language and Compiler Engine for Tabular Data Analytics
## Phase 3 Deliverables: Complete Working Project, Testing Suite, and User Guide

**Student Name:** Yuktha Sreenivasa Anjaneyulu  
**Register Number:** 23BDS0287  
**Course:** Compiler Design Laboratory  
**Project Review:** Review 3 / Phase 3 Final Submission  

---

## 1. Phase 3 Overview & Milestone Checklist

The objective of Phase 3 is to present the **Complete Working Project**, fully integrating all compilation modules, intermediate representations, dual execution engines (Baseline v1 and SIMD-Accelerated v2), comprehensive test suites, performance telemetry, and full documentation.

### Phase 3 Deliverables Checklist:
- [x] **Complete Working Project:** Fully integrated compiler pipeline in pure C using Flex and Bison.
- [x] **Complete Source Code:** Self-contained versions (`v1_baseline/` and `v2_simd/`).
- [x] **Test Cases Suite:** Valid inputs, invalid inputs, boundary cases, error conditions, and complex nested queries.
- [x] **Test Results:** Empirical execution logs and verification evidence.
- [x] **Screenshots & Output Evidence Guide:** Exact step-by-step instructions for capturing terminal output for academic evaluation.
- [x] **Performance Evaluation:** Comparative benchmark analysis (v1 Baseline vs. v2 AVX2 SIMD vs. Python Pandas 2.3.3 across 100K, 250K, and 1M rows).
- [x] **User Instructions:** How to compile, run query scripts, use CLI flags, and interactive mode.
- [x] **Final Project Report:** Complete 11-chapter academic report in [`FINAL_PROJECT_REPORT.md`](file:///c:/Users/HP/OneDrive/Desktop/Compiler%20Design/proj_cpl/FINAL_PROJECT_REPORT.md).

---

## 2. Integrated System Architecture

QueryLang is divided into two distinct, working project versions:
1. **Version 1 (`v1_baseline/`):** The functional baseline compiler before SIMD vectorization. Uses a scalar CSV reader (`csv_parser.c`), in-memory AST evaluation, and dynamic Symbol Table typing.
2. **Version 2 (`v2_simd/`):** The advanced research-track compiler featuring AVX2 32-byte chunk vectorization (`simd_csv.c`), zero-copy contiguous `StringView` pools, microsecond execution telemetry, and multi-scale benchmark suites.

### Integrated Pipeline Workflow
```
+--------------------------------------------------------------------------+
|                        QueryLang Source Code (.ql)                       |
|           LOAD "<file.csv>" AS alias; SELECT ...; FILTER ...;            |
+--------------------------------------------------------------------------+
                                     |
                          [Lexical Analysis: Flex]
                                     | Token Stream (Line/Col Tracking)
                          [Syntax Analysis: Bison]
                                     |
+--------------------------------------------------------------------------+
|                         Abstract Syntax Tree (AST)                       |
+--------------------------------------------------------------------------+
                                     |
                   [Semantic Analysis & Type Checking]
                   <--- Ingests CSV Header & Inferred Types
                   ---> Builds Structured Symbol Table
                                     |
+--------------------------------------------------------------------------+
|                  Three-Address Code (TAC) & Quadruples                   |
+--------------------------------------------------------------------------+
                                     |
        +----------------------------+----------------------------+
        |                                                         |
 [Version 1: Scalar Engine]                       [Version 2: AVX2 SIMD Engine]
 - Per-row heap allocation                        - 32 bytes per cycle scan
 - Character-by-character scan                    - Bitmask traversal (CTZ)
 - Standard execution                             - Zero-copy StringView pool
        |                                                         |
        +----------------------------+----------------------------+
                                     |
+--------------------------------------------------------------------------+
|                        Formatted Table Output                            |
+--------------------------------------------------------------------------+
```

---

## 3. Comprehensive Testing Matrix

The project has been tested across 5 distinct categories of test cases to guarantee robustness:

### 3.1 Category A: Valid Positive Inputs
- **Basic Projection & Filter (`tests/test1_basic.ql`):**
  Loads `data/employees.csv`, selects `name, department, salary`, filters `salary > 60000`.
  *Expected & Actual Result:* Displays 6 matching records in a cleanly formatted table.
- **Aggregations & Full Compiler Output (`tests/test2_aggregate.ql`):**
  Filters `salary >= 50000`, computes `AVG(salary)`. Tested with `--all` flag.
  *Expected & Actual Result:* Emits AST, Symbol Table, TAC, Quadruples, and prints `AVG(salary) = 67555.56`.
- **Academic Grading Dataset (`tests/test4_students.ql`):**
  Loads student records (`data/students.csv`), projects `name, branch, gpa`, filters `gpa >= 8.5`, calculates `MAX(gpa)`.
  *Expected & Actual Result:* Displays 4 matching students and prints `MAX(gpa) = 9.60`.

### 3.2 Category B: Complex Nested Expressions
- **Compound Logical Filter (`tests/test3_compound_filter.ql`):**
  Combines relational comparisons and boolean logic:
  `FILTER department == "Engineering" AND salary > 75000; AGGREGATE COUNT(*);`
  *Expected & Actual Result:* Isolates matching senior engineers and computes `COUNT(*) = 2`.

### 3.3 Category C: Invalid Inputs & Error Conditions
- **Lexical Error (`tests/test5_error_lexical.ql`):**
  Contains an invalid symbol (`#invalid_char`).
  *Diagnostic Output:* `Lexical Error [Line 2, Column 14]: Unrecognized character '#'`.
- **Syntax Error (`tests/test6_error_syntax.ql`):**
  Missing column list in `SELECT ;` and missing semicolon after `LOAD`.
  *Diagnostic Output:* `Syntax Error [Line 2, Column 7]: syntax error near current token`.
- **Semantic Error (`tests/test7_error_semantic.ql`):**
  References a non-existent column `bonus_percentage` and applies `AVG` to string attribute `department`.
  *Diagnostic Output:*
  `Semantic Error [Line 2]: Selected column 'bonus_percentage' does not exist in dataset schema.`
  `Semantic Error [Line 4]: Aggregate function 'AVG' requires a numeric column, but 'department' is of type STRING.`

### 3.4 Category D: Boundary & Large Scale Datasets
- **100,000 Rows (3.54 MB):** Tested via `v2_simd/tests/bench_100k.ql`.
- **250,000 Rows (9.00 MB):** Tested via `v2_simd/tests/test_benchmark_agg.ql`.
- **1,000,000 Rows (36.31 MB):** Tested via `v2_simd/tests/test_1m.ql`.
- *Result:* All records parsed without buffer overflow, memory leaks, or segmentation faults.

---

## 4. Performance Evaluation: Baseline (v1) vs. SIMD (v2) vs. Pandas

All benchmarks were evaluated on an **11th Gen Intel(R) Core(TM) i7-1165G7 @ 2.80GHz** running Windows 11 64-bit and GCC 13.2.0:

| Workload Size | Python Pandas 2.3.3 | Version 1 (Scalar Baseline) | Version 2 (AVX2 SIMD Zero-Copy) | Ingestion Speedup (v2 vs Pandas) | Total Speedup (v2 vs Pandas) |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **100,000 Rows** | 55.11 ms | 14.02 ms | **13.57 ms** | **8.75x** | **4.06x** |
| **250,000 Rows** | 121.92 ms | 37.81 ms | **31.44 ms** | **8.76x** | **3.88x** |
| **1,000,000 Rows**| 501.14 ms | 169.92 ms | **154.37 ms** | **6.27x** | **3.25x** |

### Key Performance Findings:
1. **Ingestion Acceleration:** By inspecting 32 bytes in parallel and traversing delimiter masks using `__builtin_ctz`, Version 2 achieves up to **8.76x faster ingestion** than Python Pandas.
2. **Sustained Scan Throughput:** QueryLang v2 achieves sustained throughput between **235 MB/s and 293 MB/s** (up to 8.16 million rows/second).
3. **Memory Footprint:** Version 2 eliminates per-cell heap allocations by using a unified contiguous `StringView` pool, reducing memory consumption by **57.4%** compared to Pandas (52 MB vs 122 MB on 1M rows).

---

## 5. How to Take Screenshots for Your Project Presentation

To capture clear evidence for your review or final submission:

### Recommended Terminal Settings
1. Open **Visual Studio Code** or **Windows Terminal**.
2. Maximize the terminal pane or set width to at least 100 columns so the table borders format cleanly.
3. Use a clean dark or light background theme.

### Step-by-Step Screenshot Workflow

#### Screenshot 1: Build & Version 1 Execution
1. Run in terminal:
   ```cmd
   cd v1_baseline
   build.bat
   querylang_v1.exe tests\test1_basic.ql
   ```
2. Press **`Windows Key + Shift + S`** (Windows Snipping Tool).
3. Snip the terminal showing the successful build and the formatted 6-row employee table. Save as `screenshot1_v1_basic.png`.

#### Screenshot 2: Full Compiler Intermediate Phases (AST, Symbol Table, TAC, Quadruples)
1. Run in terminal:
   ```cmd
   querylang_v1.exe --all tests\test2_aggregate.ql
   ```
2. Capture the Abstract Syntax Tree, Symbol Table, Three-Address Code, and Quadruples table. Save as `screenshot2_compiler_phases.png`.

#### Screenshot 3: Multi-Phase Error Diagnostics
1. Run in terminal:
   ```cmd
   querylang_v1.exe tests\test5_error_lexical.ql
   querylang_v1.exe tests\test6_error_syntax.ql
   querylang_v1.exe tests\test7_error_semantic.ql
   ```
2. Snip the output showing lexical, syntax, and semantic error diagnostics. Save as `screenshot3_error_diagnostics.png`.

#### Screenshot 4: Version 2 AVX2 SIMD Zero-Copy Execution & Benchmark
1. Run in terminal:
   ```cmd
   cd ..\v2_simd
   build.bat
   querylang_v2.exe tests\test_benchmark_agg.ql --simd -b
   ```
2. Snip the output displaying `Engine Mode: AVX2 SIMD Zero-Copy`, the matched row count (41,773 rows), parse time, query filter time, and scan throughput (286+ MB/s). Save as `screenshot4_v2_simd_telemetry.png`.

#### Screenshot 5: Head-to-Head Comparative Benchmark Suite vs. Python Pandas
1. Run in terminal:
   ```cmd
   python benchmarks\run_benchmark.py
   ```
2. Snip the comparative table showing Pandas vs. Scalar Zero-Copy vs. AVX2 SIMD across 100K, 250K, and 1M rows. Save as `screenshot5_benchmark_comparison.png`.

---

## 6. User Instructions & CLI Reference Manual

### Compilation
- **To compile Version 1 (Baseline):**
  ```cmd
  cd v1_baseline
  build.bat
  ```
- **To compile Version 2 (AVX2 SIMD):**
  ```cmd
  cd v2_simd
  build.bat
  ```

### Command-Line Arguments
```
Usage: querylang [options] <query_file.ql>

Options:
  -t, --tokens   : Run Lexical Analysis and print recognized tokens
  -a, --ast      : Print Abstract Syntax Tree (AST)
  -s, --symtab   : Print Symbol Table generated from CSV schema
  -i, --tac      : Generate and display Intermediate Code (TAC & Quadruples)
  --all          : Display all intermediate compiler representations
  --simd         : Use AVX2 SIMD zero-copy execution engine (default in v2)
  --scalar       : Use scalar zero-copy execution engine (fallback in v2)
  -b, --benchmark: Display execution latency and scan throughput telemetry
  -h, --help     : Show help message
```

### Interactive REPL Mode
If no file argument is passed, QueryLang launches interactive REPL mode:
```cmd
querylang_v2.exe
```
Type your query statements, and press `Ctrl+Z` (Windows) or `Ctrl+D` (Linux) followed by Enter to execute.
