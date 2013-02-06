/* example.c -- libzygote example */
#include "example.common.c"

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
    code = zygote("zygote.socket", a, b, c, NULL);

    return code;
}

/*
   Step 2: Enclose the code you will change and run frequently with run().
 */
int run(int objc, void* objv[],  int argc, char* argv[]) {
    int code;
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
            atoi(argv[1]), atof(argv[2])  /* command-line arguments */
        );

    return code;
}
