#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>


int main(void) {
    int coid = name_open("central_controller", 0);

    if (coid == -1) {
        perror("Could not connect to Central");
        return EXIT_FAILURE;
    }

    const int pulse_code = _PULSE_CODE_MINAVAIL + 50;
    const int old_state = 3;
    const int new_state = 2;
    const int value = old_state + 10 * new_state;

    int result = MsgSendPulse_r(coid, -1, pulse_code, value);

    if (result != EOK) {
        fprintf(stderr, "Could not send pulse. Error code: %d\n",
                result);
        name_close(coid);
        return EXIT_FAILURE;
    }

    puts("Test pulse sent.");

    if (name_close(coid) == -1) {
        perror("Could not close connection");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
