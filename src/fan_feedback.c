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
    float output; /* output of the PID */ 
    uint16_t sam_rate; /* sampling rate */ 
    float integral_max; /* Maximum of the error integral */ 
    float pid_max; /* Maximum of the PID */ 
} pid_instance;

typedef enum { 
   pid_ok = 0, 
   pid_numerical 
}pid_typedef; 

void set_pid(pid_instance *pid, float p, float i, float d)
{
    pid->integral_error = 0;
    pid->Kp = p;
    pid->Ki = i;
    pid->Kd = d;
}

void reset_pid(pid_instance *pid)
{
    pid->integral_error = 0;
    pid->prev_error = 0;
}

pid_typedef apply_pid(pid_instance *pid, float input_error)
{
    pid ->integral_error += input_error;
    if(pid->integral_error > pid ->integral_max)
    {
        pid->integral_error = pid->integral_max;
    }
    if(pid->integral_error < -pid->integral_max)
    {
        pid->integral_error = -pid ->integral_max;
    }
    if ( pid ->sam_rate == 0)
    {
        return pid_numerical;
    }
    pid ->output = pid ->Kp * input_error +
            pid ->Ki * (pid->integral_error) / pid ->sam_rate +
            pid ->Kd * pid ->sam_rate * (input_error - pid->prev_error);


    if(pid->output >= pid ->pid_max)
    {
        pid->output = pid ->pid_max;
    }
    if(pid->output <= -pid ->pid_max)
    {
        pid->output = -pid ->pid_max;
    }
    pid->prev_error = input_error;


    return pid_ok;
} 


void run_rpm_pid(int target_rpm, int current_rpm) {
    // fill in

}