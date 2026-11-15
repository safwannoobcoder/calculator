#!/usr/bin/env python3
"""
calc.py — a calculator with a real parser. No eval().

    python3 calc.py                    # interactive
    python3 calc.py "2 + 3 * 4"        # one-shot

    expr   := term (('+' | '-') term)*
    term   := unary (('*' | '/' | '%') unary)*
    unary  := ('+' | '-') unary | power
    power  := atom ('^' unary)?
    atom   := NUMBER | NAME | NAME '(' expr ')' | '(' expr ')'
"""

import math
import sys


class CalcError(Exception):
    """Anything the user did wrong. Never escapes to a traceback."""


def tokenize(source):
    tokens = []
    i = 0
    while i < len(source):
        char = source[i]

        if char.isspace():
            i += 1
            continue

        if char.isdigit() or char == ".":
            j = i
            while j < len(source) and (source[j].isdigit() or source[j] == "."):
                j += 1
            text = source[i:j]
            try:
                value = float(text)
            except ValueError:
                raise CalcError("%r is not a number" % text) from None
            tokens.append(("NUM", value))
            i = j
            continue

        if char.isalpha() or char == "_":
            j = i
            while j < len(source) and (source[j].isalnum() or source[j] == "_"):
                j += 1
            tokens.append(("NAME", source[i:j]))
            i = j
            continue

        if char in "+-*/%^":
            tokens.append(("OP", char))
            i += 1
            continue

        if char == "(":
            tokens.append(("LPAREN", char))
            i += 1
            continue

        if char == ")":
            tokens.append(("RPAREN", char))
            i += 1
            continue

        raise CalcError("unexpected character %r" % char)

    return tokens


def _factorial(n):
    """n! for a non-negative integer; rejects 3.5 and -2 the way maths does."""
    if n < 0 or n != int(n):
        raise ValueError("factorial needs a non-negative integer")
    return float(math.factorial(int(n)))


FUNCTIONS = {
    "sqrt": math.sqrt,
    "abs": abs,
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "log": math.log,
    "log10": math.log10,
    "exp": math.exp,
    "floor": math.floor,
    "ceil": math.ceil,
    "round": round,
    "fact": _factorial,
}


class Parser:
    def __init__(self, tokens, env):
        self.tokens = tokens
        self.pos = 0
        self.env = env

    def peek(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else (None, None)

    def take(self):
        token = self.peek()
        self.pos += 1
        return token

    def expect(self, kind):
        kind_found, value = self.take()
        if kind_found != kind:
            found = "end of input" if kind_found is None else repr(value)
            raise CalcError("expected %s, found %s" % (kind, found))
        return value

    def parse(self):
        value = self.expr()
        if self.pos != len(self.tokens):
            kind, token = self.peek()
            raise CalcError("unexpected %r after a complete expression" % (token,))
        return value

    def expr(self):
        value = self.term()
        while True:
            kind, op = self.peek()
            if kind == "OP" and op in "+-":
                self.take()
                rhs = self.term()
                value = value + rhs if op == "+" else value - rhs
            else:
                return value

    def term(self):
        value = self.unary()
        while True:
            kind, op = self.peek()
            if kind != "OP" or op not in "*/%":
                return value
            self.take()
            rhs = self.unary()
            if op == "*":
                value = value * rhs
            elif op == "/":
                if rhs == 0:
                    raise CalcError("division by zero")
                value = value / rhs
            else:
                if rhs == 0:
                    raise CalcError("modulo by zero")
                value = math.fmod(value, rhs)

    def unary(self):
        kind, op = self.peek()
        if kind == "OP" and op in "+-":
            self.take()
            value = self.unary()
            return -value if op == "-" else value
        return self.power()

    def power(self):
        base = self.atom()
        kind, op = self.peek()
        if kind == "OP" and op == "^":
            self.take()
            exponent = self.unary()
            try:
                result = base ** exponent
            except OverflowError:
                raise CalcError("result is too large to represent") from None
            if isinstance(result, complex):
                raise CalcError("result is complex, and this calculator is real-only")
            return result
        return base

    def atom(self):
        kind, value = self.take()

        if kind == "NUM":
            return value

        if kind == "NAME":
            if self.peek()[0] == "LPAREN":
                self.take()
                argument = self.expr()
                self.expect("RPAREN")
                return self.call(value, argument)
            if value in self.env:
                return self.env[value]
            raise CalcError("unknown name %r" % (value,))

        if kind == "LPAREN":
            inner = self.expr()
            self.expect("RPAREN")
            return inner

        if kind is None:
            raise CalcError("unexpected end of expression")

        raise CalcError("unexpected %r" % (value,))

    def call(self, name, argument):
        function = FUNCTIONS.get(name)
        if function is None:
            raise CalcError("unknown function %r (try: %s)"
                            % (name, ", ".join(sorted(FUNCTIONS))))
        try:
            return function(argument)
        except ValueError as exc:
            raise CalcError("%s(%g): %s" % (name, argument, exc)) from None


def evaluate(source, env):
    return Parser(tokenize(source), env).parse()


def format_number(value):
    if value == 0:
        return "0"
    if math.isnan(value):
        return "nan"
    if math.isinf(value):
        return "inf" if value > 0 else "-inf"
    return "%.12g" % value


HELP = """\
operators   + - * / % ^     (^ is a power, right associative: 2^3^2 = 512)
parens      (2 + 3) * 4
functions   sqrt abs sin cos tan asin acos atan log log10 exp floor ceil round fact
constants   pi  e  tau
ans         the previous result
commands    help  quit"""


def main(argv):
    env = {"pi": math.pi, "e": math.e, "tau": math.tau, "ans": 0.0}

    if argv:
        try:
            print(format_number(evaluate(" ".join(argv), env)))
            return 0
        except CalcError as exc:
            print("error: %s" % exc, file=sys.stderr)
            return 1

    print("calc — type 'help' for the syntax, or 'quit'")
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return 0

        if not line:
            continue
        if line in ("quit", "exit", "q"):
            return 0
        if line == "help":
            print(HELP)
            continue

        try:
            value = evaluate(line, env)
        except CalcError as exc:
            print("error: %s" % exc)
            continue

        env["ans"] = value
        print(format_number(value))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
