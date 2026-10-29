@echo off
setlocal

if not exist querylang_v1.exe (
    call build.bat
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)

echo [Test 1] Basic projection and filter:
querylang_v1.exe tests\test1_basic.ql
echo.

echo [Test 2] Full intermediate phases (--all):
querylang_v1.exe --all tests\test2_aggregate.ql
echo.

echo [Test 3] Filter with AND condition:
querylang_v1.exe tests\test3_compound_filter.ql
echo.

echo [Test 4] Students dataset:
querylang_v1.exe tests\test4_students.ql
echo.

echo [Test 5] Lexical error handling:
querylang_v1.exe tests\test5_error_lexical.ql
echo.

echo [Test 6] Syntax error handling:
querylang_v1.exe tests\test6_error_syntax.ql
echo.

echo [Test 7] Semantic error handling:
querylang_v1.exe tests\test7_error_semantic.ql
echo.
endlocal
