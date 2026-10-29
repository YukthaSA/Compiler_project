@echo off
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%

echo Compiling QueryLang Version 2 (AVX2 SIMD Zero-Copy Engine)...
bison -d -o src/query.tab.c src/query.y
if %ERRORLEVEL% neq 0 (
    echo Error during Bison generation.
    exit /b %ERRORLEVEL%
)

flex -o src/lex.yy.c src/query.l
if %ERRORLEVEL% neq 0 (
    echo Error during Flex generation.
    exit /b %ERRORLEVEL%
)

gcc -Wall -O3 -mavx2 -Isrc -o querylang_v2.exe src/query.tab.c src/lex.yy.c src/ast.c src/symbol_table.c src/semantic.c src/tac.c src/simd_csv.c src/csv_parser.c src/executor.c src/main.c
if %ERRORLEVEL% neq 0 (
    echo Error during GCC compilation.
    exit /b %ERRORLEVEL%
)

echo Build complete: querylang_v2.exe
