# 👻 Wisp
### ***A Lisp-y language made reading [buildyourownlisp.com](http://www.buildyourownlisp.com/) by [Daniel Holden](https://github.com/orangeduck)***

I was looking for some resources to learn more about C and came across this wonderful project. In fourteen short but meaningful chapters, Daniel walks you through building a language from scratch. I highly recommend it for anyone interested in C or the inner workings of programming languages. While I'm not sure I could roll some of the features he teaches on my own yet, I certainly feel more comfortable in my knowledge of C programs and learned some excellent foundations on the design of programming languages. Thanks Daniel!

—Jason  
June 2021

---

- [Installation](#installation)
- [Language Features](#language-features)
- [The Basics](#the-basics)
- [Examples](#examples)
- [Language Reference](#language-reference)
- [Dependencies](#dependencies)

---

# Using Wisp

## Installation
To run Wisp, download or clone this repo, compile `wisp.c` and run the output file. This starts up a REPL in the terminal window.

### Compile on Linux and Mac
```
cc -std=c99 -Wall wisp.c mpc.c -ledit -lm -o wisp
```
### Compile on Windows
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
- Comments

## The Basics
Like a [Lisp](https://en.wikipedia.org/wiki/Lisp_(programming_language)) program, Wisp code is just a bunch of *expressions*. Expressions are contained within parentheses.
```
( some expression )
```
When expressions get *evaluated*, they produce a value -- which can in turn be part of another expression.
```
( some expression ( another expression to be part of the first ) )
```
Expressions get evaluated by applying a *function* to its elements. The function (or operator) is always the first element in the expression. This is called [Polish](https://en.wikipedia.org/wiki/Polish_notation), or prefix, notation.
```
( operator element element element )
```
The addition operator `+`, for example, adds all its elements together. So the expression `(+ 1 2 3)` evaluates to `6`. We can now combine what we learned earlier to say that the expression `(+ 1 2 3 (+ 1 2 3))` would evaluate to `12` — The nested expression evaluates to `6`, which then becomes part of evaluating the outer expression.  

As you might expect, minus, multiply, and divide are also operators:
```
(- 1 2 3) evalutes to -4
(* 2 4)   evalutes to 8
(/ 10 2)  evalutes to 5
```
The eagle-eyed among you might have noticed that the order matters here. The expressions are evaluated from left to right. `(- 1 2 3)` evaluates to `-4`, whereas `(- 2 3 1)` would evaluate to `-2`. `(/ 10 2)`  becomes `5`, but `(/ 2 10)` would be `0`. (Floating point numbers are not yet supported.)

## Examples

#### Write a comment
```
> 11 ;Eleven
< 11
```

#### Define a variable
```
> def {time} 11
< ()
> time
< 11
```

#### Define a function
```
> def {greet} (\ {hour} {
        if (< hour 12)
        {print "Good morning!"}
        {print "Good afternoon!"}
  }
```

#### Call the function
```
> greet 13
< "Good afternoon!"
> greet time
< "Good morning!"
```

#### Make a list
```
> def {times} {10 11 12 13 14}
< ()
> times
< {10 11 12 13 14}
```

#### Work with a list
```
> head times
< {10}
> tail times
< {11 12 13 14}
> join {+} (tail times)
< {+ 11 12 13 14}
> eval (join {+} (tail times))
< 50
> greet (eval (head times))
< "Good morning!"
```

## Language Reference

`42` Number  
`"Hello World!"` String  
`( )` Symbolic expression (Gets evaluated)  
`{ }` Quoted expression (Doesn't get evaluated)  

`def` Assigns a value or expression to a symbol  
`\` Defines a function. Eg: `\ {params} {body}`  
`;` Starts a comment until end of the line  
`print` Prints values to screen

### Math operators
` + ` Adds elements  
` - ` Subtracts elements  
` * ` Multiplies elements  
` / ` Divides elements  

### Conditional operators
`0` is considered falsy, all other numbers are considered truthy.  
` > ` Evaluates to `1` if the first element is greater than the second. `0` otherwise.  
` < ` Evaluates to `1` if the first element is less than the second. `0` otherwise.  
`>=` Evaluates to `1` if the first element is greater than or equal to the second. `0` otherwise.  
`<=` Evaluates to `1` if the first element is less than or equal to the second. `0` otherwise.  
`==` Evaluates to `1` if two elements are equal. `0` otherwise.  
`!=` Evaluates to `1` if two elements aren't equal. `0` otherwise.  
`if` Evaluates the first expression if the condition is true and the second one if it's false. Eg: `if (> 100 3) {big} {small}`  

### List operators   
`head` Returns the first element in a Quoted Expression  
`tail` Returns the rest of a Q-Expression (without the first element)  
`join` Makes one new Q-Expression from multiple Q-Expressions  
`eval` Attempts to evaluate a Q-Expression 

# Dependencies
This repo contains a copy `mpc.c` and `mpc.h` from [github.com/orangeduck/mpc](https://github.com/orangeduck/mpc)

<!-- 
781         wenv_add_builtin(e, "list", builtin_list);
 782         wenv_add_builtin(e, "head", builtin_head);
 783         wenv_add_builtin(e, "tail", builtin_tail);
 784         wenv_add_builtin(e, "eval", builtin_eval);
 785         wenv_add_builtin(e, "join", builtin_join);
 786 
 787         /* Math functions */
 788         wenv_add_builtin(e, "+", builtin_add);
 789         wenv_add_builtin(e, "-", builtin_sub);
 790         wenv_add_builtin(e, "*", builtin_mul);
 791         wenv_add_builtin(e, "/", builtin_div);
 792 
 793         /* Comparison functions */
 794         wenv_add_builtin(e, "if", builtin_if);
 795         wenv_add_builtin(e, "==", builtin_eq);
 796         wenv_add_builtin(e, "!=", builtin_ne);
 797         wenv_add_builtin(e, ">",  builtin_gt);
 798         wenv_add_builtin(e, "<",  builtin_lt);
 799         wenv_add_builtin(e, ">=", builtin_ge);
 800         wenv_add_builtin(e, "<=", builtin_le);
 801 
 802         /* String functions */
 803         wenv_add_builtin(e, "load",  builtin_load);
 804         wenv_add_builtin(e, "error", builtin_error);
 805         wenv_add_builtin(e, "print", builtin_print);
 -->
