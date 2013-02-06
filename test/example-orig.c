/* example.c -- libzygote example */
#include "example-details.c"

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
            atoi(argv[2]), atof(argv[3])  /* command-line arguments */
        );

    return code;
}
