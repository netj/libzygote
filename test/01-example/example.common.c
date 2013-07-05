#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *values;
    int count;
} my_data_structure;

typedef struct {
    float *errors;
    int sum;
    int count;
} another_structure;

void long_running_loading(
        my_data_structure **pa, my_data_structure **pb,
        another_structure **pc,
        char* filename
        ) {
    char buf[BUFSIZ];
    int i, count;

    *pa = (my_data_structure *) malloc(sizeof(my_data_structure));
    *pb = (my_data_structure *) malloc(sizeof(my_data_structure));
    *pc = (another_structure *) malloc(sizeof(another_structure));

    FILE* fp = fopen(filename, "r");

    // read the count
    fgets(buf, sizeof(buf), fp);
    sscanf(buf, "%d", &count);
    (*pc)->count = (*pb)->count = (*pa)->count = count;

    // allocate memory;
    (*pa)->values =   (int *) malloc(count * sizeof(int));
    (*pb)->values =   (int *) malloc(count * sizeof(int));
    (*pc)->errors = (float *) malloc(count * sizeof(float));

    // read lines and fill a, b
    int *va = (*pa)->values;
    int *vb = (*pb)->values;
    int sum = 0;
    for (i=0; i<count; i++) {
        fgets(buf, sizeof(buf), fp);
        va[i] = atoi(buf);
        vb[i] = (sum - vb[i < 4 ? 0 : i - 4]) / 4;
        sum += va[i];
    }
    (*pc)->sum = sum;
    // fill c
    float *vc = (*pc)->errors;
    float avg = (float)sum / (float)count;
    for (i=0; i<count; i++) {
        vc[i] = (float)va[i] - avg;
    }
}

int some_actual_computation(
        my_data_structure *a, my_data_structure *b,
        another_structure *c,
        int x, float y
        ) {
    printf("a->values[%d] = %d\n", x, a->values[x]);
    printf("b->values[%d] = %d\n", x, b->values[x]);
    printf("c->values[%d] = %f\n", x, c->errors[x]);
    return (int) (c->errors[x] / y);
}
