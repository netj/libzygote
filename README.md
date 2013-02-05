# libzygote -- Zygote Process Library

There are programs you want to run multiple times with slight variations, but
are wasting a lot of time going through the same start up procedure, such as
loading a set of data into memory.  Using libzygote, you can easily eliminate
this inefficiency with minimal changes to the code.  You will first run your
start up code once and libzygote will keep it alive as a "zygote" process.
Whenever you want to feed the rest of your code, you can use libzygote's "grow"
command, and grow your zygote into an actual process.

First, you need to identify the following two parts in your program, and
partition the code into two parts:

1. The start up code that takes some time but is common across all runs, and
2. The variable code that you want to run multiple times without going through
   the start up overhead.

The only change you need to make to your code structure is to:

1. split your start up code by ending it with a call to `run_multiple(char*
   socket_path, ...)`, and
2. extract the rest into `run(int objc, void* objv[], int argc, char* argv[])`.

Then, your start up part will need an `#include <zygote.h>` line and link
against `-lzygote` to produce a zygote executable.  You can run this executable
as usual to create a zygote process.  A Unix domain socket is created at the
`socket_path` you passed, and your zygote process will listen on it, waiting
for the rest of the code to arrive.  Any pointer you provide to
`run_multiple()` will be passed to `run()`'s `objv` later in the same order.

The other part now enclosed in `run()` should be compiled into a shared
library, instead of an executable.  For most compilers, this is possible by
adding a `-shared` flag for the linker.  You might need to add `-fPIC` while you
compile individual objects (\*.o) as well.  Once you have the shared object of
your code, you can pass it to the zygote process using libzygote's `grow` command,
after telling where the socket file is.  Any additional command-line arguments
to `grow` will be passed to `run()`'s `argv`.
