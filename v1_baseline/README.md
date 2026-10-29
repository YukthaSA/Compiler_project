# QueryLang Version 1: Baseline Compiler (Before SIMD Optimization)

**Author:** Yuktha Sreenivasa Anjaneyulu (Reg. No: 23BDS0287)  
**System:** QueryLang Baseline Data Analytics Compiler (Phase 1 & Baseline Phase 2)

---

## Overview
This folder contains the complete, working **Version 1 (v1)** of the QueryLang compiler. This version represents the system **before** AVX2 SIMD vectorization and zero-copy optimization were introduced.

### Characteristics of Version 1 (Baseline):
- **Lexer & Parser:** Built with Flex 2.6.4 and Bison 3.8.2.
- **Abstract Syntax Tree (AST):** Strongly typed in-memory AST in pure C.
- **Symbol Table:** Dynamically inferred from the CSV schema.
- **Semantic Analyzer:** Full compile-time validation for column existence and type compatibility.
- **Intermediate Code:** Generates Three-Address Code (TAC) and Quadruples table.
- **CSV Ingestion Engine:** Standard scalar character-by-character parser (`src/csv_parser.c`) allocating discrete heap strings (`strdup`) for each field in every row.
- **Query Interpreter:** Evaluates filter expressions per row using heap-allocated `CSVRecord` structures.

---

## How to Build and Run Version 1

### 1. Build:
```cmd
build.bat
```
This generates `querylang_v1.exe`.

### 2. Run Test Suite:
```cmd
run_tests.bat
```
Runs all 7 baseline test cases.

### 3. Run a Query:
```cmd
querylang_v1.exe tests\test1_basic.ql
querylang_v1.exe --all tests\test2_aggregate.ql
```
