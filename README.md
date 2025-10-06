# confy

Configure anything :)

Automate parametric modifications of any text-based file format, using either a scripting-friendly command line interface or an ncurses-like TUI. Think ``ccmake`` or the Linux kernel's `menuconfig`, but for *anything*.

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

### Syntax overview

For confy to operate on a file, it must contain a `confy-setup` block.
This takes the form
```
confy-setup { key: "value", ... }
```
where valid keys are `line`, `block_start`, `block_end`, `meta_line`, `meta_block_start` and `meta_block_end`. These define delimiters for line and block comments for *inert segments* and *meta segments* respectively. *Inert segments* are runs in the syntax of the target file that have been inactivated for some reason, such as being in a conditional branch under a test that failed, or being a template to be instantiated with the contents of confy variables. *Meta segments* contain confy instructions and control flow.

Valid constructs that can occur in meta segments are:

* `[hidden] <type> $<varname> ["Friendly name"] = <value> ;` 
This defines a new confy variable, optionally with a friendly name for the TUI.
The initial `<value>` is mandatory and must be a basic value, as this is where confy will save the new value if it is modified.
 If the variable is marked `hidden`, it will not be displayed in the TUI.

* `$<varname> = <expression> ;`
This will evaluate `<expression>` and assign the result to the variable, if it has already been defined.

* `include("relative-path");`
Includes a file identified by its path relative to the current file. Variables defined in the included file become readable and writable in the current file and namespace.

* `if ( <expression> ) { <closed-form> } `
`if ( <expression> ) { <closed-form> } else [<if-statement> | { <closed-form> }`
Evaluates the expression and activates the `<closed-form>` if and only if it evaluates to `true` (resp. a nonzero number, or a nonempty string). This strips comments from all inert blocks and lines in it. Inactivates it if the expression evaluates to `false` (turning all non-commented parts of it into line or block inert segments). If an `else` branch if present, the opposite operation is performed on it.

* `template { <pattern> } into { }`



## Planned features

* [ ] Shell invocations: on configuration change, to compute values...
* [ ] Better error reporting
* [ ] Variable namespaces

## Building
Confy only requires a C++ compiler supporting c++17, and an ncurses-capable terminal if you want to use interactive mode. To build:
```bash
git clone https://github.com/blackhole89/confy.git
git submodule update --init --recursive
make
# ./confy
```

