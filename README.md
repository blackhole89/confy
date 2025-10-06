# confy

Configure anything :)

Automate parametric modifications of any text-based file format, using either a scripting-friendly command line interface or an ncurses-like TUI. Think the Linux kernel's `menuconfig` or `ccmake`, but for *anything*.

As long as the format supports some form of block or line comments, the code that tells confy what to do can be written in place inside the files it controls. Link together all your config files and conditionally set any parameter combination to create and quickly switch between complex presets.

## About

**Warning:** confy is currently a work in progress. No part of it is guaranteed to be stable, forward- or backward-compatible. Use at your own risk.

### Simple example

```latex
\section{LaTeX example}

The confy-setup block tells confy how meta-instructions will be embedded in 
this file. Confy supports line and block comments, and you should specify at
least one of the two for each of inert text and metacode. 

LaTeX only supports line comments. In general, the sensible approach is to
take the current file's language's comment syntax, and suffix it with some
unique character sequence to avoid accidental clashes with any existing
comments that should not have any meaning to confy.

% confy-setup { line: "%-", meta_line: "%!" }

We can now define some variables. The quoted text is optional, and allows
 you to display more memorable descriptions in interactive mode.

%! bool $b1 "First boolean" = true;
%! bool $b2 "Second boolean" = true;

Are both booleans true?
%! if($b1 && $b2) {
Yes!
%! } else {
%-No!
%! }

If you now were to run confy and set one of the two booleans to false, 
confy would change this file to comment out the line saying `Yes!', and
uncomment the line saying `No!'.

For more advanced usage, you can also use \textit{templating}:

%! int $i1 "First integer" = 1337;
%! template {
%-The integer is $i1.
%! } into {
The integer is 1337.
%! }

This text will be updated to reflect the chosen value of $i1.
```

## Planned features

* [ ] Shell invocations: on configuration change, to compute values...
* [ ] Better error reporting

## Building
Confy only requires a C++ compiler supporting c++17, and an ncurses-capable terminal if you want to use interactive mode. To build:
```bash
git clone https://github.com/blackhole89/confy.git
git submodule update --init --recursive
make
# ./confy
```

