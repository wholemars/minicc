/*
 * minicc - A Simple C Compiler for macOS (ARM64 and x86-64)
 * 
 * Supports:
 * - Integer variables (int)
 * - Arithmetic expressions (+, -, *, /, %)
 * - Comparison operators (==, !=, <, >, <=, >=)
 * - Logical operators (&&, ||, !)
 * - if/else statements
 * - while loops
 * - for loops
 * - Functions with parameters and return values
 * - printf (via libc)
 * - Single-line and multi-line comments
 * - Local variables
 * - Global variables
 * - Arrays (basic support)
 * - String literals
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

// Token types
typedef enum {
    TOK_EOF,
    TOK_INT,        // 'int' keyword
    TOK_IF,         // 'if'
    TOK_ELSE,       // 'else'
    TOK_WHILE,      // 'while'
    TOK_FOR,        // 'for'
    TOK_RETURN,     // 'return'
    TOK_VOID,       // 'void'
    TOK_IDENT,      // identifier
    TOK_NUM,        // number literal
    TOK_STR,        // string literal
    TOK_PLUS,       // +
    TOK_MINUS,      // -
    TOK_STAR,       // *
    TOK_SLASH,      // /
    TOK_PERCENT,    // %
    TOK_ASSIGN,     // =
    TOK_EQ,         // ==
    TOK_NE,         // !=
    TOK_LT,         // <
    TOK_GT,         // >
    TOK_LE,         // <=
    TOK_GE,         // >=
    TOK_AND,        // &&
    TOK_OR,         // ||
    TOK_NOT,        // !
    TOK_LPAREN,     // (
    TOK_RPAREN,     // )
    TOK_LBRACE,     // {
    TOK_RBRACE,     // }
    TOK_LBRACKET,   // [
    TOK_RBRACKET,   // ]
    TOK_SEMI,       // ;
    TOK_COMMA,      // ,
    TOK_AMP,        // &
    TOK_PLUSPLUS,   // ++
    TOK_MINUSMINUS, // --
    TOK_PLUSEQ,     // +=
    TOK_MINUSEQ,    // -=
} TokenType;

typedef struct {
    TokenType type;
    char *str;
    int num;
    int line;
    int col;
} Token;

// AST node types
typedef enum {
    AST_NUM,
    AST_STR,
    AST_VAR,
    AST_BINOP,
    AST_UNOP,
    AST_ASSIGN,
    AST_CALL,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_BLOCK,
    AST_FUNC,
    AST_VARDECL,
    AST_PROGRAM,
    AST_ARRAY_ACCESS,
    AST_ADDR,
} ASTType;

typedef struct AST {
    ASTType type;
    union {
        int num;                    // AST_NUM
        char *str;                  // AST_STR, AST_VAR
        struct {                    // AST_BINOP
            int op;
            struct AST *left;
            struct AST *right;
        } binop;
        struct {                    // AST_UNOP
            int op;
            struct AST *operand;
        } unop;
        struct {                    // AST_ASSIGN
            struct AST *left;
            struct AST *right;
            int op;                 // 0 for =, '+' for +=, etc.
        } assign;
        struct {                    // AST_CALL
            char *name;
            struct AST **args;
            int nargs;
        } call;
        struct {                    // AST_IF
            struct AST *cond;
            struct AST *then_branch;
            struct AST *else_branch;
        } if_stmt;
        struct {                    // AST_WHILE
            struct AST *cond;
            struct AST *body;
        } while_stmt;
        struct {                    // AST_FOR
            struct AST *init;
            struct AST *cond;
            struct AST *update;
            struct AST *body;
        } for_stmt;
        struct {                    // AST_RETURN
            struct AST *value;
        } ret;
        struct {                    // AST_BLOCK
            struct AST **stmts;
            int nstmts;
        } block;
        struct {                    // AST_FUNC
            char *name;
            char **params;
            int nparams;
            struct AST *body;
            int is_void;
        } func;
        struct {                    // AST_VARDECL
            char *name;
            struct AST *init;
            int is_array;
            int array_size;
        } vardecl;
        struct {                    // AST_PROGRAM
            struct AST **funcs;
            int nfuncs;
            struct AST **globals;
            int nglobals;
        } program;
        struct {                    // AST_ARRAY_ACCESS
            char *name;
            struct AST *index;
        } array_access;
        struct {                    // AST_ADDR
            char *name;
        } addr;
    };
} AST;

// Symbol table entry
typedef struct {
    char *name;
    int offset;         // Stack offset for locals, or 0 for globals
    int is_global;
    int is_param;
    int param_index;
    int is_array;
    int array_size;
} Symbol;

// Compiler state
typedef struct {
    char *src;
    int pos;
    int line;
    int col;
    Token cur;
    
    Symbol symbols[256];
    int nsymbols;
    int stack_offset;
    
    FILE *out;
    int label_count;
    
    char *string_literals[256];
    int nstrings;
    
    int is_arm64;       // Architecture detection
    int is_linux;       // OS detection (Linux vs macOS)
} Compiler;

// Error handling
static void error(Compiler *c, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error at line %d, col %d: ", c->line, c->col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

// Lexer
static char peek(Compiler *c) {
    return c->src[c->pos];
}

static char advance(Compiler *c) {
    char ch = c->src[c->pos++];
    if (ch == '\n') {
        c->line++;
        c->col = 1;
    } else {
        c->col++;
    }
    return ch;
}

static void skip_whitespace(Compiler *c) {
    while (1) {
        char ch = peek(c);
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            advance(c);
        } else if (ch == '/' && c->src[c->pos + 1] == '/') {
            // Single-line comment
            while (peek(c) && peek(c) != '\n') advance(c);
        } else if (ch == '/' && c->src[c->pos + 1] == '*') {
            // Multi-line comment
            advance(c); advance(c);
            while (peek(c) && !(peek(c) == '*' && c->src[c->pos + 1] == '/')) {
                advance(c);
            }
            if (peek(c)) { advance(c); advance(c); }
        } else {
            break;
        }
    }
}

static void next_token(Compiler *c) {
    skip_whitespace(c);
    
    c->cur.line = c->line;
    c->cur.col = c->col;
    
    char ch = peek(c);
    
    if (!ch) {
        c->cur.type = TOK_EOF;
        return;
    }
    
    // Identifiers and keywords
    if (isalpha(ch) || ch == '_') {
        int start = c->pos;
        while (isalnum(peek(c)) || peek(c) == '_') advance(c);
        int len = c->pos - start;
        
        c->cur.str = malloc(len + 1);
        strncpy(c->cur.str, c->src + start, len);
        c->cur.str[len] = '\0';
        
        if (strcmp(c->cur.str, "int") == 0) c->cur.type = TOK_INT;
        else if (strcmp(c->cur.str, "void") == 0) c->cur.type = TOK_VOID;
        else if (strcmp(c->cur.str, "if") == 0) c->cur.type = TOK_IF;
        else if (strcmp(c->cur.str, "else") == 0) c->cur.type = TOK_ELSE;
        else if (strcmp(c->cur.str, "while") == 0) c->cur.type = TOK_WHILE;
        else if (strcmp(c->cur.str, "for") == 0) c->cur.type = TOK_FOR;
        else if (strcmp(c->cur.str, "return") == 0) c->cur.type = TOK_RETURN;
        else c->cur.type = TOK_IDENT;
        return;
    }
    
    // Numbers
    if (isdigit(ch)) {
        c->cur.num = 0;
        while (isdigit(peek(c))) {
            c->cur.num = c->cur.num * 10 + (advance(c) - '0');
        }
        c->cur.type = TOK_NUM;
        return;
    }
    
    // String literals
    if (ch == '"') {
        advance(c);
        int start = c->pos;
        while (peek(c) && peek(c) != '"') {
            if (peek(c) == '\\') advance(c);
            advance(c);
        }
        int len = c->pos - start;
        c->cur.str = malloc(len + 1);
        strncpy(c->cur.str, c->src + start, len);
        c->cur.str[len] = '\0';
        if (peek(c) == '"') advance(c);
        c->cur.type = TOK_STR;
        return;
    }
    
    // Operators and punctuation
    advance(c);
    switch (ch) {
        case '+':
            if (peek(c) == '+') { advance(c); c->cur.type = TOK_PLUSPLUS; }
            else if (peek(c) == '=') { advance(c); c->cur.type = TOK_PLUSEQ; }
            else c->cur.type = TOK_PLUS;
            break;
        case '-':
            if (peek(c) == '-') { advance(c); c->cur.type = TOK_MINUSMINUS; }
            else if (peek(c) == '=') { advance(c); c->cur.type = TOK_MINUSEQ; }
            else c->cur.type = TOK_MINUS;
            break;
        case '*': c->cur.type = TOK_STAR; break;
        case '/': c->cur.type = TOK_SLASH; break;
        case '%': c->cur.type = TOK_PERCENT; break;
        case '=':
            if (peek(c) == '=') { advance(c); c->cur.type = TOK_EQ; }
            else c->cur.type = TOK_ASSIGN;
            break;
        case '!':
            if (peek(c) == '=') { advance(c); c->cur.type = TOK_NE; }
            else c->cur.type = TOK_NOT;
            break;
        case '<':
            if (peek(c) == '=') { advance(c); c->cur.type = TOK_LE; }
            else c->cur.type = TOK_LT;
            break;
        case '>':
            if (peek(c) == '=') { advance(c); c->cur.type = TOK_GE; }
            else c->cur.type = TOK_GT;
            break;
        case '&':
            if (peek(c) == '&') { advance(c); c->cur.type = TOK_AND; }
            else c->cur.type = TOK_AMP;
            break;
        case '|':
            if (peek(c) == '|') { advance(c); c->cur.type = TOK_OR; }
            else error(c, "Expected '||'");
            break;
        case '(': c->cur.type = TOK_LPAREN; break;
        case ')': c->cur.type = TOK_RPAREN; break;
        case '{': c->cur.type = TOK_LBRACE; break;
        case '}': c->cur.type = TOK_RBRACE; break;
        case '[': c->cur.type = TOK_LBRACKET; break;
        case ']': c->cur.type = TOK_RBRACKET; break;
        case ';': c->cur.type = TOK_SEMI; break;
        case ',': c->cur.type = TOK_COMMA; break;
        default:
            error(c, "Unexpected character: '%c'", ch);
    }
}

static int accept(Compiler *c, TokenType type) {
    if (c->cur.type == type) {
        next_token(c);
        return 1;
    }
    return 0;
}

static void expect(Compiler *c, TokenType type) {
    if (!accept(c, type)) {
        error(c, "Unexpected token");
    }
}

// AST creation helpers
static AST *new_ast(ASTType type) {
    AST *node = calloc(1, sizeof(AST));
    node->type = type;
    return node;
}

// Forward declarations
static AST *parse_expr(Compiler *c);
static AST *parse_stmt(Compiler *c);

// Parser
static AST *parse_primary(Compiler *c) {
    if (c->cur.type == TOK_NUM) {
        AST *node = new_ast(AST_NUM);
        node->num = c->cur.num;
        next_token(c);
        return node;
    }
    
    if (c->cur.type == TOK_STR) {
        AST *node = new_ast(AST_STR);
        node->str = c->cur.str;
        next_token(c);
        return node;
    }
    
    if (c->cur.type == TOK_IDENT) {
        char *name = c->cur.str;
        next_token(c);
        
        // Function call
        if (c->cur.type == TOK_LPAREN) {
            next_token(c);
            AST *node = new_ast(AST_CALL);
            node->call.name = name;
            node->call.args = malloc(16 * sizeof(AST*));
            node->call.nargs = 0;
            
            while (c->cur.type != TOK_RPAREN) {
                node->call.args[node->call.nargs++] = parse_expr(c);
                if (c->cur.type != TOK_RPAREN) expect(c, TOK_COMMA);
            }
            expect(c, TOK_RPAREN);
            return node;
        }
        
        // Array access
        if (c->cur.type == TOK_LBRACKET) {
            next_token(c);
            AST *node = new_ast(AST_ARRAY_ACCESS);
            node->array_access.name = name;
            node->array_access.index = parse_expr(c);
            expect(c, TOK_RBRACKET);
            return node;
        }
        
        // Variable
        AST *node = new_ast(AST_VAR);
        node->str = name;
        return node;
    }
    
    if (c->cur.type == TOK_LPAREN) {
        next_token(c);
        AST *node = parse_expr(c);
        expect(c, TOK_RPAREN);
        return node;
    }
    
    if (c->cur.type == TOK_AMP) {
        next_token(c);
        if (c->cur.type != TOK_IDENT) error(c, "Expected identifier after &");
        AST *node = new_ast(AST_ADDR);
        node->addr.name = c->cur.str;
        next_token(c);
        return node;
    }
    
    error(c, "Expected expression");
    return NULL;
}

static AST *parse_unary(Compiler *c) {
    if (c->cur.type == TOK_MINUS || c->cur.type == TOK_NOT) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_UNOP);
        node->unop.op = op;
        node->unop.operand = parse_unary(c);
        return node;
    }
    if (c->cur.type == TOK_PLUSPLUS || c->cur.type == TOK_MINUSMINUS) {
        int op = c->cur.type;
        next_token(c);
        if (c->cur.type != TOK_IDENT) error(c, "Expected identifier after ++/--");
        AST *var = new_ast(AST_VAR);
        var->str = c->cur.str;
        next_token(c);
        
        AST *one = new_ast(AST_NUM);
        one->num = 1;
        
        AST *add = new_ast(AST_BINOP);
        add->binop.op = (op == TOK_PLUSPLUS) ? TOK_PLUS : TOK_MINUS;
        add->binop.left = var;
        add->binop.right = one;
        
        AST *assign = new_ast(AST_ASSIGN);
        AST *var2 = new_ast(AST_VAR);
        var2->str = strdup(var->str);
        assign->assign.left = var2;
        assign->assign.right = add;
        assign->assign.op = 0;
        return assign;
    }
    return parse_primary(c);
}

static AST *parse_multiplicative(Compiler *c) {
    AST *left = parse_unary(c);
    while (c->cur.type == TOK_STAR || c->cur.type == TOK_SLASH || c->cur.type == TOK_PERCENT) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = op;
        node->binop.left = left;
        node->binop.right = parse_unary(c);
        left = node;
    }
    return left;
}

static AST *parse_additive(Compiler *c) {
    AST *left = parse_multiplicative(c);
    while (c->cur.type == TOK_PLUS || c->cur.type == TOK_MINUS) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = op;
        node->binop.left = left;
        node->binop.right = parse_multiplicative(c);
        left = node;
    }
    return left;
}

static AST *parse_relational(Compiler *c) {
    AST *left = parse_additive(c);
    while (c->cur.type == TOK_LT || c->cur.type == TOK_GT || 
           c->cur.type == TOK_LE || c->cur.type == TOK_GE) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = op;
        node->binop.left = left;
        node->binop.right = parse_additive(c);
        left = node;
    }
    return left;
}

static AST *parse_equality(Compiler *c) {
    AST *left = parse_relational(c);
    while (c->cur.type == TOK_EQ || c->cur.type == TOK_NE) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = op;
        node->binop.left = left;
        node->binop.right = parse_relational(c);
        left = node;
    }
    return left;
}

static AST *parse_logical_and(Compiler *c) {
    AST *left = parse_equality(c);
    while (c->cur.type == TOK_AND) {
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = TOK_AND;
        node->binop.left = left;
        node->binop.right = parse_equality(c);
        left = node;
    }
    return left;
}

static AST *parse_logical_or(Compiler *c) {
    AST *left = parse_logical_and(c);
    while (c->cur.type == TOK_OR) {
        next_token(c);
        AST *node = new_ast(AST_BINOP);
        node->binop.op = TOK_OR;
        node->binop.left = left;
        node->binop.right = parse_logical_and(c);
        left = node;
    }
    return left;
}

static AST *parse_assignment(Compiler *c) {
    AST *left = parse_logical_or(c);
    
    if (c->cur.type == TOK_ASSIGN || c->cur.type == TOK_PLUSEQ || c->cur.type == TOK_MINUSEQ) {
        int op = c->cur.type;
        next_token(c);
        AST *node = new_ast(AST_ASSIGN);
        node->assign.left = left;
        node->assign.right = parse_assignment(c);
        node->assign.op = (op == TOK_ASSIGN) ? 0 : (op == TOK_PLUSEQ) ? '+' : '-';
        return node;
    }
    
    return left;
}

static AST *parse_expr(Compiler *c) {
    return parse_assignment(c);
}

static AST *parse_block(Compiler *c) {
    expect(c, TOK_LBRACE);
    AST *node = new_ast(AST_BLOCK);
    node->block.stmts = malloc(256 * sizeof(AST*));
    node->block.nstmts = 0;
    
    while (c->cur.type != TOK_RBRACE) {
        node->block.stmts[node->block.nstmts++] = parse_stmt(c);
    }
    expect(c, TOK_RBRACE);
    return node;
}

static AST *parse_stmt(Compiler *c) {
    // Variable declaration
    if (c->cur.type == TOK_INT) {
        next_token(c);
        AST *node = new_ast(AST_VARDECL);
        node->vardecl.name = c->cur.str;
        expect(c, TOK_IDENT);
        
        // Array declaration
        if (c->cur.type == TOK_LBRACKET) {
            next_token(c);
            node->vardecl.is_array = 1;
            node->vardecl.array_size = c->cur.num;
            expect(c, TOK_NUM);
            expect(c, TOK_RBRACKET);
        }
        
        if (c->cur.type == TOK_ASSIGN) {
            next_token(c);
            node->vardecl.init = parse_expr(c);
        }
        expect(c, TOK_SEMI);
        return node;
    }
    
    // If statement
    if (c->cur.type == TOK_IF) {
        next_token(c);
        expect(c, TOK_LPAREN);
        AST *node = new_ast(AST_IF);
        node->if_stmt.cond = parse_expr(c);
        expect(c, TOK_RPAREN);
        node->if_stmt.then_branch = parse_stmt(c);
        if (c->cur.type == TOK_ELSE) {
            next_token(c);
            node->if_stmt.else_branch = parse_stmt(c);
        }
        return node;
    }
    
    // While loop
    if (c->cur.type == TOK_WHILE) {
        next_token(c);
        expect(c, TOK_LPAREN);
        AST *node = new_ast(AST_WHILE);
        node->while_stmt.cond = parse_expr(c);
        expect(c, TOK_RPAREN);
        node->while_stmt.body = parse_stmt(c);
        return node;
    }
    
    // For loop
    if (c->cur.type == TOK_FOR) {
        next_token(c);
        expect(c, TOK_LPAREN);
        AST *node = new_ast(AST_FOR);
        
        // Init
        if (c->cur.type == TOK_INT) {
            next_token(c);
            AST *decl = new_ast(AST_VARDECL);
            decl->vardecl.name = c->cur.str;
            expect(c, TOK_IDENT);
            if (c->cur.type == TOK_ASSIGN) {
                next_token(c);
                decl->vardecl.init = parse_expr(c);
            }
            node->for_stmt.init = decl;
        } else if (c->cur.type != TOK_SEMI) {
            node->for_stmt.init = parse_expr(c);
        }
        expect(c, TOK_SEMI);
        
        // Condition
        if (c->cur.type != TOK_SEMI) {
            node->for_stmt.cond = parse_expr(c);
        }
        expect(c, TOK_SEMI);
        
        // Update
        if (c->cur.type != TOK_RPAREN) {
            node->for_stmt.update = parse_expr(c);
        }
        expect(c, TOK_RPAREN);
        
        node->for_stmt.body = parse_stmt(c);
        return node;
    }
    
    // Return statement
    if (c->cur.type == TOK_RETURN) {
        next_token(c);
        AST *node = new_ast(AST_RETURN);
        if (c->cur.type != TOK_SEMI) {
            node->ret.value = parse_expr(c);
        }
        expect(c, TOK_SEMI);
        return node;
    }
    
    // Block
    if (c->cur.type == TOK_LBRACE) {
        return parse_block(c);
    }
    
    // Expression statement
    AST *expr = parse_expr(c);
    expect(c, TOK_SEMI);
    return expr;
}

static AST *parse_func(Compiler *c) {
    AST *node = new_ast(AST_FUNC);
    
    // Return type
    node->func.is_void = (c->cur.type == TOK_VOID);
    if (c->cur.type == TOK_INT || c->cur.type == TOK_VOID) {
        next_token(c);
    } else {
        error(c, "Expected type");
    }
    
    // Function name
    node->func.name = c->cur.str;
    expect(c, TOK_IDENT);
    
    // Parameters
    expect(c, TOK_LPAREN);
    node->func.params = malloc(16 * sizeof(char*));
    node->func.nparams = 0;
    
    while (c->cur.type != TOK_RPAREN) {
        if (c->cur.type == TOK_INT) next_token(c);
        node->func.params[node->func.nparams++] = c->cur.str;
        expect(c, TOK_IDENT);
        if (c->cur.type != TOK_RPAREN) expect(c, TOK_COMMA);
    }
    expect(c, TOK_RPAREN);
    
    // Body
    node->func.body = parse_block(c);
    
    return node;
}

static AST *parse_program(Compiler *c) {
    AST *node = new_ast(AST_PROGRAM);
    node->program.funcs = malloc(64 * sizeof(AST*));
    node->program.nfuncs = 0;
    node->program.globals = malloc(64 * sizeof(AST*));
    node->program.nglobals = 0;
    
    while (c->cur.type != TOK_EOF) {
        // Check if it's a global variable or function
        if (c->cur.type == TOK_INT || c->cur.type == TOK_VOID) {
            int saved_pos = c->pos;
            int saved_line = c->line;
            int saved_col = c->col;
            TokenType saved_type = c->cur.type;
            char *saved_str = c->cur.str;
            
            next_token(c);  // Skip type
            char *name = c->cur.str;
            next_token(c);  // Skip name
            
            if (c->cur.type == TOK_LPAREN) {
                // It's a function - restore and parse
                c->pos = saved_pos;
                c->line = saved_line;
                c->col = saved_col;
                c->cur.type = saved_type;
                c->cur.str = saved_str;
                // Re-lex to restore state
                c->pos -= strlen(saved_str) + 4;  // Rough estimate
                next_token(c);
                
                node->program.funcs[node->program.nfuncs++] = parse_func(c);
            } else {
                // It's a global variable
                AST *global = new_ast(AST_VARDECL);
                global->vardecl.name = name;
                
                if (c->cur.type == TOK_LBRACKET) {
                    next_token(c);
                    global->vardecl.is_array = 1;
                    global->vardecl.array_size = c->cur.num;
                    expect(c, TOK_NUM);
                    expect(c, TOK_RBRACKET);
                }
                
                if (c->cur.type == TOK_ASSIGN) {
                    next_token(c);
                    global->vardecl.init = parse_expr(c);
                }
                expect(c, TOK_SEMI);
                node->program.globals[node->program.nglobals++] = global;
            }
        } else {
            error(c, "Expected function or variable declaration");
        }
    }
    
    return node;
}

// Simple forward-looking parser for lookahead
static AST *do_parse_program(Compiler *c) {
    AST *node = new_ast(AST_PROGRAM);
    node->program.funcs = malloc(64 * sizeof(AST*));
    node->program.nfuncs = 0;
    node->program.globals = malloc(64 * sizeof(AST*));
    node->program.nglobals = 0;
    
    while (c->cur.type != TOK_EOF) {
        if (c->cur.type == TOK_INT || c->cur.type == TOK_VOID) {
            int is_void = (c->cur.type == TOK_VOID);
            next_token(c);  // type
            
            char *name = c->cur.str;
            next_token(c);  // name
            
            if (c->cur.type == TOK_LPAREN) {
                // Function
                AST *func = new_ast(AST_FUNC);
                func->func.name = name;
                func->func.is_void = is_void;
                
                expect(c, TOK_LPAREN);
                func->func.params = malloc(16 * sizeof(char*));
                func->func.nparams = 0;
                
                while (c->cur.type != TOK_RPAREN) {
                    if (c->cur.type == TOK_INT) next_token(c);
                    func->func.params[func->func.nparams++] = c->cur.str;
                    expect(c, TOK_IDENT);
                    if (c->cur.type != TOK_RPAREN) expect(c, TOK_COMMA);
                }
                expect(c, TOK_RPAREN);
                func->func.body = parse_block(c);
                
                node->program.funcs[node->program.nfuncs++] = func;
            } else {
                // Global variable
                AST *global = new_ast(AST_VARDECL);
                global->vardecl.name = name;
                
                if (c->cur.type == TOK_LBRACKET) {
                    next_token(c);
                    global->vardecl.is_array = 1;
                    global->vardecl.array_size = c->cur.num;
                    expect(c, TOK_NUM);
                    expect(c, TOK_RBRACKET);
                }
                
                if (c->cur.type == TOK_ASSIGN) {
                    next_token(c);
                    global->vardecl.init = parse_expr(c);
                }
                expect(c, TOK_SEMI);
                node->program.globals[node->program.nglobals++] = global;
            }
        } else {
            error(c, "Expected function or variable declaration");
        }
    }
    
    return node;
}

// Symbol table
static Symbol *find_symbol(Compiler *c, const char *name) {
    for (int i = c->nsymbols - 1; i >= 0; i--) {
        if (strcmp(c->symbols[i].name, name) == 0) {
            return &c->symbols[i];
        }
    }
    return NULL;
}

static Symbol *add_symbol(Compiler *c, const char *name, int is_global, int is_param, int param_index) {
    Symbol *sym = &c->symbols[c->nsymbols++];
    sym->name = strdup(name);
    sym->is_global = is_global;
    sym->is_param = is_param;
    sym->param_index = param_index;
    sym->is_array = 0;
    sym->array_size = 0;
    
    if (!is_global && !is_param) {
        c->stack_offset += 8;
        sym->offset = c->stack_offset;
    }
    
    return sym;
}

// JSON AST output helpers
static const char *ast_type_name(ASTType type) {
    switch (type) {
        case AST_NUM: return "NumLiteral";
        case AST_STR: return "StringLiteral";
        case AST_VAR: return "Variable";
        case AST_BINOP: return "BinaryOp";
        case AST_UNOP: return "UnaryOp";
        case AST_ASSIGN: return "Assignment";
        case AST_CALL: return "FunctionCall";
        case AST_IF: return "IfStatement";
        case AST_WHILE: return "WhileLoop";
        case AST_FOR: return "ForLoop";
        case AST_RETURN: return "ReturnStatement";
        case AST_BLOCK: return "Block";
        case AST_FUNC: return "FunctionDecl";
        case AST_VARDECL: return "VarDecl";
        case AST_PROGRAM: return "Program";
        case AST_ARRAY_ACCESS: return "ArrayAccess";
        case AST_ADDR: return "AddressOf";
        default: return "Unknown";
    }
}

static const char *op_to_string(int op) {
    switch (op) {
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_AND: return "&&";
        case TOK_OR: return "||";
        case TOK_NOT: return "!";
        default: return "?";
    }
}

static void print_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(out, "  ");
    }
}

static void print_json_string(FILE *out, const char *str) {
    fprintf(out, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"': fprintf(out, "\\\""); break;
            case '\\': fprintf(out, "\\\\"); break;
            case '\n': fprintf(out, "\\n"); break;
            case '\r': fprintf(out, "\\r"); break;
            case '\t': fprintf(out, "\\t"); break;
            default: fprintf(out, "%c", *p); break;
        }
    }
    fprintf(out, "\"");
}

static void ast_to_json(FILE *out, AST *node, int indent) {
    if (!node) {
        fprintf(out, "null");
        return;
    }

    fprintf(out, "{\n");
    print_indent(out, indent + 1);
    fprintf(out, "\"type\": \"%s\"", ast_type_name(node->type));

    switch (node->type) {
        case AST_NUM:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"value\": %d", node->num);
            break;

        case AST_STR:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"value\": ");
            print_json_string(out, node->str);
            break;

        case AST_VAR:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\"", node->str);
            break;

        case AST_BINOP:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"operator\": \"%s\",\n", op_to_string(node->binop.op));
            print_indent(out, indent + 1);
            fprintf(out, "\"left\": ");
            ast_to_json(out, node->binop.left, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"right\": ");
            ast_to_json(out, node->binop.right, indent + 1);
            break;

        case AST_UNOP:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"operator\": \"%s\",\n", op_to_string(node->unop.op));
            print_indent(out, indent + 1);
            fprintf(out, "\"operand\": ");
            ast_to_json(out, node->unop.operand, indent + 1);
            break;

        case AST_ASSIGN: {
            const char *assign_op = "=";
            if (node->assign.op == '+') assign_op = "+=";
            else if (node->assign.op == '-') assign_op = "-=";
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"operator\": \"%s\",\n", assign_op);
            print_indent(out, indent + 1);
            fprintf(out, "\"left\": ");
            ast_to_json(out, node->assign.left, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"right\": ");
            ast_to_json(out, node->assign.right, indent + 1);
            break;
        }

        case AST_CALL:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\",\n", node->call.name);
            print_indent(out, indent + 1);
            fprintf(out, "\"arguments\": [");
            if (node->call.nargs > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->call.nargs; i++) {
                    print_indent(out, indent + 2);
                    ast_to_json(out, node->call.args[i], indent + 2);
                    if (i < node->call.nargs - 1) fprintf(out, ",");
                    fprintf(out, "\n");
                }
                print_indent(out, indent + 1);
            }
            fprintf(out, "]");
            break;

        case AST_IF:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"condition\": ");
            ast_to_json(out, node->if_stmt.cond, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"then\": ");
            ast_to_json(out, node->if_stmt.then_branch, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"else\": ");
            ast_to_json(out, node->if_stmt.else_branch, indent + 1);
            break;

        case AST_WHILE:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"condition\": ");
            ast_to_json(out, node->while_stmt.cond, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"body\": ");
            ast_to_json(out, node->while_stmt.body, indent + 1);
            break;

        case AST_FOR:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"init\": ");
            ast_to_json(out, node->for_stmt.init, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"condition\": ");
            ast_to_json(out, node->for_stmt.cond, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"update\": ");
            ast_to_json(out, node->for_stmt.update, indent + 1);
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"body\": ");
            ast_to_json(out, node->for_stmt.body, indent + 1);
            break;

        case AST_RETURN:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"value\": ");
            ast_to_json(out, node->ret.value, indent + 1);
            break;

        case AST_BLOCK:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"statements\": [");
            if (node->block.nstmts > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->block.nstmts; i++) {
                    print_indent(out, indent + 2);
                    ast_to_json(out, node->block.stmts[i], indent + 2);
                    if (i < node->block.nstmts - 1) fprintf(out, ",");
                    fprintf(out, "\n");
                }
                print_indent(out, indent + 1);
            }
            fprintf(out, "]");
            break;

        case AST_FUNC:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\",\n", node->func.name);
            print_indent(out, indent + 1);
            fprintf(out, "\"returnType\": \"%s\",\n", node->func.is_void ? "void" : "int");
            print_indent(out, indent + 1);
            fprintf(out, "\"parameters\": [");
            if (node->func.nparams > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->func.nparams; i++) {
                    print_indent(out, indent + 2);
                    fprintf(out, "\"%s\"", node->func.params[i]);
                    if (i < node->func.nparams - 1) fprintf(out, ",");
                    fprintf(out, "\n");
                }
                print_indent(out, indent + 1);
            }
            fprintf(out, "],\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"body\": ");
            ast_to_json(out, node->func.body, indent + 1);
            break;

        case AST_VARDECL:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\",\n", node->vardecl.name);
            print_indent(out, indent + 1);
            fprintf(out, "\"isArray\": %s", node->vardecl.is_array ? "true" : "false");
            if (node->vardecl.is_array) {
                fprintf(out, ",\n");
                print_indent(out, indent + 1);
                fprintf(out, "\"arraySize\": %d", node->vardecl.array_size);
            }
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"initializer\": ");
            ast_to_json(out, node->vardecl.init, indent + 1);
            break;

        case AST_PROGRAM:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"globals\": [");
            if (node->program.nglobals > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->program.nglobals; i++) {
                    print_indent(out, indent + 2);
                    ast_to_json(out, node->program.globals[i], indent + 2);
                    if (i < node->program.nglobals - 1) fprintf(out, ",");
                    fprintf(out, "\n");
                }
                print_indent(out, indent + 1);
            }
            fprintf(out, "],\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"functions\": [");
            if (node->program.nfuncs > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->program.nfuncs; i++) {
                    print_indent(out, indent + 2);
                    ast_to_json(out, node->program.funcs[i], indent + 2);
                    if (i < node->program.nfuncs - 1) fprintf(out, ",");
                    fprintf(out, "\n");
                }
                print_indent(out, indent + 1);
            }
            fprintf(out, "]");
            break;

        case AST_ARRAY_ACCESS:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\",\n", node->array_access.name);
            print_indent(out, indent + 1);
            fprintf(out, "\"index\": ");
            ast_to_json(out, node->array_access.index, indent + 1);
            break;

        case AST_ADDR:
            fprintf(out, ",\n");
            print_indent(out, indent + 1);
            fprintf(out, "\"name\": \"%s\"", node->addr.name);
            break;
    }

    fprintf(out, "\n");
    print_indent(out, indent);
    fprintf(out, "}");
}

// Parse only (for --dump-ast)
static AST *parse_only(Compiler *c, const char *src) {
    c->src = (char *)src;
    c->pos = 0;
    c->line = 1;
    c->col = 1;
    c->nsymbols = 0;
    c->label_count = 0;
    c->nstrings = 0;
    c->stack_offset = 0;

    next_token(c);
    return do_parse_program(c);
}

// Code generation - ARM64
static void emit(Compiler *c, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(c->out, fmt, args);
    fprintf(c->out, "\n");
    va_end(args);
}

static int new_label(Compiler *c) {
    return c->label_count++;
}

static void gen_expr_arm64(Compiler *c, AST *node);

static void gen_expr_arm64(Compiler *c, AST *node) {
    switch (node->type) {
        case AST_NUM:
            if (node->num >= 0 && node->num < 65536) {
                emit(c, "    mov w0, #%d", node->num);
            } else {
                emit(c, "    mov x0, #%d", node->num & 0xFFFF);
                if (node->num > 65535) {
                    emit(c, "    movk x0, #%d, lsl #16", (node->num >> 16) & 0xFFFF);
                }
            }
            break;
            
        case AST_STR: {
            int idx = c->nstrings;
            c->string_literals[c->nstrings++] = node->str;
            emit(c, "    adrp x0, _str%d@PAGE", idx);
            emit(c, "    add x0, x0, _str%d@PAGEOFF", idx);
            break;
        }
        
        case AST_VAR: {
            Symbol *sym = find_symbol(c, node->str);
            if (!sym) error(c, "Undefined variable: %s", node->str);
            
            if (sym->is_global) {
                emit(c, "    adrp x0, _%s@PAGE", node->str);
                emit(c, "    add x0, x0, _%s@PAGEOFF", node->str);
                if (sym->is_array) {
                    // For arrays, return address
                } else {
                    emit(c, "    ldr w0, [x0]");
                }
            } else if (sym->is_param) {
                // Parameters are in x0-x7 on ARM64, saved on stack
                emit(c, "    ldr w0, [x29, #%d]", -(sym->param_index + 1) * 8);
            } else {
                emit(c, "    ldr w0, [x29, #-%d]", sym->offset);
            }
            break;
        }
        
        case AST_ADDR: {
            Symbol *sym = find_symbol(c, node->addr.name);
            if (!sym) error(c, "Undefined variable: %s", node->addr.name);
            
            if (sym->is_global) {
                emit(c, "    adrp x0, _%s@PAGE", node->addr.name);
                emit(c, "    add x0, x0, _%s@PAGEOFF", node->addr.name);
            } else {
                emit(c, "    sub x0, x29, #%d", sym->offset);
            }
            break;
        }
        
        case AST_ARRAY_ACCESS: {
            Symbol *sym = find_symbol(c, node->array_access.name);
            if (!sym) error(c, "Undefined variable: %s", node->array_access.name);
            
            gen_expr_arm64(c, node->array_access.index);
            emit(c, "    str x0, [sp, #-16]!");
            
            if (sym->is_global) {
                emit(c, "    adrp x1, _%s@PAGE", node->array_access.name);
                emit(c, "    add x1, x1, _%s@PAGEOFF", node->array_access.name);
            } else {
                emit(c, "    sub x1, x29, #%d", sym->offset);
            }
            
            emit(c, "    ldr x0, [sp], #16");
            emit(c, "    ldr w0, [x1, x0, lsl #2]");
            break;
        }
        
        case AST_BINOP: {
            gen_expr_arm64(c, node->binop.left);
            emit(c, "    str x0, [sp, #-16]!");
            gen_expr_arm64(c, node->binop.right);
            emit(c, "    mov x1, x0");
            emit(c, "    ldr x0, [sp], #16");
            
            switch (node->binop.op) {
                case TOK_PLUS:  emit(c, "    add w0, w0, w1"); break;
                case TOK_MINUS: emit(c, "    sub w0, w0, w1"); break;
                case TOK_STAR:  emit(c, "    mul w0, w0, w1"); break;
                case TOK_SLASH: emit(c, "    sdiv w0, w0, w1"); break;
                case TOK_PERCENT:
                    emit(c, "    sdiv w2, w0, w1");
                    emit(c, "    msub w0, w2, w1, w0");
                    break;
                case TOK_EQ:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, eq");
                    break;
                case TOK_NE:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, ne");
                    break;
                case TOK_LT:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, lt");
                    break;
                case TOK_GT:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, gt");
                    break;
                case TOK_LE:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, le");
                    break;
                case TOK_GE:
                    emit(c, "    cmp w0, w1");
                    emit(c, "    cset w0, ge");
                    break;
                case TOK_AND: {
                    int lbl = new_label(c);
                    emit(c, "    cbz w0, L%d", lbl);
                    emit(c, "    mov w0, w1");
                    emit(c, "L%d:", lbl);
                    emit(c, "    cmp w0, #0");
                    emit(c, "    cset w0, ne");
                    break;
                }
                case TOK_OR: {
                    int lbl = new_label(c);
                    emit(c, "    cbnz w0, L%d", lbl);
                    emit(c, "    mov w0, w1");
                    emit(c, "L%d:", lbl);
                    emit(c, "    cmp w0, #0");
                    emit(c, "    cset w0, ne");
                    break;
                }
            }
            break;
        }
        
        case AST_UNOP:
            gen_expr_arm64(c, node->unop.operand);
            switch (node->unop.op) {
                case TOK_MINUS: emit(c, "    neg w0, w0"); break;
                case TOK_NOT:
                    emit(c, "    cmp w0, #0");
                    emit(c, "    cset w0, eq");
                    break;
            }
            break;
            
        case AST_ASSIGN: {
            gen_expr_arm64(c, node->assign.right);
            
            AST *left = node->assign.left;
            if (left->type == AST_VAR) {
                Symbol *sym = find_symbol(c, left->str);
                if (!sym) error(c, "Undefined variable: %s", left->str);
                
                if (node->assign.op != 0) {
                    emit(c, "    str x0, [sp, #-16]!");
                    gen_expr_arm64(c, left);
                    emit(c, "    mov w1, w0");
                    emit(c, "    ldr x0, [sp], #16");
                    if (node->assign.op == '+') emit(c, "    add w0, w1, w0");
                    else emit(c, "    sub w0, w1, w0");
                }
                
                if (sym->is_global) {
                    emit(c, "    adrp x1, _%s@PAGE", left->str);
                    emit(c, "    add x1, x1, _%s@PAGEOFF", left->str);
                    emit(c, "    str w0, [x1]");
                } else if (sym->is_param) {
                    emit(c, "    str w0, [x29, #%d]", -(sym->param_index + 1) * 8);
                } else {
                    emit(c, "    str w0, [x29, #-%d]", sym->offset);
                }
            } else if (left->type == AST_ARRAY_ACCESS) {
                emit(c, "    str x0, [sp, #-16]!");
                gen_expr_arm64(c, left->array_access.index);
                emit(c, "    str x0, [sp, #-16]!");
                
                Symbol *sym = find_symbol(c, left->array_access.name);
                if (sym->is_global) {
                    emit(c, "    adrp x1, _%s@PAGE", left->array_access.name);
                    emit(c, "    add x1, x1, _%s@PAGEOFF", left->array_access.name);
                } else {
                    emit(c, "    sub x1, x29, #%d", sym->offset);
                }
                
                emit(c, "    ldr x0, [sp], #16");
                emit(c, "    ldr x2, [sp], #16");
                emit(c, "    str w2, [x1, x0, lsl #2]");
                emit(c, "    mov w0, w2");
            }
            break;
        }
        
        case AST_CALL: {
            // Save arguments on stack in reverse order
            for (int i = node->call.nargs - 1; i >= 0; i--) {
                gen_expr_arm64(c, node->call.args[i]);
                emit(c, "    str x0, [sp, #-16]!");
            }
            // Load arguments into registers
            for (int i = 0; i < node->call.nargs && i < 8; i++) {
                emit(c, "    ldr x%d, [sp], #16", i);
            }
            emit(c, "    bl _%s", node->call.name);
            break;
        }
        
        default:
            error(c, "Cannot generate expression");
    }
}

static void gen_stmt_arm64(Compiler *c, AST *node);

static void gen_stmt_arm64(Compiler *c, AST *node) {
    switch (node->type) {
        case AST_VARDECL: {
            Symbol *sym = add_symbol(c, node->vardecl.name, 0, 0, 0);
            if (node->vardecl.is_array) {
                sym->is_array = 1;
                sym->array_size = node->vardecl.array_size;
                // Allocate space on stack
                c->stack_offset += (node->vardecl.array_size - 1) * 4;
                sym->offset = c->stack_offset;
            }
            if (node->vardecl.init) {
                gen_expr_arm64(c, node->vardecl.init);
                emit(c, "    str w0, [x29, #-%d]", sym->offset);
            }
            break;
        }
        
        case AST_IF: {
            int else_label = new_label(c);
            int end_label = new_label(c);
            
            gen_expr_arm64(c, node->if_stmt.cond);
            emit(c, "    cbz w0, L%d", else_label);
            gen_stmt_arm64(c, node->if_stmt.then_branch);
            emit(c, "    b L%d", end_label);
            emit(c, "L%d:", else_label);
            if (node->if_stmt.else_branch) {
                gen_stmt_arm64(c, node->if_stmt.else_branch);
            }
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_WHILE: {
            int start_label = new_label(c);
            int end_label = new_label(c);
            
            emit(c, "L%d:", start_label);
            gen_expr_arm64(c, node->while_stmt.cond);
            emit(c, "    cbz w0, L%d", end_label);
            gen_stmt_arm64(c, node->while_stmt.body);
            emit(c, "    b L%d", start_label);
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_FOR: {
            int start_label = new_label(c);
            int end_label = new_label(c);
            
            if (node->for_stmt.init) {
                gen_stmt_arm64(c, node->for_stmt.init);
            }
            emit(c, "L%d:", start_label);
            if (node->for_stmt.cond) {
                gen_expr_arm64(c, node->for_stmt.cond);
                emit(c, "    cbz w0, L%d", end_label);
            }
            gen_stmt_arm64(c, node->for_stmt.body);
            if (node->for_stmt.update) {
                gen_expr_arm64(c, node->for_stmt.update);
            }
            emit(c, "    b L%d", start_label);
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_RETURN:
            if (node->ret.value) {
                gen_expr_arm64(c, node->ret.value);
            }
            emit(c, "    mov sp, x29");
            emit(c, "    ldp x29, x30, [sp], #16");
            emit(c, "    ret");
            break;
            
        case AST_BLOCK:
            for (int i = 0; i < node->block.nstmts; i++) {
                gen_stmt_arm64(c, node->block.stmts[i]);
            }
            break;
            
        default:
            gen_expr_arm64(c, node);
            break;
    }
}

static void gen_func_arm64(Compiler *c, AST *node) {
    // Reset local state
    c->stack_offset = 0;
    int saved_nsymbols = c->nsymbols;
    
    emit(c, ".globl _%s", node->func.name);
    emit(c, ".p2align 2");
    emit(c, "_%s:", node->func.name);
    
    // Prologue
    emit(c, "    stp x29, x30, [sp, #-16]!");
    emit(c, "    mov x29, sp");
    emit(c, "    sub sp, sp, #256");  // Reserve stack space
    
    // Save parameters
    for (int i = 0; i < node->func.nparams; i++) {
        add_symbol(c, node->func.params[i], 0, 1, i);
        emit(c, "    str x%d, [x29, #%d]", i, -(i + 1) * 8);
    }
    // Local variables start after parameters
    c->stack_offset = node->func.nparams * 8;
    
    // Generate body
    gen_stmt_arm64(c, node->func.body);
    
    // Epilogue (in case no return)
    emit(c, "    mov sp, x29");
    emit(c, "    ldp x29, x30, [sp], #16");
    emit(c, "    ret");
    emit(c, "");
    
    // Restore symbol table
    c->nsymbols = saved_nsymbols;
}

static void gen_program_arm64(Compiler *c, AST *node) {
    // First, add all global variables to symbol table so they can be referenced
    for (int i = 0; i < node->program.nglobals; i++) {
        AST *global = node->program.globals[i];
        Symbol *sym = add_symbol(c, global->vardecl.name, 1, 0, 0);
        if (global->vardecl.is_array) {
            sym->is_array = 1;
            sym->array_size = global->vardecl.array_size;
        }
    }
    
    if (c->is_linux) {
        emit(c, ".section .text");
    } else {
        emit(c, ".section __TEXT,__text");
    }
    emit(c, "");
    
    // Generate functions
    for (int i = 0; i < node->program.nfuncs; i++) {
        gen_func_arm64(c, node->program.funcs[i]);
    }
    
    // Generate data section
    if (c->is_linux) {
        emit(c, ".section .data");
    } else {
        emit(c, ".section __DATA,__data");
    }
    
    // Global variables
    for (int i = 0; i < node->program.nglobals; i++) {
        AST *global = node->program.globals[i];
        const char *prefix = c->is_linux ? "" : "_";
        emit(c, ".globl %s%s", prefix, global->vardecl.name);
        emit(c, ".p2align 2");
        emit(c, "%s%s:", prefix, global->vardecl.name);
        
        if (global->vardecl.is_array) {
            emit(c, "    .zero %d", global->vardecl.array_size * 4);
        } else if (global->vardecl.init) {
            emit(c, "    .long %d", global->vardecl.init->num);
        } else {
            emit(c, "    .long 0");
        }
        emit(c, "");
    }
    
    // String literals
    if (c->is_linux) {
        emit(c, ".section .rodata");
    } else {
        emit(c, ".section __TEXT,__cstring");
    }
    const char *prefix = c->is_linux ? "" : "_";
    for (int i = 0; i < c->nstrings; i++) {
        emit(c, "%sstr%d:", prefix, i);
        emit(c, "    .asciz \"%s\"", c->string_literals[i]);
    }
}

// Code generation - x86-64
static const char *sym_prefix(Compiler *c) {
    return c->is_linux ? "" : "_";
}

static void gen_expr_x64(Compiler *c, AST *node);

static void gen_expr_x64(Compiler *c, AST *node) {
    switch (node->type) {
        case AST_NUM:
            emit(c, "    movl $%d, %%eax", node->num);
            break;
            
        case AST_STR: {
            int idx = c->nstrings;
            c->string_literals[c->nstrings++] = node->str;
            emit(c, "    leaq %sstr%d(%%rip), %%rax", sym_prefix(c), idx);
            break;
        }
        
        case AST_VAR: {
            Symbol *sym = find_symbol(c, node->str);
            if (!sym) error(c, "Undefined variable: %s", node->str);
            
            if (sym->is_global) {
                if (sym->is_array) {
                    emit(c, "    leaq %s%s(%%rip), %%rax", sym_prefix(c), node->str);
                } else {
                    emit(c, "    movl %s%s(%%rip), %%eax", sym_prefix(c), node->str);
                }
            } else if (sym->is_param) {
                emit(c, "    movl %d(%%rbp), %%eax", -(sym->param_index + 1) * 8);
            } else {
                emit(c, "    movl -%d(%%rbp), %%eax", sym->offset);
            }
            break;
        }
        
        case AST_ADDR: {
            Symbol *sym = find_symbol(c, node->addr.name);
            if (!sym) error(c, "Undefined variable: %s", node->addr.name);
            
            if (sym->is_global) {
                emit(c, "    leaq %s%s(%%rip), %%rax", sym_prefix(c), node->addr.name);
            } else {
                emit(c, "    leaq -%d(%%rbp), %%rax", sym->offset);
            }
            break;
        }
        
        case AST_ARRAY_ACCESS: {
            Symbol *sym = find_symbol(c, node->array_access.name);
            if (!sym) error(c, "Undefined variable: %s", node->array_access.name);
            
            gen_expr_x64(c, node->array_access.index);
            emit(c, "    pushq %%rax");
            
            if (sym->is_global) {
                emit(c, "    leaq %s%s(%%rip), %%rcx", sym_prefix(c), node->array_access.name);
            } else {
                emit(c, "    leaq -%d(%%rbp), %%rcx", sym->offset);
            }
            
            emit(c, "    popq %%rax");
            emit(c, "    movl (%%rcx,%%rax,4), %%eax");
            break;
        }
        
        case AST_BINOP: {
            gen_expr_x64(c, node->binop.left);
            emit(c, "    pushq %%rax");
            gen_expr_x64(c, node->binop.right);
            emit(c, "    movl %%eax, %%ecx");
            emit(c, "    popq %%rax");
            
            switch (node->binop.op) {
                case TOK_PLUS:  emit(c, "    addl %%ecx, %%eax"); break;
                case TOK_MINUS: emit(c, "    subl %%ecx, %%eax"); break;
                case TOK_STAR:  emit(c, "    imull %%ecx, %%eax"); break;
                case TOK_SLASH:
                    emit(c, "    cltd");
                    emit(c, "    idivl %%ecx");
                    break;
                case TOK_PERCENT:
                    emit(c, "    cltd");
                    emit(c, "    idivl %%ecx");
                    emit(c, "    movl %%edx, %%eax");
                    break;
                case TOK_EQ:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    sete %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_NE:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    setne %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_LT:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    setl %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_GT:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    setg %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_LE:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    setle %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_GE:
                    emit(c, "    cmpl %%ecx, %%eax");
                    emit(c, "    setge %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                case TOK_AND: {
                    int lbl = new_label(c);
                    emit(c, "    testl %%eax, %%eax");
                    emit(c, "    je L%d", lbl);
                    emit(c, "    movl %%ecx, %%eax");
                    emit(c, "L%d:", lbl);
                    emit(c, "    testl %%eax, %%eax");
                    emit(c, "    setne %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                }
                case TOK_OR: {
                    int lbl = new_label(c);
                    emit(c, "    testl %%eax, %%eax");
                    emit(c, "    jne L%d", lbl);
                    emit(c, "    movl %%ecx, %%eax");
                    emit(c, "L%d:", lbl);
                    emit(c, "    testl %%eax, %%eax");
                    emit(c, "    setne %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
                }
            }
            break;
        }
        
        case AST_UNOP:
            gen_expr_x64(c, node->unop.operand);
            switch (node->unop.op) {
                case TOK_MINUS: emit(c, "    negl %%eax"); break;
                case TOK_NOT:
                    emit(c, "    testl %%eax, %%eax");
                    emit(c, "    sete %%al");
                    emit(c, "    movzbl %%al, %%eax");
                    break;
            }
            break;
            
        case AST_ASSIGN: {
            gen_expr_x64(c, node->assign.right);
            
            AST *left = node->assign.left;
            if (left->type == AST_VAR) {
                Symbol *sym = find_symbol(c, left->str);
                if (!sym) error(c, "Undefined variable: %s", left->str);
                
                if (node->assign.op != 0) {
                    emit(c, "    pushq %%rax");
                    gen_expr_x64(c, left);
                    emit(c, "    movl %%eax, %%ecx");
                    emit(c, "    popq %%rax");
                    if (node->assign.op == '+') emit(c, "    addl %%ecx, %%eax");
                    else emit(c, "    subl %%eax, %%ecx\n    movl %%ecx, %%eax");
                }
                
                if (sym->is_global) {
                    emit(c, "    movl %%eax, %s%s(%%rip)", sym_prefix(c), left->str);
                } else if (sym->is_param) {
                    emit(c, "    movl %%eax, %d(%%rbp)", -(sym->param_index + 1) * 8);
                } else {
                    emit(c, "    movl %%eax, -%d(%%rbp)", sym->offset);
                }
            } else if (left->type == AST_ARRAY_ACCESS) {
                emit(c, "    pushq %%rax");
                gen_expr_x64(c, left->array_access.index);
                emit(c, "    pushq %%rax");
                
                Symbol *sym = find_symbol(c, left->array_access.name);
                if (sym->is_global) {
                    emit(c, "    leaq %s%s(%%rip), %%rcx", sym_prefix(c), left->array_access.name);
                } else {
                    emit(c, "    leaq -%d(%%rbp), %%rcx", sym->offset);
                }
                
                emit(c, "    popq %%rax");
                emit(c, "    popq %%rdx");
                emit(c, "    movl %%edx, (%%rcx,%%rax,4)");
                emit(c, "    movl %%edx, %%eax");
            }
            break;
        }
        
        case AST_CALL: {
            // x86-64 calling convention: rdi, rsi, rdx, rcx, r8, r9
            const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            
            // Save arguments on stack in reverse order
            for (int i = node->call.nargs - 1; i >= 0; i--) {
                gen_expr_x64(c, node->call.args[i]);
                emit(c, "    pushq %%rax");
            }
            // Load arguments into registers
            for (int i = 0; i < node->call.nargs && i < 6; i++) {
                emit(c, "    popq %%%s", regs[i]);
            }
            // Align stack to 16 bytes before call - save rbx first since it's callee-saved
            emit(c, "    pushq %%rbx");  // Save rbx (callee-saved)
            emit(c, "    movq %%rsp, %%rbx");
            emit(c, "    andq $-16, %%rsp");
            emit(c, "    xorl %%eax, %%eax");  // For variadic functions
            emit(c, "    callq %s%s", sym_prefix(c), node->call.name);
            emit(c, "    movq %%rbx, %%rsp");
            emit(c, "    popq %%rbx");  // Restore rbx
            break;
        }
        
        default:
            error(c, "Cannot generate expression");
    }
}

static void gen_stmt_x64(Compiler *c, AST *node);

static void gen_stmt_x64(Compiler *c, AST *node) {
    switch (node->type) {
        case AST_VARDECL: {
            Symbol *sym = add_symbol(c, node->vardecl.name, 0, 0, 0);
            if (node->vardecl.is_array) {
                sym->is_array = 1;
                sym->array_size = node->vardecl.array_size;
                c->stack_offset += (node->vardecl.array_size - 1) * 4;
                sym->offset = c->stack_offset;
            }
            if (node->vardecl.init) {
                gen_expr_x64(c, node->vardecl.init);
                emit(c, "    movl %%eax, -%d(%%rbp)", sym->offset);
            }
            break;
        }
        
        case AST_IF: {
            int else_label = new_label(c);
            int end_label = new_label(c);
            
            gen_expr_x64(c, node->if_stmt.cond);
            emit(c, "    testl %%eax, %%eax");
            emit(c, "    je L%d", else_label);
            gen_stmt_x64(c, node->if_stmt.then_branch);
            emit(c, "    jmp L%d", end_label);
            emit(c, "L%d:", else_label);
            if (node->if_stmt.else_branch) {
                gen_stmt_x64(c, node->if_stmt.else_branch);
            }
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_WHILE: {
            int start_label = new_label(c);
            int end_label = new_label(c);
            
            emit(c, "L%d:", start_label);
            gen_expr_x64(c, node->while_stmt.cond);
            emit(c, "    testl %%eax, %%eax");
            emit(c, "    je L%d", end_label);
            gen_stmt_x64(c, node->while_stmt.body);
            emit(c, "    jmp L%d", start_label);
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_FOR: {
            int start_label = new_label(c);
            int end_label = new_label(c);
            
            if (node->for_stmt.init) {
                gen_stmt_x64(c, node->for_stmt.init);
            }
            emit(c, "L%d:", start_label);
            if (node->for_stmt.cond) {
                gen_expr_x64(c, node->for_stmt.cond);
                emit(c, "    testl %%eax, %%eax");
                emit(c, "    je L%d", end_label);
            }
            gen_stmt_x64(c, node->for_stmt.body);
            if (node->for_stmt.update) {
                gen_expr_x64(c, node->for_stmt.update);
            }
            emit(c, "    jmp L%d", start_label);
            emit(c, "L%d:", end_label);
            break;
        }
        
        case AST_RETURN:
            if (node->ret.value) {
                gen_expr_x64(c, node->ret.value);
            }
            emit(c, "    movq %%rbp, %%rsp");
            emit(c, "    popq %%rbp");
            emit(c, "    retq");
            break;
            
        case AST_BLOCK:
            for (int i = 0; i < node->block.nstmts; i++) {
                gen_stmt_x64(c, node->block.stmts[i]);
            }
            break;
            
        default:
            gen_expr_x64(c, node);
            break;
    }
}

static void gen_func_x64(Compiler *c, AST *node) {
    c->stack_offset = 0;
    int saved_nsymbols = c->nsymbols;
    const char *prefix = c->is_linux ? "" : "_";
    
    emit(c, ".globl %s%s", prefix, node->func.name);
    emit(c, "%s%s:", prefix, node->func.name);
    
    // Prologue
    emit(c, "    pushq %%rbp");
    emit(c, "    movq %%rsp, %%rbp");
    emit(c, "    subq $256, %%rsp");
    
    // Save parameters - and track their stack usage
    const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    for (int i = 0; i < node->func.nparams && i < 6; i++) {
        add_symbol(c, node->func.params[i], 0, 1, i);
        emit(c, "    movq %%%s, %d(%%rbp)", regs[i], -(i + 1) * 8);
    }
    // Local variables start after parameters
    c->stack_offset = node->func.nparams * 8;
    
    gen_stmt_x64(c, node->func.body);
    
    // Epilogue
    emit(c, "    movq %%rbp, %%rsp");
    emit(c, "    popq %%rbp");
    emit(c, "    retq");
    emit(c, "");
    
    c->nsymbols = saved_nsymbols;
}

static void gen_program_x64(Compiler *c, AST *node) {
    // First, add all global variables to symbol table so they can be referenced
    for (int i = 0; i < node->program.nglobals; i++) {
        AST *global = node->program.globals[i];
        Symbol *sym = add_symbol(c, global->vardecl.name, 1, 0, 0);
        if (global->vardecl.is_array) {
            sym->is_array = 1;
            sym->array_size = global->vardecl.array_size;
        }
    }
    
    if (c->is_linux) {
        emit(c, ".section .text");
    } else {
        emit(c, ".section __TEXT,__text");
    }
    emit(c, "");
    
    for (int i = 0; i < node->program.nfuncs; i++) {
        gen_func_x64(c, node->program.funcs[i]);
    }
    
    if (c->is_linux) {
        emit(c, ".section .data");
    } else {
        emit(c, ".section __DATA,__data");
    }
    
    const char *prefix = c->is_linux ? "" : "_";
    
    for (int i = 0; i < node->program.nglobals; i++) {
        AST *global = node->program.globals[i];
        emit(c, ".globl %s%s", prefix, global->vardecl.name);
        emit(c, "%s%s:", prefix, global->vardecl.name);
        
        if (global->vardecl.is_array) {
            emit(c, "    .zero %d", global->vardecl.array_size * 4);
        } else if (global->vardecl.init) {
            emit(c, "    .long %d", global->vardecl.init->num);
        } else {
            emit(c, "    .long 0");
        }
        emit(c, "");
    }
    
    if (c->is_linux) {
        emit(c, ".section .rodata");
    } else {
        emit(c, ".section __TEXT,__cstring");
    }
    for (int i = 0; i < c->nstrings; i++) {
        emit(c, "%sstr%d:", prefix, i);
        emit(c, "    .asciz \"%s\"", c->string_literals[i]);
    }
}

// Main compiler function
static void compile(Compiler *c, const char *src, FILE *out) {
    c->src = (char *)src;
    c->pos = 0;
    c->line = 1;
    c->col = 1;
    c->out = out;
    c->nsymbols = 0;
    c->label_count = 0;
    c->nstrings = 0;
    c->stack_offset = 0;
    
    // Detect architecture
#if defined(__aarch64__) || defined(__arm64__)
    c->is_arm64 = 1;
#else
    c->is_arm64 = 0;
#endif

    // Detect OS
#if defined(__linux__)
    c->is_linux = 1;
#else
    c->is_linux = 0;
#endif
    
    next_token(c);
    AST *program = do_parse_program(c);
    
    if (c->is_arm64) {
        gen_program_arm64(c, program);
    } else {
        gen_program_x64(c, program);
    }
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.c> [-o output] [-S] [--dump-ast]\n", argv[0]);
        fprintf(stderr, "  -o output   Specify output file name\n");
        fprintf(stderr, "  -S          Output assembly only (no linking)\n");
        fprintf(stderr, "  --dump-ast  Output AST as JSON (no compilation)\n");
        return 1;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    int asm_only = 0;
    int dump_ast = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-S") == 0) {
            asm_only = 1;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = 1;
        } else {
            input_file = argv[i];
        }
    }
    
    if (!input_file) {
        fprintf(stderr, "No input file specified\n");
        return 1;
    }
    
    // Determine output names
    char asm_file[256];
    char exec_file[256];
    
    if (output_file) {
        if (asm_only) {
            strcpy(asm_file, output_file);
        } else {
            strcpy(exec_file, output_file);
            snprintf(asm_file, sizeof(asm_file), "%s.s", output_file);
        }
    } else {
        // Default: replace .c with .s and executable name
        strcpy(asm_file, input_file);
        char *dot = strrchr(asm_file, '.');
        if (dot) *dot = '\0';
        strcpy(exec_file, asm_file);
        strcat(asm_file, ".s");
    }
    
    char *src = read_file(input_file);

    // Handle --dump-ast option
    if (dump_ast) {
        Compiler compiler = {0};
        AST *program = parse_only(&compiler, src);

        FILE *out = stdout;
        if (output_file) {
            out = fopen(output_file, "w");
            if (!out) {
                fprintf(stderr, "Cannot open output file: %s\n", output_file);
                return 1;
            }
        }

        ast_to_json(out, program, 0);
        fprintf(out, "\n");

        if (output_file) {
            fclose(out);
            printf("Generated AST JSON: %s\n", output_file);
        }
        return 0;
    }

    FILE *out = fopen(asm_file, "w");
    if (!out) {
        fprintf(stderr, "Cannot open output file: %s\n", asm_file);
        return 1;
    }

    Compiler compiler = {0};
    compile(&compiler, src, out);
    fclose(out);

    printf("Generated assembly: %s\n", asm_file);
    
    if (!asm_only) {
        // Assemble and link
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cc -o %s %s -lc 2>&1", exec_file, asm_file);
        
        printf("Assembling and linking...\n");
        int ret = system(cmd);
        
        if (ret == 0) {
            printf("Created executable: %s\n", exec_file);
        } else {
            fprintf(stderr, "Linking failed\n");
            return 1;
        }
    }
    
    return 0;
}
