# calculator

A calculator that actually parses its input. No `eval()`, no third-party
packages, standard library only.

The interesting part is `calc.py`: a hand-written tokenizer and recursive-descent
parser. `calc_gui.py` is a thin Tkinter shell over it.

## Run it

```sh
python3 calc.py "2 + 3 * 4"      # 14, one-shot
python3 calc.py                  # interactive
python3 calc_gui.py              # the GUI
```

On Debian, the GUI needs Tk, which is not part of `python3` itself:

```sh
sudo apt install -y python3-tk
```

## Layout

```
calc.py       tokenizer + parser + CLI   <- the part that matters
calc_gui.py   Tkinter GUI, imports calc
```

The GUI does `from calc import evaluate`, so both files must sit in the same
directory.

## The grammar

Precedence lives in the call graph, not in a table:

```
expr   := term (('+' | '-') term)*
term   := unary (('*' | '/' | '%') unary)*
unary  := ('+' | '-') unary | power
power  := atom ('^' unary)?          # right associative
atom   := NUMBER | NAME | NAME '(' expr ')' | '(' expr ')'
```

Consequences worth knowing:

- `2 + 3 * 4` is `14` because `expr` cannot finish without descending into `term`.
- `2^3^2` is `512`, not `64` — the exponent slot calls `unary()` recursively.
- `-2^2` is `-4`, because unary minus binds looser than `^`.

## Known limits, on purpose

- **No implicit multiplication.** `2x` is two tokens; there is no rule joining
  them, and `2x^2` would be ambiguous anyway.
- **`=` is not an operator.** It is a REPL command, not arithmetic.
- **Real numbers only.** `(-8)^(1/3)` reports an error rather than returning a
  complex result.
- **`%` is modulo**, not percent. `7 % 3` is `1`.
- **Functions wrap the whole expression.** With `2+3` on screen, `x²` gives
  `(2+3)^2`, not `3^2`.
