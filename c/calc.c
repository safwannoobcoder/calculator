/*
 * calc.c — a calculator in C, built on the standard library.
 *
 * The tokenizer and the recursive-descent parser are written by hand, because
 * no library parses *your* grammar. Everything transcendental is libm:
 * sqrt, pow, sin, cos, tan, asin/acos/atan, log, log10, exp, floor, ceil,
 * round, fmod, and tgamma for factorial.
 *
 *   cc -std=c99 -O2 -Wall -Wextra -o calc calc.c -lm
 *   ./calc "2^3^2"
 *   ./calc
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_TOKENS 512
#define MAX_TEXT 32

#define PI_VALUE  3.14159265358979323846
#define E_VALUE   2.71828182845904523536

typedef enum {
    T_NUM, T_NAME, T_OP, T_LPAREN, T_RPAREN, T_END
} TokenKind;

typedef struct {
    TokenKind kind;
    double number;
    char text[MAX_TEXT];
} Token;

typedef double (*MathFn)(double);

typedef struct {
    const char *name;
    MathFn fn;
} Builtin;

static Token g_tokens[MAX_TOKENS];
static int g_ntokens;
static int g_pos;
static double g_ans;
static int g_failed;
static char g_error[160];

static void fail(const char *fmt, ...)
{
    va_list args;

    if (g_failed) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(g_error, sizeof g_error, fmt, args);
    va_end(args);

    g_failed = 1;
}

/* tgamma(n + 1) is n! for real input, so the domain check is ours to do. */
static double fact(double n)
{
    if (!isfinite(n) || n < 0.0 || n != floor(n)) {
        fail("factorial needs a non-negative integer");
        return 0.0;
    }
    if (n > 170.0) {
        fail("factorial is too large to represent");
        return 0.0;
    }
    return tgamma(n + 1.0);
}

/* pow() quietly returns NaN for a negative base with a fractional exponent. */
static double calc_pow(double base, double exponent)
{
    double result = pow(base, exponent);

    if (isnan(result)) {
        fail("result is complex, and this calculator is real-only");
        return 0.0;
    }
    return result;
}

static const Builtin g_builtins[] = {
    {"sqrt", sqrt},
    {"abs", fabs},
    {"sin", sin},
    {"cos", cos},
    {"tan", tan},
    {"asin", asin},
    {"acos", acos},
    {"atan", atan},
    {"ln", log},
    {"log", log},
    {"log10", log10},
    {"exp", exp},
    {"floor", floor},
    {"ceil", ceil},
    {"round", round},
    {"fact", fact},
};

static double call_builtin(const char *name, double argument)
{
    size_t i;

    for (i = 0; i < sizeof g_builtins / sizeof g_builtins[0]; i++) {
        if (strcmp(g_builtins[i].name, name) == 0) {
            return g_builtins[i].fn(argument);
        }
    }

    fail("unknown function '%s'", name);
    return 0.0;
}

static Token *new_token(void)
{
    if (g_ntokens >= MAX_TOKENS) {
        fail("expression is too long");
        return NULL;
    }
    return &g_tokens[g_ntokens++];
}

static void tokenize(const char *source)
{
    g_ntokens = 0;

    while (*source != '\0' && !g_failed) {
        unsigned char ch = (unsigned char)*source;

        if (isspace(ch)) {
            source++;
            continue;
        }

        if (isdigit(ch) || ch == '.') {
            Token *token;
            double value = 0.0;
            int digits = 0;

            while (isdigit((unsigned char)*source)) {
                value = value * 10.0 + (double)(*source - '0');
                source++;
                digits++;
            }
            if (*source == '.') {
                double scale = 0.1;
                source++;
                while (isdigit((unsigned char)*source)) {
                    value += (double)(*source - '0') * scale;
                    scale *= 0.1;
                    source++;
                    digits++;
                }
            }
            if (digits == 0) {
                fail("'%c' is not a number", ch);
                return;
            }

            token = new_token();
            if (token == NULL) {
                return;
            }
            token->kind = T_NUM;
            token->number = value;
            snprintf(token->text, MAX_TEXT, "%.12g", value);
            continue;
        }

        if (isalpha(ch) || ch == '_') {
            char buffer[MAX_TEXT];
            int length = 0;
            Token *token;

            while (isalnum((unsigned char)*source) || *source == '_') {
                if (length < MAX_TEXT - 1) {
                    buffer[length++] = *source;
                }
                source++;
            }
            buffer[length] = '\0';

            token = new_token();
            if (token == NULL) {
                return;
            }
            token->kind = T_NAME;
            token->number = 0.0;
            memcpy(token->text, buffer, (size_t)length + 1);
            continue;
        }

        if (ch == '(' || ch == ')') {
            Token *token = new_token();

            if (token == NULL) {
                return;
            }
            token->kind = (ch == '(') ? T_LPAREN : T_RPAREN;
            token->number = 0.0;
            token->text[0] = (char)ch;
            token->text[1] = '\0';
            source++;
            continue;
        }

        if (strchr("+-*/%^", (int)ch) != NULL) {
            Token *token = new_token();

            if (token == NULL) {
                return;
            }
            token->kind = T_OP;
            token->number = 0.0;
            token->text[0] = (char)ch;
            token->text[1] = '\0';
            source++;
            continue;
        }

        fail("unexpected character '%c'", ch);
        return;
    }

    if (!g_failed) {
        Token *token = new_token();

        if (token != NULL) {
            token->kind = T_END;
            token->number = 0.0;
            token->text[0] = '\0';
        }
    }
}

static double parse_expr(void);

static double parse_atom(void)
{
    Token token;

    if (g_failed) {
        return 0.0;
    }
    if (g_pos >= g_ntokens) {
        fail("unexpected end of expression");
        return 0.0;
    }

    token = g_tokens[g_pos];

    if (token.kind == T_NUM) {
        g_pos++;
        return token.number;
    }

    if (token.kind == T_NAME) {
        g_pos++;
        if (g_pos < g_ntokens && g_tokens[g_pos].kind == T_LPAREN) {
            double argument;

            g_pos++;
            argument = parse_expr();
            if (g_failed) {
                return 0.0;
            }
            if (g_pos >= g_ntokens || g_tokens[g_pos].kind != T_RPAREN) {
                fail("expected ')'");
                return 0.0;
            }
            g_pos++;
            return call_builtin(token.text, argument);
        }
        if (strcmp(token.text, "pi") == 0) {
            return PI_VALUE;
        }
        if (strcmp(token.text, "e") == 0) {
            return E_VALUE;
        }
        if (strcmp(token.text, "tau") == 0) {
            return 2.0 * PI_VALUE;
        }
        if (strcmp(token.text, "ans") == 0) {
            return g_ans;
        }
        fail("unknown name '%s'", token.text);
        return 0.0;
    }

    if (token.kind == T_LPAREN) {
        double value;

        g_pos++;
        value = parse_expr();
        if (g_failed) {
            return 0.0;
        }
        if (g_pos >= g_ntokens || g_tokens[g_pos].kind != T_RPAREN) {
            fail("expected ')'");
            return 0.0;
        }
        g_pos++;
        return value;
    }

    if (token.kind == T_END) {
        fail("unexpected end of expression");
        return 0.0;
    }

    fail("unexpected '%s'", token.text);
    return 0.0;
}

static double parse_power(void)
{
    double base = parse_atom();

    if (g_failed) {
        return 0.0;
    }
    if (g_pos < g_ntokens && g_tokens[g_pos].kind == T_OP && g_tokens[g_pos].text[0] == '^') {
        double exponent;

        g_pos++;
        exponent = parse_unary();
        if (g_failed) {
            return 0.0;
        }
        return calc_pow(base, exponent);
    }
    return base;
}

static double parse_unary(void)
{
    if (g_pos < g_ntokens && g_tokens[g_pos].kind == T_OP &&
        (g_tokens[g_pos].text[0] == '-' || g_tokens[g_pos].text[0] == '+')) {
        char op = g_tokens[g_pos].text[0];
        double value;

        g_pos++;
        value = parse_unary();
        return op == '-' ? -value : value;
    }
    return parse_power();
}

static double parse_term(void)
{
    double value = parse_unary();

    while (!g_failed && g_pos < g_ntokens && g_tokens[g_pos].kind == T_OP &&
           strchr("*/%", g_tokens[g_pos].text[0]) != NULL) {
        char op = g_tokens[g_pos].text[0];
        double rhs;

        g_pos++;
        rhs = parse_unary();
        if (g_failed) {
            return 0.0;
        }

        if (op == '*') {
            value = value * rhs;
        } else if (op == '/') {
            if (rhs == 0.0) {
                fail("division by zero");
                return 0.0;
            }
            value = value / rhs;
        } else {
            if (rhs == 0.0) {
                fail("modulo by zero");
                return 0.0;
            }
            value = fmod(value, rhs);
        }
    }
    return value;
}

static double parse_expr(void)
{
    double value = parse_term();

    while (!g_failed && g_pos < g_ntokens && g_tokens[g_pos].kind == T_OP &&
           (g_tokens[g_pos].text[0] == '+' || g_tokens[g_pos].text[0] == '-')) {
        char op = g_tokens[g_pos].text[0];
        double rhs;

        g_pos++;
        rhs = parse_term();
        if (g_failed) {
            return 0.0;
        }
        value = (op == '+') ? value + rhs : value - rhs;
    }
    return value;
}

static int evaluate(const char *source, double *out)
{
    double value;

    g_failed = 0;
    g_error[0] = '\0';
    g_pos = 0;

    tokenize(source);
    if (g_failed) {
        return 0;
    }

    value = parse_expr();
    if (g_failed) {
        return 0;
    }

    if (g_pos < g_ntokens && g_tokens[g_pos].kind != T_END) {
        fail("unexpected '%s' after a complete expression", g_tokens[g_pos].text);
        return 0;
    }

    *out = value;
    return 1;
}

static void report(double value)
{
    if (isnan(value)) {
        printf("error: result is not a number\n");
        return;
    }
    if (isinf(value)) {
        printf("error: result is too large to represent\n");
        return;
    }
    g_ans = value;
    printf("%.12g\n", value);
}

static const char *HELP_TEXT =
    "operators   + - * / % ^   (^ is a power, right associative: 2^3^2 = 512)\n"
    "parens      (2 + 3) * 4\n"
    "functions   sqrt abs sin cos tan asin acos atan ln log10 exp floor ceil round fact\n"
    "constants   pi  e  tau\n"
    "ans         the previous result\n"
    "commands    help  quit\n";

int main(int argc, char **argv)
{
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    if (argc > 1) {
        char joined[2048] = "";
        double value;
        int i;

        for (i = 1; i < argc; i++) {
            size_t used = strlen(joined);

            if (i > 1) {
                strncat(joined, " ", sizeof joined - used - 1);
            }
            used = strlen(joined);
            strncat(joined, argv[i], sizeof joined - used - 1);
        }

        if (!evaluate(joined, &value)) {
            fprintf(stderr, "error: %s\n", g_error);
            return 1;
        }
        report(value);
        return 0;
    }

    printf("calc — type 'help' for the syntax, or 'quit'\n");

    for (;;) {
        double value;

        printf("> ");
        fflush(stdout);

        length = getline(&line, &capacity, stdin);
        if (length < 0) {
            printf("\n");
            break;
        }

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0) {
            continue;
        }
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) {
            break;
        }
        if (strcmp(line, "help") == 0) {
            fputs(HELP_TEXT, stdout);
            continue;
        }

        if (!evaluate(line, &value)) {
            printf("error: %s\n", g_error);
            continue;
        }
        report(value);
    }

    free(line);
    return 0;
}
