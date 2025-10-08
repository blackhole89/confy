# confy

Configure anything :)

Automate parametric modifications of any text-based file format, using either a scripting-friendly command line interface or an ncurses-like TUI. Think ``ccmake`` or the Linux kernel's `menuconfig`, but for *anything*.

As long as the format supports some form of block or line comments, the code that tells confy what to do can be written in place inside the files it controls. Link together all your config files and conditionally set any parameter combination to create and quickly switch between complex presets.

## About

**Warning:** confy is currently a work in progress. No part of it is guaranteed to be stable, forward- or backward-compatible. Use at your own risk.

When dealing with "programmatic" text files such as config files, scripts or source code, one would often like to be able to switch between complex alternatives quickly, without having to recall the rules of and write correct incantations in the appropriate format every time.
There are many ad-hoc approaches to this: 
source code is often instrumented by configuration generators as part of build systems which can disable or enable features, shell scripts can take additional parameters from the environment, and a common last resort is to leave a menu of alternatives to blocks of configuration or code as comments next to them.

The purpose of *confy* is to provide a convenient automated solution for all situations of this shape. To this end, confy implements a generic language that can be embedded into the comments of any file format that has them, and which can both store configuration state and describe how this configuration state affects the "live" (non-commented) parts of the file.
In the most basic case, the instructions given to confy may just say to comment or uncomment a block of code depending on the setting of a Boolean configuration variable; more advanced applications may involve templating stretches of code with the contents of configuration variables, performing complex tests and even using the results of stateful computations.

### Simple example

<p align="center"><a href="https://asciinema.org/a/mnMnqEqKpg3CEmekumq78zP9a" target="_blank"><img src="https://asciinema.org/a/mnMnqEqKpg3CEmekumq78zP9a.svg?a" /></a>
</p>

```latex
\section{LaTeX example}

The confy setup block tells confy how meta-instructions will be embedded in 
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

For confy to operate on a file, it must contain a `confy-setup` block, and the first instance of the string `confy-setup` in the file must be followed by such a block.
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
`if ( <expression> ) { <closed-form> } else [<if-statement> | { <closed-form> }`]  
Evaluates the expression and activates the `<closed-form>` if and only if it evaluates to `true` (resp. a nonzero number, or a nonempty string). This strips comments from all inert blocks and lines in it.  
Inactivates the `<closed-form>` if the expression evaluates to `false`, turning all non-commented parts of it into line or block inert segments.  
If an `else` branch if present, the opposite operation is performed on it.

* `template { <pattern> } into { }`  
Pattern should consist of inert blocks. Any instance of `$varname` in it will be replaced with a string representing the contents of that variable. Avoid substitution by escaping with `\\`. Any `~` immediately after a variable name will be deleted to allow for gapless substitution, so `\$3+$length~cm` may evaluate to `$3+15cm`.  
Due to current technical limitations, the syntax of this construct should not be spread out across meta-blocks of different type (e.g. with the `} into {` in a block comment and the final `}` in a line comment).

## Examples

The `examples/` subfolder contains several instructive examples that demonstrate core features.

## Planned features

* [ ] Shell invocations: on configuration change, to compute values...
* [ ] Better error reporting
* [ ] Variable namespaces
* [ ] Better treatment of conditional `include`. Hide variables from recursive includes.
* [ ] Support instrumentation of files with no usable comment format through shadow templates, like `.<filename>.confy-in`

## Building
Confy only requires a C++ compiler supporting c++17, and an ncurses-capable terminal if you want to use interactive mode. To build:
```bash
git clone https://github.com/blackhole89/confy.git
git submodule update --init --recursive
make
# ./confy
```

