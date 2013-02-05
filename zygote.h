/*
 * Copyright 2013 Jaeho Shin <netj@cs.stanford.edu>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
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
 * See: https://github.com/netj/libzygote/#readme
 */

#ifndef _ZYGOTE_H
#define _ZYGOTE_H

#define ZYGOTE_VERSION 0x00000001

#ifdef __cplusplus 
extern "C" {
#endif

/**
 * zygote() will turn this into a zygote process that is accesible through a
 * Unix domain socket created at the given socket_path, and wait for runnable
 * shared objects containing run() to arrive.  Prior to calling zygote(),
 * finish all the time-consuming loading or start up code, and pass any
 * pointers the subsequent code will have to use.  Pointers given after
 * socket_path, will be passed to run() later as objc and objv.  The argument
 * list must be NULL-terminated.
 */
int zygote(char* socket_path, ... /*, NULL */);

/**
 * run() is the function you'll need to fit the rest of your code into.  The
 * first two arguments, objc and objv are the pointers passed to zygote()
 * from the zygote process, argc and argv correspond to the command line
 * arguments given to the grow command.  argv[0] is the path to the shared
 * object.
 */
int run(int objc, void* objv[], int argc, char* argv[]);


/**
 * Define ZYGOTE_DISABLED if you want to skip the zygote process mechanism, and
 * simply invoke the run() that exists in the same executable or address space.
 *
 * NOTE: You will need to pass -rdynamic flag to the linker for libzygote to be
 * able to locate your run() function using dlopen().
 */
#ifdef ZYGOTE_DISABLED
#define zygote(args...) zygote_skip(args)
int zygote_skip(char* socket_path, ... /*, NULL */);
#endif /* ZYGOTE_DISABLED */


#ifdef __cplusplus 
}
#endif

#endif /* _ZYGOTE_H */
