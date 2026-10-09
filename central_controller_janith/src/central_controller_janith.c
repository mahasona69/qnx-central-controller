#include <stdio.h>
#include <stdlib.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <time.h>

typedef enum {
    TRAIN_UNKNOWN = -1,
    TRAIN_NORMAL = 0,
    TRAIN_SYSTEM_FAILURE = 1,
    TRAIN_APPROACHING_CROSSING = 2,
    TRAIN_CROSSING = 3
} train_state_t;

typedef enum{
	X1_UNKNOWN = -1,
	X1_IDLE = 0,
	X1_TRAIN_APPROACHING = 1,
	X1_WARNING_ACTIVE = 2,
	X1_GATES_LOWERING = 3,
	X1_GATES_DOWN = 4,
	X1_TRAIN_CROSSING = 5,
	X1_TRAIN_CLEAR_WAIT = 6,
	X1_GATES_RAISING = 7,
	X1_CLEAR = 8,
	X1_FAULT = 9
}crossing_state_t;



const char *train_state_name(train_state_t state) {
    switch (state) {
        case TRAIN_UNKNOWN:              return "Unknown";
        case TRAIN_NORMAL:               return "Normal";
        case TRAIN_SYSTEM_FAILURE:       return "System failure";
        case TRAIN_APPROACHING_CROSSING:  return "Approaching crossing";
        case TRAIN_CROSSING:             return "Crossing";
        default:                         return "Invalid state";
    }
}

const char *crossing_state_name(crossing_state_t state) {
    switch (state) {
        case X1_UNKNOWN:           return "Unknown";
        case X1_IDLE:              return "Idle";
        case X1_TRAIN_APPROACHING: return "Train approaching";
        case X1_WARNING_ACTIVE:    return "Warning active";
        case X1_GATES_LOWERING:    return "Gates lowering";
        case X1_GATES_DOWN:        return "Gates down";
        case X1_TRAIN_CROSSING:    return "Train crossing";
        case X1_TRAIN_CLEAR_WAIT:  return "Train clear wait";
        case X1_GATES_RAISING:     return "Gates raising";
        case X1_CLEAR:             return "Clear";
        case X1_FAULT:             return "Fault";
        default:                   return "Invalid state";
    }
}

void display_status(train_state_t train, crossing_state_t crossing) {
    puts("\n--- Central status ---");
    printf("Train : %s\n", train_state_name(train));
    printf("X1    : %s\n", crossing_state_name(crossing));
    puts("----------------------");
}


int make_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm utc_time;

    if (now == (time_t)-1) {
        return -1;
    }

    if (gmtime_r(&now, &utc_time) == NULL) {
        return -1;
    }

    if (strftime(buffer, buffer_size,
                 "%Y-%m-%d %H:%M:%S UTC", &utc_time) == 0) {
        return -1;
    }

    return 0;
}


int main(void) {

	const int crossing_code = _PULSE_CODE_MINAVAIL + 50;
	const int train_code = _PULSE_CODE_MINAVAIL + 51;
	train_state_t train_state = TRAIN_UNKNOWN;
	crossing_state_t crossing_state = X1_UNKNOWN;
	puts("Central controller starting...");
	//printf("Train state code: %d\n", train_state);
	//printf("X1 state code: %d\n", crossing_state);
	display_status(train_state, crossing_state);

	name_attach_t *attach = name_attach(NULL, "central_controller", 0);

	if (attach == NULL) {
	    perror("Could not create central controller channel");
	    return EXIT_FAILURE;
	}

	printf("Central channel created. Channel ID: %d\n", attach->chid);


	int exit_status = EXIT_SUCCESS;

	puts("Waiting for updates...");
	fflush(stdout);

	while (1) {
	    struct _pulse received = {0};
	    struct _msg_info info = {0};

	    int rcvid = MsgReceive(
	        attach->chid,
	        &received,
	        sizeof(received),
	        &info
	    );

	    if (rcvid == -1) {
	        if (errno == EINTR) {
	            continue;
	        }

	        perror("Could not receive an update");
	        exit_status = EXIT_FAILURE;
	        break;
	    }

	    if (rcvid == 0) {
	        if (received.code == _PULSE_CODE_DISCONNECT) {
	            if (ConnectDetach(received.scoid) == -1) {
	                perror("Could not clean up client connection");
	            }
	        } else if (received.code >= _PULSE_CODE_MINAVAIL) {
	        	//printf("Received pulse: code=%d, value=%d\n",
	        	  //     received.code,
	        	    //   received.value.sival_int);
	        	char timestamp[32];

	        	if (make_timestamp(timestamp, sizeof(timestamp)) == 0) {
	        	    printf("[%s] ", timestamp);
	        	} else {
	        	    printf("[Time unavailable] ");
	        	}

	        	printf("Received pulse: code=%d, value=%d\n",
	        	       received.code,
	        	       received.value.sival_int);

	        	if (received.code == train_code) {
	        	    int value = received.value.sival_int;
	        	    int old_state = value % 10;
	        	    int new_state = value / 10;

	        	    if (value < 0 ||
	        	        old_state > TRAIN_CROSSING ||
	        	        new_state > TRAIN_CROSSING) {
	        	        printf("Invalid train update: %d\n", value);
	        	    } else {
	        	        train_state = (train_state_t)new_state;

	        	        printf("Train update: %d -> %d\n",
	        	               old_state, new_state);
	        	    }

	        	    //printf("Stored train state: %s\n",
	        	           //train_state_name(train_state));
	        	} else if (received.code == crossing_code) {
	        	    int value = received.value.sival_int;
	        	    int old_state = value % 10;
	        	    int new_state = value / 10;

	        	    if (value < 0 ||
	        	        old_state > X1_FAULT ||
	        	        new_state > X1_FAULT) {
	        	        printf("Invalid X1 update: %d\n", value);
	        	    } else {
	        	        crossing_state = (crossing_state_t)new_state;

	        	        printf("X1 update: %d -> %d\n",
	        	               old_state, new_state);
	        	    }

	        	    //printf("Stored X1 state: %s\n",
	        	           //crossing_state_name(crossing_state));
	        	}


	        	display_status(train_state, crossing_state);
	        	fflush(stdout);
	        }

	        continue;
	    }

	    if (info.msglen >= 4 &&
	        received.type == _IO_CONNECT &&
	        received.subtype == _IO_CONNECT_OPEN) {
	        if (MsgReply(rcvid, EOK, NULL, 0) == -1) {
	            perror("Could not reply to connection request");
	        }
	    } else {
	        if (MsgError(rcvid, ENOSYS) == -1) {
	            perror("Could not reject unsupported message");
	        }
	    }
	}

	if (name_detach(attach, 0) == -1) {
	    perror("Could not remove central controller channel");
	    return EXIT_FAILURE;
	}

	return exit_status;
}
