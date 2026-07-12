#include "fan_feedback.h"

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

// pid_typedef apply_pid(pid_instance *pid, float input_error)
// {
//     pid ->integral_error += input_error;
//     if(pid->integral_error > pid ->integral_max)
//     {
//         pid->integral_error = pid->integral_max;
//     }
//     if(pid->integral_error < -pid->integral_max)
//     {
//         pid->integral_error = -pid ->integral_max;
//     }
//     if ( pid ->sam_rate == 0)
//     {
//         return pid_numerical;
//     }
//     pid ->output = pid ->Kp * input_error +
//             pid ->Ki * (pid->integral_error) / pid ->sam_rate +
//             pid ->Kd * pid ->sam_rate * (input_error - pid->prev_error);

//     if(pid->output >= pid ->pid_max)
//     {
//         pid->output = pid ->pid_max;
//     }
//     if(pid->output <= -pid ->pid_max)
//     {
//         pid->output = -pid ->pid_max;
//     }
//     pid->prev_error = input_error;

//     return pid_ok;
// }

pid_typedef apply_pid(pid_instance *pid, float input_error)
{
    // Discrete Integration: Add error scaled by time step
    pid->integral_error += input_error * pid->sam_rate;

    // Windup Guard protection
    if (pid->integral_error > pid->integral_max)
        pid->integral_error = pid->integral_max;
    if (pid->integral_error < -pid->integral_max)
        pid->integral_error = -pid->integral_max;

    if (pid->sam_rate <= 0.0f)
        return pid_numerical;

    // // Standard Parallel PID Equation
    // pid->output = (pid->Kp * input_error) +
    //               (pid->Ki * pid->integral_error) +
    //               (pid->Kd * (input_error - pid->prev_error) / pid->sam_rate);

    // // Sanity boundary clamping limits
    // if(pid->output > pid->pid_max)  pid->output = pid->pid_max;
    // if(pid->output < -pid->pid_max) pid->output = -pid->pid_max;

    // pid->prev_error = input_error;
    // return pid_ok;
    // Standard Parallel PID Equation broken into parts for debugging
    float p_term = pid->Kp * input_error;
    float i_term = pid->Ki * pid->integral_error;

    // Guard against division by zero just in case
    float d_term = 0.0f;
    if (pid->sam_rate > 0.0f)
    {
        d_term = pid->Kd * (input_error - pid->prev_error) / pid->sam_rate;
    }

    pid->output = p_term + i_term + d_term;

    // --- DEBUG PRINT BLOCK ---
    // printf("[PID INTERNAL] Err: %0.2f | P: %0.2f | I_Err: %0.2f | I: %0.2f | D: %0.2f | Out: %0.2f\n",
    //        input_error, p_term, pid->integral_error, i_term, d_term, pid->output);
    // -------------------------

    // Sanity boundary clamping limits
    if (pid->output > pid->pid_max)
        pid->output = pid->pid_max;
    if (pid->output < -pid->pid_max)
        pid->output = -pid->pid_max;

    pid->prev_error = input_error;
    return pid_ok;
}

void print_pid_dashboard(int target, int current, pid_instance *pid) {
    // Format: TargetRPM, CurrentRPM, Kp, Ki, Kd, PrevError, IntegralError, Output, SamRate
    printf("DATA:%d,%d,%0.3f,%0.3f,%0.4f,%0.2f,%0.2f,%0.2f,%0.3f\n",
           target,
           current,
           pid->Kp,
           pid->Ki,
           pid->Kd,
           pid->prev_error,
           pid->integral_error,
           pid->output,
           pid->sam_rate);
}