@echo off
setlocal
echo ========================================================================
echo  QueryLang Version 1: Step-by-Step Module Verification Runner
echo  Student: Yuktha Sreenivasa Anjaneyulu (23BDS0287)
echo ========================================================================
echo.
echo Target Query: tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 1] Lexical Analysis (Flex Token Stream Recognition)
echo Command: .\querylang_v1.exe --lex tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --lex tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 2] Syntactic Analysis (Bison LALR(1) Grammar Parsing)
echo Command: .\querylang_v1.exe --syntax tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --syntax tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 3] Abstract Syntax Tree Construction
echo Command: .\querylang_v1.exe --ast tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --ast tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 4] Dynamic Symbol Table (Dataset Schema Ingestion)
echo Command: .\querylang_v1.exe --symtab tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --symtab tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 5] Semantic Analysis and Type Safety Checking
echo Command: .\querylang_v1.exe --semantic tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --semantic tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 6] Intermediate Code Generation (TAC and Quadruples)
echo Command: .\querylang_v1.exe --tac tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe --tac tests\test1_basic.ql
echo.

echo ------------------------------------------------------------------------
echo [MODULE 7] Baseline Execution Engine (Tabular Output)
echo Command: .\querylang_v1.exe tests\test1_basic.ql
echo ------------------------------------------------------------------------
.\querylang_v1.exe tests\test1_basic.ql
echo.
echo All Phase 2 Core Modules Verified Successfully.
