/*
 * libzygote -- Zygote Process Library
 *
 * There are programs you want to run multiple times with slight variations,
 * but are wasting a lot of time going through the same start up procedure,
 * such as loading a set of data into memory.  Using libzygote, you can easily
 * eliminate this inefficiency with minimal changes to the code.  You will
 * first run your start up code once and libzygote will keep it alive as a
 * "zygote" process.  Whenever you want to feed the rest of your code, you can
 * use libzygote's "grow" command, and grow your zygote into an actual process.
 *
 * First, you need to identify the following two parts in your program, and
 * partition the code into two parts:
 * a) the start up code that takes some time but is common across all runs, and
 * b) the variable code that you want to run multiple times without going
 *    through the start up overhead.
 * The only change you need to make to your code structure is to:
 * 1. split your start up code (a) by ending it with a call to
 *    `run_multiple(char* socket_path, ...)`, and
 * 2. extract the rest (b) into `run(int argc, char* argv[], int objc, void*
 *    objv[])`.  
 *
 * Then, your start up part (a) will need an `#include <zygote.h>` line and
 * link against `-lzygote -ldl` to produce a zygote executable.  You can run
 * this executable as usual to create a zygote process.  A Unix domain socket
 * is created on the `socket_path` you passed, and it will listen on it waiting
 * for the rest of the code to arrive.
 *
 * The other part (b) now enclosed in `run()` should be compiled into a
 * shared library, instead of an executable.  For most compilers, this is
 * possible by adding a `-shared` linker argument.  You will pass this shared
 * object to libzygote's grow command along with the path to the socket your
 * zygote process is listening to.
 *
 * Author: Jaeho Shin <netj@cs.stanford.edu>
 * Created: 2013-02-05
 */

#define ZYGOTE_VERSION 0x00000001

#ifdef __cplusplus 
extern "C" {
#endif

/*
 * run() is the function prototype you'll need to fit the rest of your code
 * into.  The first two arguments, argc and argv correspond to the command line
 * arguments of the grow command.  The next two arguments are the pointers
 * passed to run_multiple() from the zygote process.
 */
int run(int argc, char* argv[], int objc, void* objv[]);

/*
 * run_multiple() will turn this into a zygote process that is accesible
 * through a Unix domain socket created at the given socket_path, and wait for
 * shared libraries to be passed.
 */
int run_multiple(char* socket_path, ...);

/*
 * run_once() is an in-place replacement for run_multiple() you would probably
 * want to use while first writing and debugging the actually run(), in the
 * same executable.  Whenever you're ready to split run() into a separate
 * process, simply switch the call to run_once() in your zygote code to
 * run_multiple().
 */
int run_once(char* socket_path, ...);

#ifdef __cplusplus 
}
#endif
