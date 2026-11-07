  # FRIDGE Programming Language Specification (v1.0.0)

**FRIDGE** (*Friendly Runtime for Interactive Device Games & Education*) is a structured, event-driven programming language with `.frg` source files designed for KiterOS Nano.

## 1. Lexical & Block Structure
- Line-oriented statements; comments begin with `#`.
- Blocks open with a colon `:` at the end of a header line and close with `end`.
- Data types: `number` (IEEE-754 float/int), `text` (double-quoted string), `boolean` (`true`, `false`), `list` (`[a, b, c]`), and `entity` (2D sprite object).

## 2. Formal EBNF Grammar Summary
```ebnf
Program        ::= Header? Statement*
Header         ::= ("app" | "game") STRING
Statement      ::= RememberStmt | SetStmt | ShowStmt | ClearStmt
                 | IfStmt | RepeatStmt | WhileStmt | ForEachStmt
                 | FuncDecl | CallStmt | ReturnStmt | TryStmt
                 | EventDecl | EntityStmt | GraphicsStmt | AudioStmt | StorageStmt
RememberStmt   ::= "remember" IDENT "as" Expr
SetStmt        ::= "set" IDENT ("to" | "as") Expr
                 | "set" IDENT ("position" | "size" | "velocity") "to" Expr "," Expr
                 | "set" IDENT "color" "to" Expr
ShowStmt       ::= "show" Expr
IfStmt         ::= "if" Expr ":" Statement* ("otherwise" ":" Statement*)? "end"
RepeatStmt     ::= "repeat" Expr "times" ":" Statement* "end"
WhileStmt      ::= "while" Expr ":" Statement* "end"
ForEachStmt    ::= "for" "each" IDENT "in" Expr ":" Statement* "end"
FuncDecl       ::= "create" "function" IDENT ("with" IdentList)? ":" Statement* "end"
CallStmt       ::= "call" IDENT ("with" ExprList)?
EventDecl      ::= "when" ("app" "starts" | "game" "starts" | "frame" "updates" | ("player" | "user") "touches" "screen" | "key" Expr "pressed") ":" Statement* "end"
EntityStmt     ::= "create" IDENT | "move" IDENT ("up" | "down" | "left" | "right") ("by" Expr)?
GraphicsStmt   ::= "clear" Expr | "draw" "rect" Expr "," Expr "," Expr "," Expr "," Expr | "draw" "text" Expr "at" Expr "," Expr
AudioStmt      ::= "beep" Expr "," Expr
StorageStmt    ::= "save" Expr "as" Expr | "load" Expr "into" IDENT
