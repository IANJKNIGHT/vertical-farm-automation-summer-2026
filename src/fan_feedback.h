#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"

typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float prev_error;
    float integral_error;
    float output;       /* output of the PID */
    float sam_rate;     /* sampling rate */
    float integral_max; /* Maximum of the error integral */
    float pid_max;      /* Maximum of the PID */
} pid_instance;

typedef enum
{
    pid_ok = 0,
    pid_numerical
} pid_typedef;