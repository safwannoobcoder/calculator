#!/usr/bin/env python3
"""
calc_gui.py — a Windows 11 shaped front end for the parser in calc.py.

Standard mode is 4x6 keys, scientific is 6x6, and the window resizes to match.
All arithmetic lives in calc.py; this file only does pixels and keystrokes.

    python3 calc_gui.py
"""

import tkinter as tk
from tkinter import font as tkfont

from calc import CalcError, evaluate, format_number

# --- palette, loosely the Windows 11 calculator -----------------------
BG = "#f3f3f3"
SURFACE = "#ffffff"
SURFACE_ALT = "#ededed"
HOVER = "#f7f7f7"
HOVER_ALT = "#e3e3e3"
BORDER = "#e0e0e0"
TEXT = "#191919"
MUTED = "#6b6b6b"
ACCENT = "#0067c0"
ACCENT_HOVER = "#1975c5"
DANGER = "#c42b1c"

STANDARD_SIZE = (330, 520)
SCIENTIFIC_SIZE = (520, 600)

# Key specs: (label, action, payload, kind[, alt])
#   action: ins | wrap | clear | clear_all | back | negate | equals | second
#   kind:   num | func | op | equals
#   alt:    (label, payload) — used while "2nd" is active

STANDARD_KEYS = [
    [("mod", "ins", "%", "func"), ("CE", "clear", None, "func"),
     ("C", "clear_all", None, "func"), ("⌫", "back", None, "func")],
    [("1/x", "wrap", ("1/(", ")"), "func"), ("x²", "wrap", ("(", ")^2"), "func"),
     ("√", "wrap", ("sqrt(", ")"), "func"), ("÷", "ins", "/", "op")],
    [("7", "ins", "7", "num"), ("8", "ins", "8", "num"),
     ("9", "ins", "9", "num"), ("×", "ins", "*", "op")],
    [("4", "ins", "4", "num"), ("5", "ins", "5", "num"),
     ("6", "ins", "6", "num"), ("−", "ins", "-", "op")],
    [("1", "ins", "1", "num"), ("2", "ins", "2", "num"),
     ("3", "ins", "3", "num"), ("+", "ins", "+", "op")],
    [("±", "negate", None, "func"), ("0", "ins", "0", "num"),
     (".", "ins", ".", "num"), ("=", "equals", None, "equals")],
]

SCIENTIFIC_KEYS = [
    [("2nd", "second", None, "func"), ("π", "ins", "pi", "func"),
     ("e", "ins", "e", "func"), ("C", "clear_all", None, "func"),
     ("⌫", "back", None, "func"), ("÷", "ins", "/", "op")],
    [("x²", "wrap", ("(", ")^2"), "func"), ("1/x", "wrap", ("1/(", ")"), "func"),
     ("|x|", "wrap", ("abs(", ")"), "func"),
     ("7", "ins", "7", "num"), ("8", "ins", "8", "num"), ("9", "ins", "9", "num")],
    [("√", "wrap", ("sqrt(", ")"), "func"),
     ("x^y", "ins", "^", "func"), ("n!", "wrap", ("fact(", ")"), "func"),
     ("4", "ins", "4", "num"), ("5", "ins", "5", "num"), ("6", "ins", "6", "num")],
    [("ln", "wrap", ("log(", ")"), "func", ("eˣ", ("exp(", ")"))),
     ("log", "wrap", ("log10(", ")"), "func", ("10ˣ", ("10^(", ")"))),
     ("exp", "wrap", ("exp(", ")"), "func"),
     ("1", "ins", "1", "num"), ("2", "ins", "2", "num"), ("3", "ins", "3", "num")],
    [("sin", "wrap", ("sin(", ")"), "func", ("sin⁻¹", ("asin(", ")"))),
     ("cos", "wrap", ("cos(", ")"), "func", ("cos⁻¹", ("acos(", ")"))),
     ("tan", "wrap", ("tan(", ")"), "func", ("tan⁻¹", ("atan(", ")"))),
     ("0", "ins", "0", "num"), (".", "ins", ".", "num"), ("=", "equals", None, "equals")],
    [("(", "ins", "(", "func"), (")", "ins", ")", "func"),
     ("±", "negate", None, "func"),
     ("×", "ins", "*", "op"), ("−", "ins", "-", "op"), ("+", "ins", "+", "op")],
]


class Calculator(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Calculator")
        self.configure(bg=BG)
        self.geometry("%dx%d" % STANDARD_SIZE)
        self.minsize(300, 440)

        self.env = {"pi": 3.141592653589793, "e": 2.718281828459045,
                    "tau": 6.283185307179586, "ans": 0.0}
        self.entry_text = ""
        self.scientific = False
        self.second = False

        self.display_font = tkfont.Font(family="DejaVu Sans", size=30)
        self.small_font = tkfont.Font(family="DejaVu Sans", size=10)
        self.key_font = tkfont.Font(family="DejaVu Sans", size=13)

        self.columnconfigure(0, weight=1)
        self.rowconfigure(3, weight=1)

        header = tk.Frame(self, bg=BG)
        header.grid(row=0, column=0, sticky="ew", padx=10, pady=(8, 0))
        header.columnconfigure(1, weight=1)

        self.mode_button = tk.Button(
            header, text="☰", command=self.toggle_mode,
            bg=BG, fg=TEXT, activebackground=SURFACE_ALT,
            relief="flat", bd=0, highlightthickness=0, takefocus=0,
            font=self.key_font, cursor="hand2", padx=6,
        )
        self.mode_button.grid(row=0, column=0, sticky="w")

        self.mode_label = tk.Label(
            header, text="Standard", bg=BG, fg=TEXT, anchor="w",
            font=tkfont.Font(family="DejaVu Sans", size=11, weight="bold"),
        )
        self.mode_label.grid(row=0, column=1, sticky="w", padx=(8, 0))

        self.history = tk.Label(self, text="", bg=BG, fg=MUTED,
                                font=self.small_font, anchor="e")
        self.history.grid(row=1, column=0, sticky="ew", padx=16, pady=(4, 0))

        self.display = tk.Label(self, text="0", bg=BG, fg=TEXT,
                                font=self.display_font, anchor="e")
        self.display.grid(row=2, column=0, sticky="ew", padx=16, pady=(0, 8))

        self.keypad = tk.Frame(self, bg=BG)
        self.keypad.grid(row=3, column=0, sticky="nsew", padx=10, pady=(0, 10))

        self.build_keypad()

        self.bind("<Key>", self.on_key)
        self.bind("<Return>", self.equals)
        self.bind("<KP_Enter>", self.equals)
        self.bind("<Escape>", self.clear_all)
        self.after(50, self.focus_set)

    # --- layout ---------------------------------------------------------

    def build_keypad(self):
        for child in self.keypad.winfo_children():
            child.destroy()

        keys = SCIENTIFIC_KEYS if self.scientific else STANDARD_KEYS
        columns = max(len(row) for row in keys)

        for row in range(len(keys)):
            self.keypad.rowconfigure(row, weight=1)
        for col in range(columns):
            self.keypad.columnconfigure(col, weight=1)

        for r, row in enumerate(keys):
            for c, spec in enumerate(row):
                button = self.make_key(spec)
                button.grid(row=r, column=c, sticky="nsew", padx=2, pady=2)

    def make_key(self, spec):
        label, action = spec[0], spec[1]
        payload = spec[2]
        kind = spec[3] if len(spec) > 3 else "func"
        alt = spec[4] if len(spec) > 4 else None

        if self.second and alt:
            label, payload = alt[0], alt[1]

        if kind == "equals":
            background, foreground, hover = ACCENT, "#ffffff", ACCENT_HOVER
        elif kind == "num":
            background, foreground, hover = SURFACE, TEXT, HOVER
        else:
            background, foreground, hover = SURFACE_ALT, TEXT, HOVER_ALT

        if action == "second" and self.second:
            background = SURFACE

        button = tk.Button(
            self.keypad, text=label,
            command=lambda: self.handle(action, payload),
            bg=background, fg=foreground,
            activebackground=hover, activeforeground=foreground,
            relief="flat", bd=0, highlightthickness=1,
            highlightbackground=BORDER, highlightcolor=BORDER,
            font=self.key_font, cursor="hand2", takefocus=0,
        )
        button.bind("<Enter>", lambda _event: button.config(bg=hover))
        button.bind("<Leave>", lambda _event: button.config(bg=background))
        return button

    # --- state ----------------------------------------------------------

    def render(self):
        text = self.entry_text or "0"
        size = 30 if len(text) <= 18 else (24 if len(text) <= 26 else 18)
        self.display.config(text=text, font=(self.display_font.actual("family"), size))
        self.focus_set()

    def balanced(self, text):
        """Append the closing parens the user did not bother to type."""
        depth = 0
        for char in text:
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth < 0:
                    return text
        return text + ")" * depth

    # --- actions --------------------------------------------------------

    def handle(self, action, payload):
        if action == "equals":
            self.equals()
            return
        if action == "second":
            self.second = not self.second
            self.build_keypad()
            return

        if action == "ins":
            self.entry_text += payload
        elif action == "wrap":
            prefix, suffix = payload
            self.entry_text = "%s%s%s" % (prefix, self.entry_text, suffix)
        elif action == "clear":
            self.entry_text = ""
        elif action == "clear_all":
            self.entry_text = ""
            self.history.config(text="", fg=MUTED)
        elif action == "back":
            self.entry_text = self.entry_text[:-1]
        elif action == "negate":
            text = self.entry_text
            if text:
                if text.startswith("-(") and text.endswith(")"):
                    self.entry_text = text[2:-1]
                else:
                    self.entry_text = "-(%s)" % text

        self.render()

    def clear(self):
        self.entry_text = ""
        self.render()

    def clear_all(self, _event=None):
        self.entry_text = ""
        self.history.config(text="", fg=MUTED)
        self.render()

    def equals(self, _event=None):
        source = self.balanced(self.entry_text.strip())
        if not source:
            return "break"
        try:
            value = evaluate(source, self.env)
        except CalcError as exc:
            self.history.config(text="error: %s" % exc, fg=DANGER)
            return "break"

        self.env["ans"] = value
        self.history.config(text="%s =" % source, fg=MUTED)
        self.entry_text = format_number(value)
        self.render()
        return "break"

    def toggle_mode(self):
        self.scientific = not self.scientific
        self.second = False
        self.mode_label.config(text="Scientific" if self.scientific else "Standard")
        self.build_keypad()
        self.geometry("%dx%d" % (SCIENTIFIC_SIZE if self.scientific else STANDARD_SIZE))
        self.render()

    def on_key(self, event):
        char = event.char
        if char in "0123456789.+-*/%^()":
            self.entry_text += char
            self.render()
            return "break"
        if event.keysym == "BackSpace":
            self.entry_text = self.entry_text[:-1]
            self.render()
            return "break"
        return None


if __name__ == "__main__":
    Calculator().mainloop()
