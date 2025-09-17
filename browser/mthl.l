/* mthl.l - Lexical analyzer for MTHL */
%{
#include <stdio.h>
#include "mthl.tab.h"  /* Bison-generated header */

/* For handling string literals */
char string_buffer[1024];
char *string_buf_ptr;
%}

%option noyywrap

/* States */
%x STRING

%%

"page"          { return PAGE; }
"container"     { return CONTAINER; }
"text"          { return TEXT; }
"rectangle"     { return RECTANGLE; }
"button"        { return BUTTON; }
"var"           { return VAR; }
"if"            { return IF; }
"else"          { return ELSE; }
"for"           { return FOR; }
"position"      { return POSITION; }
"background"    { return BACKGROUND; }
"content"       { return CONTENT; }
"font"          { return FONT; }
"color"         { return COLOR; }
"fill"          { return FILL; }
"align"         { return ALIGN; }
"onclick"       { return ONCLICK; }
"redraw"        { return REDRAW; }

"|<"             { return LBRACE; }
">|"             { return RBRACE; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
";"             { return SEMICOLON; }
","             { return COMMA; }
"="             { return ASSIGN; }
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULTIPLY; }
"/"             { return DIVIDE; }
">"             { return GT; }
"<"             { return LT; }
">="            { return GE; }
"<="            { return LE; }
"=="            { return EQ; }
"!="            { return NE; }
"&&"            { return AND; }
"||"            { return OR; }
"!"             { return NOT; }
"."             { return DOT; }

[0-9]+          { yylval.ival = atoi(yytext); return INTEGER; }
[0-9]+"%"       { 
                  yytext[yyleng-1] = '\0';  /* Remove % sign */
                  yylval.ival = atoi(yytext); 
                  return PERCENTAGE; 
                }

[a-zA-Z_][a-zA-Z0-9_]* { 
                  yylval.sval = strdup(yytext); 
                  return IDENTIFIER; 
                }

\"              { string_buf_ptr = string_buffer; BEGIN(STRING); }
<STRING>\"      { 
                  *string_buf_ptr = '\0'; 
                  yylval.sval = strdup(string_buffer); 
                  BEGIN(INITIAL); 
                  return STRING_LITERAL; 
                }
<STRING>\\n     { *string_buf_ptr++ = '\n'; }
<STRING>\\t     { *string_buf_ptr++ = '\t'; }
<STRING>\\\"    { *string_buf_ptr++ = '\"'; }
<STRING>\\      { *string_buf_ptr++ = '\\'; }
<STRING>[^\\\"]+ { 
                  char *yptr = yytext;
                  while (*yptr)
                      *string_buf_ptr++ = *yptr++;
                }

"#"[0-9A-Fa-f]{6} { yylval.sval = strdup(yytext); return COLOR_HEX; }

[ \t\n\r]+      { /* Ignore whitespace */ }
"/^".*          { /* Ignore comments */ }

.               { fprintf(stderr, "Unrecognized character: %s\n", yytext); }

%%