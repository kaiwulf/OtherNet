/* mthl.y - Grammar for MTHL */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mthl_ast.h"  /* AST nodes definitions */

extern int yylex();
extern int yyparse();
extern FILE *yyin;

void yyerror(const char *s);

/* Root of the AST */
ASTNode *ast_root = NULL;
%}

/* Declare types for semantic values */
%union {
    int ival;
    char *sval;
    struct ASTNode *node;
    struct ASTNodeList *nodelist;
}

/* Define tokens */
%token PAGE CONTAINER TEXT RECTANGLE BUTTON
%token VAR IF ELSE FOR
%token POSITION BACKGROUND CONTENT FONT COLOR FILL ALIGN ONCLICK REDRAW
%token LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET
%token SEMICOLON COMMA ASSIGN DOT
%token PLUS MINUS MULTIPLY DIVIDE
%token GT LT GE LE EQ NE AND OR NOT
%token <ival> INTEGER PERCENTAGE
%token <sval> IDENTIFIER STRING_LITERAL COLOR_HEX

/* Define non-terminal types */
%type <node> program page_def statement statement_list expression
%type <node> container_def text_def rectangle_def button_def
%type <node> var_declaration assignment if_statement for_statement
%type <node> property property_list event_handler
%type <nodelist> expression_list

/* Define precedence */
%left OR
%left AND
%left EQ NE
%left GT LT GE LE
%left PLUS MINUS
%left MULTIPLY DIVIDE
%right NOT

%%

program
    : page_def { ast_root = $1; }
    ;

page_def
    : PAGE STRING_LITERAL LBRACE statement_list RBRACE
        { $$ = create_page_node($2, $4); }
    ;

statement_list
    : statement { $$ = $1; }
    | statement statement_list { $$ = chain_statements($1, $2); }
    ;

statement
    : var_declaration SEMICOLON { $$ = $1; }
    | assignment SEMICOLON { $$ = $1; }
    | if_statement { $$ = $1; }
    | for_statement { $$ = $1; }
    | container_def { $$ = $1; }
    | text_def { $$ = $1; }
    | rectangle_def { $$ = $1; }
    | button_def { $$ = $1; }
    | REDRAW LPAREN RPAREN SEMICOLON { $$ = create_redraw_node(); }
    ;

var_declaration
    : VAR IDENTIFIER ASSIGN expression { $$ = create_var_decl_node($2, $4); }
    ;

assignment
    : IDENTIFIER ASSIGN expression { $$ = create_assignment_node($1, $3); }
    | IDENTIFIER LBRACKET expression RBRACKET ASSIGN expression 
        { $$ = create_array_assignment_node($1, $3, $6); }
    ;

if_statement
    : IF LPAREN expression RPAREN LBRACE statement_list RBRACE
        { $$ = create_if_node($3, $6, NULL); }
    | IF LPAREN expression RPAREN LBRACE statement_list RBRACE 
      ELSE LBRACE statement_list RBRACE
        { $$ = create_if_node($3, $6, $10); }
    ;

for_statement
    : FOR LPAREN var_declaration SEMICOLON expression SEMICOLON assignment RPAREN 
      LBRACE statement_list RBRACE
        { $$ = create_for_node($3, $5, $7, $10); }
    ;

container_def
    : CONTAINER LBRACE property_list RBRACE
        { $$ = create_container_node($3); }
    ;

text_def
    : TEXT LBRACE property_list RBRACE
        { $$ = create_text_node($3); }
    ;

rectangle_def
    : RECTANGLE LBRACE property_list RBRACE
        { $$ = create_rectangle_node($3); }
    ;

button_def
    : BUTTON LBRACE property_list RBRACE
        { $$ = create_button_node($3); }
    ;

property_list
    : property { $$ = $1; }
    | property property_list { $$ = chain_properties($1, $2); }
    ;

property
    : POSITION LPAREN expression COMMA expression COMMA expression COMMA expression RPAREN SEMICOLON
        { $$ = create_position_property($3, $5, $7, $9); }
    | BACKGROUND LPAREN expression RPAREN SEMICOLON
        { $$ = create_background_property($3); }
    | CONTENT LPAREN expression RPAREN SEMICOLON
        { $$ = create_content_property($3); }
    | FONT LPAREN expression COMMA expression COMMA expression RPAREN SEMICOLON
        { $$ = create_font_property($3, $5, $7); }
    | COLOR LPAREN expression RPAREN SEMICOLON
        { $$ = create_color_property($3); }
    | FILL LPAREN expression RPAREN SEMICOLON
        { $$ = create_fill_property($3); }
    | ALIGN LPAREN expression RPAREN SEMICOLON
        { $$ = create_align_property($3); }
    | event_handler
        { $$ = $1; }
    | statement
        { $$ = $1; }
    ;

event_handler
    : ONCLICK LBRACE statement_list RBRACE
        { $$ = create_onclick_handler($3); }
    ;

expression
    : INTEGER 
        { $$ = create_integer_node($1); }
    | PERCENTAGE
        { $$ = create_percentage_node($1); }
    | STRING_LITERAL
        { $$ = create_string_node($1); }
    | COLOR_HEX
        { $$ = create_color_node($1); }
    | IDENTIFIER
        { $$ = create_variable_node($1); }
    | IDENTIFIER LBRACKET expression RBRACKET
        { $$ = create_array_access_node($1, $3); }
    | IDENTIFIER DOT IDENTIFIER
        { $$ = create_property_access_node($1, $3); }
    | expression PLUS expression
        { $$ = create_binary_op_node(OP_PLUS, $1, $3); }
    | expression MINUS expression
        { $$ = create_binary_op_node(OP_MINUS, $1, $3); }
    | expression MULTIPLY expression
        { $$ = create_binary_op_node(OP_MULTIPLY, $1, $3); }
    | expression DIVIDE expression
        { $$ = create_binary_op_node(OP_DIVIDE, $1, $3); }
    | expression GT expression
        { $$ = create_binary_op_node(OP_GT, $1, $3); }
    | expression LT expression
        { $$ = create_binary_op_node(OP_LT, $1, $3); }
    | expression GE expression
        { $$ = create_binary_op_node(OP_GE, $1, $3); }
    | expression LE expression
        { $$ = create_binary_op_node(OP_LE, $1, $3); }
    | expression EQ expression
        { $$ = create_binary_op_node(OP_EQ, $1, $3); }
    | expression NE expression
        { $$ = create_binary_op_node(OP_NE, $1, $3); }
    | expression AND expression
        { $$ = create_binary_op_node(OP_AND, $1, $3); }
    | expression OR expression
        { $$ = create_binary_op_node(OP_OR, $1, $3); }
    | NOT expression
        { $$ = create_unary_op_node(OP_NOT, $2); }
    | LPAREN expression RPAREN
        { $$ = $2; }
    | LBRACKET expression_list RBRACKET
        { $$ = create_array_node($2); }
    ;

expression_list
    : expression
        { $$ = create_expression_list($1); }
    | expression COMMA expression_list
        { $$ = add_to_expression_list($3, $1); }
    | /* empty */
        { $$ = create_expression_list(NULL); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            fprintf(stderr, "Cannot open file: %s\n", argv[1]);
            return 1;
        }
        yyin = file;
    }
    
    yyparse();
    
    if (ast_root) {
        interpret_ast(ast_root);  /* Execute the program */
    }
    
    return 0;
}