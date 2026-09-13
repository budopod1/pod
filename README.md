# pod

Pod is a Lisp-inspired scripting language designed for use as a shell on Linux.

## Building

Pod relies on GNU Readline. Its header files must be avaliable.

Pod is written primarily in [Epsilon](https://github.com/budopod1/Epsilon), so it can be built with:

    epslc compile

To ensure `pod` is added to your `$PATH`, run:

    python install.py

Then, restart your shell.

## Command usage

    The Pod shell

    usage: pod [<filename>] [<args>...] [options]

    arguments:
    [<filename>]  The file to run
    [<args>...]   Arguments for the program

    options:
    --help, -h                Show this help
    -c <exec-str>             Use program passed in as string
    -p <print-exec-str>       Use program passed in as string, print the result

    Run pod with no arguments to start REPL

## Syntax and semantics

Pod uses a syntax primarily built upon Lisp S-expressions, while also incorporating significant portions of shell syntax and additional syntactic sugar.

Simple shell syntax works as excepted:

    pgrep gimp | xargs kill

But can also be written with S-expressions:

    (kill (pgrep gimp >.))

Or with syntactic sugar (producing exactly the same syntax tree):

    kill::pgrep gimp >.

Pod is neither "homoiconic" nor does it support "code-is-data". For shell usage or small scripts macros are less useful. Additionally, it allows for parentheses to be omitted at the top level of a function (using newlines to seperate statements), as in:

    print (+ 1 2)
    print (* 4 
        (- 5 3))

The operator `::` acts as syntactic sugar. It takes an S-expression after the `::` operator without parentheses and places it as the last argument of the S-expression before it. The above block could be transformed to:

    print :: + 1 2
    print :: * 4 :: - 5 3

Pod's bareword symbols do not evaluate variables:

    # Prints the text 'foo', verbatim
    print foo

Pod contains very little syntax. Most operations are performed with builtin functions. Variables can be declared with the `set` builtin function. Their value can be used by prefixing their name with a `$`.

    set a = 3
    set b =:: * $a 3
    # prints 6
    print $b

Pod's primary data structure is the array list, created with the `arr` builtin:

    set c =:: arr 1 2 3

Arrays are iterables. All iterables can be spread into many arguments with the `:` operator:

    print :$c
    # calls
    print 1 2 3

When the value called is a symbol, if no function with the name is found, Pod runs a command instead. By default, the output is not captured, but echoed to the console.

    $ pwd
    /usr/local/bin

Simple shell redirections will work as normal.

    ls >files.txt

To capture the command's output, use the special redirect `>.`.

    set dir =:: pwd >.

*Note: `>.` strips the final newline from the ouput, if present. To recieve the output verbatim, use `>..`.*

Pod uses the `|` operator to pipe the stdout of one S-expression into the stdin of another. `|` forks the process and connects the expression on the left's stdout to the expression on the right's stdin, killing the forked process once the pipe closes. `|` works with any expressions, not just commands:

    set length =:: print $txt | wc -c >.

The `_` builtin function returns its first argument, meaning it can be used to display the value of variables:

    $ set x = 8
    $ _ $x
    8

Functions are creates by surrounding one or more S-expressions with curly brackets. They are the primary building block of control flow, and are first class values, meaning they can be stored in variables.

    $ set fn = {print hi}
    $ # these parentheses are not technically required
    $ ($fn)
    hi
    $ ($fn)
    hi

Pod does not have special forms like Lisp, instead it has builtin functions which take functions. For instance, the `if` builtin function, in its simplest form, runs its second argument when passed a truthy value as its first argument.

    if (= $a 3) {
        print "a is three"
    }

Functions can be declared with the `defun` builtin function, allowing them to be accessed throughout the program without a `$`:

    defun sayhi {
        print "Hello, world!"
    }
    sayhi

A function's arguments are avaliable as an array in the `$@` variable, and the nth argument is avaliable in `$<n-1>` (eg `$0` is the first argument). To recieve the arguments in variables, use the `args` builtin function (or its alias `=>`), which wraps a function:

    defun do-add ::=> a b {
        print $a + $b = (+ $a $b)
    }

    # prints '1 + 2 = 3'
    do-add 1 2

# License

Pod is avaliable under the GPLv3. See the `LICENSE` file.
