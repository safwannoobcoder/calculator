# calc (C)

The same calculator, in C, using the standard library for the maths.

```sh
cc -std=c99 -O2 -Wall -Wextra -o calc calc.c -lm
./calc "2^3^2"        # 512
./calc                # interactive
```

`-lm` is required: `sqrt`, `pow`, `sin`, `cos`, `log`, `exp` and `tgamma`
all come from libm.

## What is a library here, and what is not

| Part | Written by hand | Library |
|---|---|---|
| Tokenizer | yes | |
| Recursive-descent parser | yes | |
| Grammar / precedence | yes | |
| `and`, `%`, unary minus, right-associative `^` | yes | |
| `sqrt`, `pow`, `sin`, `cos`, `tan`, `asin/acos/atan` | | libm |
| `log`, `log10`, `exp`, `fmod`, `floor`, `ceil`, `round` | | libm |
| `n!` | domain check | libm `tgamma(n+1)` |
| Line input | | POSIX `getline` |
| Printing | | `printf("%.12g")` |

There is no third-party dependency. libm is part of the C standard library,
and writing `sin` yourself gets you a slower, less accurate function than the
one glibc ships — which is a fine exercise, but not a fine default.

## Grammar

```
expr   := term (('+' | '-') term)*
term   := unary (('*' | '/' | '%') unary)*
unary  := ('+' | '-') unary | power
power  := atom ('^' unary)?
      atom   := NUMBER | NAME | NAME '(' expr ')' | '(' expr ')'
```

`2 + 3 * 4` is 14, `2^3^2` is 512, `-2^2` is -4.
