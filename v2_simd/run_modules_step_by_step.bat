@echo off
setlocal
echo ========================================================================
echo  QueryLang Version 2: Step-by-Step Module Verification Runner
echo  Student: Yuktha Sreenivasa Anjaneyulu (23BDS0287)
echo ========================================================================
echo.
echo Target Query: tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 1] Lexical Analysis (Flex Token Stream Recognition)
echo Command: .\querylang_v2.exe --lex tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --lex tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 2] Syntactic Analysis (Bison LALR(1) Grammar Parsing)
echo Command: .\querylang_v2.exe --syntax tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --syntax tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 3] Abstract Syntax Tree Construction
echo Command: .\querylang_v2.exe --ast tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --ast tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 4] Dynamic Symbol Table (Dataset Schema Ingestion)
echo Command: .\querylang_v2.exe --symtab tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --symtab tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 5] Semantic Analysis and Type Safety Checking
echo Command: .\querylang_v2.exe --semantic tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --semantic tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 6] Intermediate Code Generation (TAC and Quadruples)
echo Command: .\querylang_v2.exe --tac tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --tac tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 7] Query Execution Engine (Tabular Output)
echo Command: .\querylang_v2.exe tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 8] AVX2 SIMD Zero-Copy Vector Scan and Telemetry (250K Rows)
echo Command: .\querylang_v2.exe --simd -b tests\test_benchmark_filter.ql
echo ------------------------------------------------------------------------
.\querylang_v2.exe --simd -b tests\test_benchmark_filter.ql
echo.
echo All Phase 2 Core Modules Verified Successfully.
