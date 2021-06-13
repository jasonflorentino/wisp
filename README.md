# 👻 Wisp (wip)
### ***A Lisp-y language made reading [buildyourownlisp.com](http://www.buildyourownlisp.com/) by [Daniel Holden](https://github.com/orangeduck)***

I was looking for some resources to learn more about C and came across this wonderful project. In fifteen short but meaningful chapters, Daniel walks you through building a language from scratch. I highly recommend it for anyone interested in C or the inner workings of programming languages. While I can't say I could roll some of the features he teaches on my own yet, I certainly feel more comfortable in my knowledge of C programs and learned some excellent foundations on the design of programming languages. Thanks Daniel!

—Jason  
June 2021

## Compile on Linux and Mac
```
cc -std=c99 -Wall wisp.c mpc.c -ledit -lm -o wisp
```
## Compile on Windows
```
cc -std=c99 -Wall wisp.c mpc.c -o wisp
```

## Language Features
Wisp is a [Lisp](https://en.wikipedia.org/wiki/Lisp_(programming_language))-like language that supports some basic features:
- [Polish notation](https://en.wikipedia.org/wiki/Polish_notation)
- Symbolic Expressions and Quoted Expressions
- Number, Symbol, and Function types
- Variables
- Custom functions
- Arithmetic and Comparison operators
- Conditional statements


## Dependencies
This repo includes a copy `mpc.c` and `mpc.h` from [github.com/orangeduck/mpc](https://github.com/orangeduck/mpc)
