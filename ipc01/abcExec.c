#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int main() {
    srand(time(NULL));

    while (1) {
        // Simulate generating an output value
        int outputValue = rand() % 100;

        // Write the output value to the standard output
        //printf("Output Value: %d\n", outputValue);
        printf("%d\n", outputValue);
	fflush(stdout);

        // Introduce a delay
        sleep(1);
    }

    return 0;
}

