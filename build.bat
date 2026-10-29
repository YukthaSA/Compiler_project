@echo off
setlocal
if exist C:\msys64\ucrt64\bin (
    set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
) else if exist C:\msys64\mingw64\bin (
    set "PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%"
)

echo Compiling QueryLang (AVX2 SIMD Zero-Copy Engine)...
bison -d -o src/query.tab.c src/query.y
if %ERRORLEVEL% neq 0 (
    echo Bison error.
    exit /b %ERRORLEVEL%
)

flex -o src/lex.yy.c src/query.l
if %ERRORLEVEL% neq 0 (
    echo Flex error.
    exit /b %ERRORLEVEL%
)

gcc -Wall -O3 -mavx2 -Isrc -o querylang.exe ^
    src/query.tab.c ^
    src/lex.yy.c ^
    src/ast.c ^
    src/symbol_table.c ^
    src/semantic.c ^
    src/tac.c ^
    src/csv_parser.c ^
    src/simd_csv.c ^
    src/executor.c ^
    src/main.c

if %ERRORLEVEL% neq 0 (
    echo GCC error.
    exit /b %ERRORLEVEL%
)

echo Build complete: querylang.exe
endlocal
