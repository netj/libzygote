# [libzygote][] -- Zygote Process Library for C

There are programs you want to run multiple times with slight variations, but
are wasting a lot of time going through the same start up procedure, such as
loading a set of data into memory.  Using libzygote, you can easily eliminate
this inefficiency with minimal changes to the code.  You will first run your
start up code once and libzygote will keep it alive as a "[zygote][]" process.
Whenever you want to feed the rest of your code, you can use libzygote's `grow`
command, and grow your zygote into an actual process.


## Instructions

First, you need to identify the following two parts in your program, and
partition the code into two parts:

1. The start up code that takes some time but is common across all runs, and
2. The variable code that you want to run multiple times without going through
   the start up overhead.

The only change you need to make to your code structure is to:

1. split your start up code by ending it with a call to `zygote(char*
   socket_path, ...)`, and
2. extract the rest into `run(int objc, void* objv[], int argc, char* argv[])`.

Then, your start up part will need an `#include <zygote.h>` line and link
against `-lzygote` to produce a zygote executable.  You can run this executable
as usual to create a zygote process.  A Unix domain socket is created at the
`socket_path` you passed, and your zygote process will listen on it, waiting
for the rest of the code to arrive.  Any pointer you provide to
`zygote()` will be passed to `run()`'s `objv` later in the same order.

The other part now enclosed in `run()` should be compiled into a shared
library, instead of an executable.  For most compilers, this is possible by
adding a `-shared` flag for the linker.  You might need to add `-fPIC` while you
compile individual objects (\*.o) as well.  Once you have the shared object of
your code, you can pass it to the zygote process using libzygote's `grow` command,
after telling where the socket file is.  Any additional command-line arguments
to `grow` will be passed to `run()`'s `argv`.



## An Example

### The Naive Way
Suppose you have a C code `example.c` as follows, and you want to run
`some_actual_computation()` many times with different parameters, and even with
slight changes to the code.
```c
/* example.c -- libzygote example */
int main(int argc, char* argv[]) {
    my_data_structure *a, *b;
    another_structure *c;
    int code;
    
    long_running_loading(
            &a, &b, &c, /* loads a, b, c, */
            argv[1]     /* based on command-line argument,
                           e.g., filename */
        );
    
    code = some_actual_computation(
            a, b, c,                      /* uses a, b, c, and      */
            atoi(argv[2]), atof(argv[3]), /* command-line arguments */
        );
    
    return code;
}
```

The naive way of doing this will be the following:
```sh
# First compile it,
cc -o example  example.c
# Then, run it many times
./example input_file 23 4.56
./example input_file 78 9.01
./example input_file 90 1.23

# Change the code, and repeat
cc -o example  example.c
./example input_file 23 4.56
./example input_file 54 3.21
# ...
```
However, if `long_running_loading()` takes an awful lot of time, this will be a
very bad way to get things done.

### Smarter Way with libzygote
You can use libzygote to avoid running the same `long_running_loading()` every
time you run your actual code.  Here's a step-by-step guide how to do that.

```c
/* example.c -- libzygote example */
/*
   Step 0: Include zygote.h.
 */
#include <zygote.h>

int main(int argc, char* argv[]) {
    my_data_structure *a, *b;
    another_structure *c;
    int code;
    
    long_running_loading(
            &a, &b, &c, /* loads a, b, c, */
            argv[1]     /* based on command-line argument,
                           e.g., filename */
        );
    
/*
   Step 1: Instead of directly calling some_actual_computation(),
           call zygote() passing everything you'll need later.
           Don't forget to NULL-terminate the argument list.
 */
    code = zygote("/path/to/zygote.socket", a, b, c, NULL);
    
    return code;
}

/*
   Step 2: Enclose the code you will change and run frequently with run().
 */
int run(int objc, void* objv[],  int argc, char* argv[]) {
/*
   Step 2.1: The dirtiest job will be casting types back to original ones.
 */
    my_data_structure *a = (my_data_structure *) objv[0],
                      *b = (my_data_structure *) objv[1];
    another_structure *c = (another_structure *) objv[2];
    
/*
   Step 2.2: Rest of the code remains the same.
             (apart from the shifted argv indexes)
 */
    code = some_actual_computation(
            a, b, c,                      /* uses a, b, c, and      */
            atoi(argv[1]), atof(argv[2]), /* command-line arguments */
        );
    
    return code;
}
```

Next, after making these changes to your code, you need to build two things out
of it: a zygote executable, and a runnable shared object that contains the
actual `run()`.

```sh
# Step 3.1: Build a zygote executable.
cc -o example-zygote               example.c -lzygote

# Step 3.2: Build a runnable shared object.
cc -o example-run.so -shared -fPIC example.c -lzygote
```

Now, you can launch the zygote executable once and keep it alive, and run the
shared object as many times as you want.

```sh
# Step 4.1: Launch your zygote process in background.
./example-zygote input_file &

# Step 4.2: Then run your run() function without going through the
#           long_running_loading() every time.
grow /path/to/zygote.socket ./example-run.so 23 4.56
grow /path/to/zygote.socket ./example-run.so 78 9.01
grow /path/to/zygote.socket ./example-run.so 90 1.23
grow /path/to/zygote.socket ./example-run.so 45 6.78

# You can now run even your modified code instantly,
cc -o example.run.so -shared -fPIC example.c -lzygote
grow /path/to/zygote.socket ./example-run.so 23 4.56
grow /path/to/zygote.socket ./example-run.so 54 3.21

# as long as you keep the zygote process alive.
cc -o example.run.so -shared -fPIC example.c -lzygote
grow /path/to/zygote.socket ./example-run.so 45 1.23

# ...
```


## Installation
You can install libzygote to your system using the following command:
```sh
make install PREFIX=/usr/local
```
This is all you need if you choose a standard location, such as `/usr/local`.


If you intend to install libzygote just for your project, choose an appropriate
`PREFIX` while installing, such as:
```sh
LIBZYGOTE_PREFIX=/your/project/dir/libzygote
make install PREFIX=$LIBZYGOTE_PREFIX
```

With a non-standard installation, you need to add the following compiler and
linker flags while you build your code:
```sh
export CFLAGS=-I$LIBZYGOTE_PREFIX/include
export LDFLAGS=-L$LIBZYGOTE_PREFIX/lib

# use them while you compile
cc -o example-zygote         $CFLAGS       example.c $LDFLAGS -lzygote
cc -o example.run.so -shared $CFLAGS -fPIC example.c $LDFLAGS -lzygote

# or make will recognize them
```

Finally, set the environment variable before you run your zygote executable or
libzygote's `grow` command.
```sh
export PATH=$LIBZYGOTE_PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$LIBZYGOTE_PREFIX/lib

./example-zygote ... &
grow ...
```

----

## Credits
Libzygote is written by [Jaeho Shin][netj], and [available as open
source][libzygote] using [Apache 2.0
license](http://www.apache.org/licenses/LICENSE-2.0.html).

Use of "zygote" in its name was inspired by Google's [Android operating
system][android zygote], and [a similar concept in the Chrome web
browser][chrome zygote].  [Nailgun][] has also been very influential, which is
a similar project aiming to avoid the start up overhead of Java VMs.


[zygote]: http://en.wikipedia.org/wiki/Zygote

[libzygote]: https://github.com/netj/libzygote/#readme
[netj]: https://github.com/netj

[android zygote]: https://android.googlesource.com/platform/libcore/+/master/dalvik/src/main/java/dalvik/system/Zygote.java
[chrome zygote]: http://code.google.com/p/chromium/wiki/LinuxZygote
[nailgun]: https://github.com/martylamb/nailgun
