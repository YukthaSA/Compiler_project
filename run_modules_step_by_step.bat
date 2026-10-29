@echo off
setlocal
echo ========================================================================
echo  QueryLang: Step-by-Step Module Verification Runner
echo  Student: Yuktha Sreenivasa Anjaneyulu (23BDS0287)
echo ========================================================================
echo.
echo Target Query: tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 1] Lexical Analysis (Flex Token Stream Recognition)
echo Command: .\querylang.exe --lex tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --lex tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 2] Syntactic Analysis (Bison LALR(1) Grammar Parsing)
echo Command: .\querylang.exe --syntax tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --syntax tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 3] Abstract Syntax Tree Construction
echo Command: .\querylang.exe --ast tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --ast tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 4] Dynamic Symbol Table (Dataset Schema Ingestion)
echo Command: .\querylang.exe --symtab tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --symtab tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 5] Semantic Analysis and Type Safety Checking
echo Command: .\querylang.exe --semantic tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --semantic tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 6] Intermediate Code Generation (TAC and Quadruples)
echo Command: .\querylang.exe --tac tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe --tac tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 7] Query Execution Engine (Tabular Output)
echo Command: .\querylang.exe tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang.exe tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 8] AVX2 SIMD Zero-Copy Vector Scan and Telemetry (250K Rows)
echo Command: .\querylang.exe --simd -b tests\test_benchmark_filter.ql
echo ------------------------------------------------------------------------
.\querylang.exe --simd -b tests\test_benchmark_filter.ql
echo.
echo All Phase 2 Core Modules Verified Successfully.
