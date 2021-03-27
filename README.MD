# iniget

## Table Of Contents

1. [Description](https://github.com/Randoragon/iniget#description)
2. [Quick Example](https://github.com/Randoragon/iniget#quick-example)
3. [Installation](https://github.com/Randoragon/iniget#installation)
4. [Syntax Rules](https://github.com/Randoragon/iniget#syntax-rules)
    - [Queries](https://github.com/Randoragon/iniget#queries)
    - [INI Files](https://github.com/Randoragon/iniget#ini-files)
5. [Pitfalls](https://github.com/Randoragon/iniget#pitfalls)

## Description

iniget is a simple INI files parser. It receives a file and queries in command-line parameters and
then scans through the file and computes each query, printing it on a separate line on standard output.

- pure ANSI C with no external dependencies
- fully standard compliant (`-Wall`, `-Wextra`, `-pedantic`)

For developers, the program is thoroughly documented in Doxygen. You can generate the documentation
yourself by running `doxygen` in the main project directory, or read the
[online documentation](https://codedocs.xyz/Randoragon/iniget/).

## Quick Example

**test.ini:**

```ini
; Test input INI file
exp = 3

[nums]
a = 2
b = 8

[strings]
hello = Hello
space = " "
there = there.
```

**command output:**

```sh
$ iniget test.ini '({nums.a}*{nums.b})^{exp}' '{strings.hello} + {strings.space} + {strings.there}'
4096
Hello there.
```

In the above example, two separate queries are run on the same file (file is read only once!),
and their results get printed on separate lines in the same order.

## Installation

Download the repository and run the following (if necessary, as root):

    make install

The program will be installed to `/usr/local/bin/iniget`.

## Syntax Rules

### Queries

- each query is a mathematical expression consisting of operands and operators in infix notation
- operands must be of form `{section.key}` (section is optional)
- operators are one of:
    - `+` addition
    - `-` subtraction
    - `*` multiplication
    - `/` division
    - `%` modulus
    - `^` exponentiation
- round parentheses may be used to enforce order of operation
- operator precedence and associativity should work intuitively (like in math)
- the `*` operator is implicit and may be omitted (like in math)
- all operators work on numbers and some work on strings
- for strings, `+` is concatenation, and `*` is repetition (must be multiplied by a non-negative number, only the integer part matters)

### INI Files

iniget is not an INI files validation tool, it (for the most part) assumes that the file it reads
is formatted correctly. It does so to minimize the amount of processing required (for example,
once it reaches the closing bracket of a section, it doesn't care about the remainder of the line).

Here's the expected INI format:

- section names and key/value pairs must be on separate lines
- section and key names may consist of alphanumeric characters, hyphens and underscores
- key and value must be delimited by `=` (may be surrounded by whitespace)
- leading/trailing whitespace characters are ignored
- semicolon starts a comment until the end of the line
- values consisting of digits (with optional decimal part) are interpreted as numbers
- any value that fails to be recognized as a number is considered a string
- you can force a value to be a string by enclosing it in "double quotes"
- "  double quotes  " may also be used to have a string with leading/trailing whitespace

## Pitfalls

iniget uses `double` for all number calculations, so it's only viable for relatively simple calculations.
With huge or tiny numbers, you will get output like `inf`.

Internally, each query has its own copies of all data that it needs to compute the result. While individual
queries were optimized not to store multiple copies of the same value, separate queries using the same value
store independent copies. Because of that, if you pass many queries, each needing a very long string value from
the file, the memory footprint of the program could get very large.
