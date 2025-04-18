#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif /* ifdef DEBUG_PRINT_CODE */

typedef struct {
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_TERNARY,     // ?:
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    ParseFn mixfix;
    Precedence precedence;
} ParseRule;

Parser parser;

Chunk* compiling_chunk;

static Chunk* current_chunk() {
    return compiling_chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {

    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;

        error_at_current(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(Value value) {
    int constant = add_constant(current_chunk(), value);
    if (constant == UINT8_MAX) {
        error("Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}

static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void end_compiler() {
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif /* ifdef DEBUG_PRINT_CODE */
}

static void expression();
static void statement();
static void declaration();

static ParseRule* get_rule(TokenType);
static void parse_precedence(Precedence);

static void binary() {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence) (rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_BANG_EQUAL:      emit_bytes(OP_EQUAL, OP_NOT);   break;
        case TOKEN_EQUAL_EQUAL:     emit_byte(OP_EQUAL);            break;
        case TOKEN_GREATER:         emit_byte(OP_GREATER);          break;
        case TOKEN_GREATER_EQUAL:   emit_bytes(OP_LESS, OP_NOT);    break;
        case TOKEN_LESS:            emit_byte(OP_LESS);             break;
        case TOKEN_LESS_EQUAL:      emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:            emit_byte(OP_ADD);              break;
        case TOKEN_MINUS:           emit_byte(OP_SUBTRACT);         break;
        case TOKEN_STAR:            emit_byte(OP_MULTIPLY);         break;
        case TOKEN_SLASH:           emit_byte(OP_DIVIDE);           break;

        default: return;
    }
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_FALSE:   emit_byte(OP_FALSE);        break;
        case TOKEN_TRUE:    emit_byte(OP_TRUE);         break;
        case TOKEN_NIL:     emit_byte(OP_NIL);          break;

        default: return;
    }
}

static void ternary() {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence) (rule->precedence + 1));
    /*switch (operator_type) {*/
    /*    case TOKEN_QUESTION:    emit_byte(OP_CONDITION) break;*/
    /*    default:*/
    /*};*/
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string() {
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void unary() {
    TokenType operator_type = parser.previous.type;
    
    // Compiling operand
    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_BANG:    emit_byte(OP_NOT);      break;
        case TOKEN_MINUS:   emit_byte(OP_NEGATE);   break;

        default: return; // Unreachable
    }
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping,    NULL,   NULL,          PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,        NULL,   NULL,          PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_COMMA]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_DOT]           = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_MINUS]         = {unary,       binary, NULL,          PREC_TERM},
  [TOKEN_PLUS]          = {NULL,        binary, NULL,          PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_SLASH]         = {NULL,        binary, NULL,          PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,        binary, NULL,          PREC_FACTOR},
  [TOKEN_BANG]          = {unary,       NULL,   NULL,          PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_GREATER_EQUAL] = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_LESS]          = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_LESS_EQUAL]    = {NULL,        binary, NULL,          PREC_EQUALITY},
  [TOKEN_QUESTION]      = {NULL,        NULL,   ternary,       PREC_TERNARY},
  [TOKEN_COLON]         = {NULL,        NULL,   ternary,       PREC_TERNARY},
  [TOKEN_IDENTIFIER]    = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_STRING]        = {string,      NULL,   NULL,          PREC_NONE},
  [TOKEN_NUMBER]        = {number,      NULL,   NULL,          PREC_NONE},
  [TOKEN_AND]           = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_CLASS]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_ELSE]          = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_FALSE]         = {literal,     NULL,   NULL,          PREC_NONE},
  [TOKEN_FOR]           = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_FUN]           = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_IF]            = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_NIL]           = {literal,     NULL,   NULL,          PREC_NONE},
  [TOKEN_OR]            = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_PRINT]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_RETURN]        = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_SUPER]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_THIS]          = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_TRUE]          = {literal,     NULL,   NULL,          PREC_NONE},
  [TOKEN_VAR]           = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_WHILE]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_ERROR]         = {NULL,        NULL,   NULL,          PREC_NONE},
  [TOKEN_EOF]           = {NULL,        NULL,   NULL,          PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseRule* rule = get_rule(parser.previous.type);
        ParseFn infix_rule = rule->infix;
        ParseFn mixfix_rule = rule->mixfix;
        if (mixfix_rule) {
            mixfix_rule();
        } else {
            infix_rule();
        }
    }
}

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void declaration() {
    statement();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    }
}

bool compile(const char* source, Chunk* chunk) {
    init_scanner(source);
    compiling_chunk = chunk;

    parser.had_error = false;
    parser.panic_mode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }
    end_compiler();
    return !parser.had_error;
}
