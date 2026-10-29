%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "query.tab.h"

int yycolumn = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;

static char* strip_quotes(const char *src) {
    size_t len = strlen(src);
    if (len < 2) return strdup("");
    char *res = (char*)malloc(len);
    strncpy(res, src + 1, len - 2);
    res[len - 2] = '\0';
    return res;
}
%}

%option noyywrap
%option yylineno
%option case-insensitive
%option noinput
%option nounput

%%

[ \t]+                  { /* Skip spaces and tabs */ }
\r?\n                   { yycolumn = 1; /* Reset column on newline */ }

"//".*                  { /* Skip single-line comments */ }
"/*"([^*]|\*+[^*/])*\*+"/" { /* Skip multi-line comments */ }

"LOAD"                  { return LOAD; }
"AS"                    { return AS; }
"SELECT"                { return SELECT; }
"FILTER"                { return FILTER; }
"AGGREGATE"             { return AGGREGATE; }

"SUM"                   { return SUM; }
"AVG"                   { return AVG; }
"COUNT"                 { return COUNT; }
"MAX"                   { return MAX; }
"MIN"                   { return MIN; }

"AND"                   { return AND; }
"OR"                    { return OR; }
"NOT"                   { return NOT; }

"=="                    { return EQ; }
"="                     { return EQ; }
"!="                    { return NEQ; }
">="                    { return GTE; }
"<="                    { return LTE; }
">"                     { return '>'; }
"<"                     { return '<'; }

";"                     { return ';'; }
","                     { return ','; }
"("                     { return '('; }
")"                     { return ')'; }
"*"                     { return '*'; }

[0-9]+\.[0-9]+          {
                            yylval.fval = atof(yytext);
                            return FLOAT_LITERAL;
                        }

[0-9]+                  {
                            yylval.ival = atoi(yytext);
                            return NUMBER;
                        }

\"([^\"\\]|\\.)*\"      {
                            yylval.sval = strip_quotes(yytext);
                            return STRING;
                        }

[a-zA-Z_][a-zA-Z0-9_]*  {
                            yylval.sval = strdup(yytext);
                            return IDENTIFIER;
                        }

.                       {
                            fprintf(stderr, "Lexical Error [Line %d, Column %d]: Unrecognized character '%s'\n",
                                    yylineno, yycolumn - yyleng, yytext);
                        }

%%
