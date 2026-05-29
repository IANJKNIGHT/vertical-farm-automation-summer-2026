#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

extern char key_char;
extern void turn_on_selected_leds(char key);
extern char last_num_entered;
// Remove 'extern' and add 'volatile'
volatile uint8_t blink_step = 0; // Tracks our current position in time
extern bool time_val_for_fan_displayed;
extern bool first_num_set;
extern absolute_time_t mode_switch_time_remaining_to_set;

enum SystemTimerState
{
    MODE_DEFAULT,
    MODE_MANUAL_ADJUST,
    SEL_TIME_VALS,
    ENTER_DAYS,
    ENTER_HOURS,
    ENTER_MINUTES,
    ENTER_SECONDS,
    MODE_TIMED_RUN
};

extern enum SystemTimerState system_timer_state;

enum EnterState
{
    NO_ENTRY,
    FIRST_PRESS,
    SAVE_STATE_1,
    SECOND_PRESS,
    SAVE_STATE_2,
    TOO_BIG
};

extern enum EnterState enter_state;

extern int remaining_days;
extern int remaining_hours;
extern int remaining_minutes;
extern int remaining_seconds_seconds;
extern bool entry_too_large_error; // when the entered time is greater than 23 hours or 59 min/seconds
extern bool time_interval_head_set;
extern bool linked_list_set; 
struct timerInterval
{
    enum SystemTimerState timer_state;
    struct timerInterval *next;
    struct timerInterval *prev;
};
extern struct timerInterval *timer_interval_head;
struct timerInterval *next;
char possible_inputs_array[5] = {'A', 'B', 'C', 'D'};
enum SystemTimerState possible_states_arr[4] = {ENTER_DAYS, ENTER_HOURS, ENTER_MINUTES, ENTER_SECONDS};
extern int timer_interval_index;
int set_idx = 4;
void start_new_blink_sequence();

void printCurrentState(struct timerInterval *current_interval_entry)
{
    switch (current_interval_entry->timer_state)
    {
    case ENTER_DAYS:
        printf("Only state: Days\n");
        break;

    case ENTER_HOURS:
        printf("Only state: Hrs\n");
        break;

    case ENTER_MINUTES:
        printf("Only state: Mins\n");
        break;

    case ENTER_SECONDS:
        printf("Only state: Secs\n");
        break;
    default:
        break;
    }
}

void printConfiguredIntervals()
{
    printf("\n--- CURRENT CONFIGURATION CHAIN ---\n");
    if (timer_interval_head == NULL)
    {
        printf("List is empty!\n");
        printf("------------------------------------\n\n");
        return;
    }

    struct timerInterval *temp = timer_interval_head;
    int position = 1;
    while (temp != NULL)
    {
        printf("[%d] ", position++);
        printCurrentState(temp);
        temp = temp->next; // Move safely down the line
    }
    printf("------------------------------------\n\n");
}

void setTimeIntervals()
{
    // If the user presses '#' again, it means they are done choosing intervals
    if (key_char == '#')
    {
        linked_list_set = true;

        printConfiguredIntervals();
        if (timer_interval_head != NULL)
        {
            system_timer_state = timer_interval_head->timer_state;
            enter_state = NO_ENTRY;
        }
        else
        {
            // Safe fallback if they pressed # without choosing any intervals
            system_timer_state = MODE_DEFAULT;
        }
        return;
    }

    // Match the key pressed to our available time states
    int found_idx = -1;
    for (int i = 0; i < 4; i++)
    {
        // printf("key_char = %c and current_arr_input = %c\n", key_char, possible_inputs_array[i]);
        if (key_char == possible_inputs_array[i])
        {
            found_idx = i;
            break;
        }
    }

    // Allocate and append only if it's a valid dynamic selection (A, B, C, or D)
    if (found_idx != -1)
    {
        struct timerInterval *newNode = (struct timerInterval *)malloc(sizeof(struct timerInterval));
        if (newNode == NULL)
            return; // Heap safety check

        newNode->timer_state = possible_states_arr[found_idx];
        newNode->next = NULL;

        if (!time_interval_head_set)
        {
            timer_interval_head = newNode;
            timer_interval_head->prev = NULL;
            time_interval_head_set = true;
        }
        else
        {
            struct timerInterval *temp = timer_interval_head;
            while (temp->next != NULL)
            {
                temp = temp->next;
            }
            temp->next = newNode;
            temp->next->prev = temp;
        }
        // printf("Added Interval State: %d\n", newNode->timer_state);
    }
}

// void setTransitionStates()
// {
//     struct timerInterval * temp = timer_interval_head;
//     if (temp == NULL)
//     {
//         return;
//     }
//     else if (temp->next == NULL)
//     {
//         switch(temp->timer_state)
//         {
//             case ENTER_DAYS:
//                 after_enter_days_state = MODE_TIMED_RUN;
//                 break;
//             case ENTER_HOURS:
//                 before_enter_hrs_state = MODE_MANUAL_ADJUST;
//                 after_enter_hrs_state = MODE_TIMED_RUN;
//                 break;
//             case ENTER_MINUTES:
//                 before_enter_hrs_state = MODE_MANUAL_ADJUST;
//                 after_enter_hrs_state = MODE_TIMED_RUN;
//             case ENTER_SECONDS:
//                 before
//         }
//     }
// }

void destroyList()
{
    struct timerInterval *temp = timer_interval_head;
    // Check if temp itself is NULL BEFORE looking inside it
    while (temp != NULL)
    {
        struct timerInterval *next_node = temp->next; // Safely look ahead while temp is valid
        // printCurrentState(temp);
        free(temp);       // Safely delete current
        temp = next_node; // Advance
    }

    // Reset control variables so we don't try to use freed memory
    timer_interval_head = NULL;
    time_interval_head_set = false;
}

struct timerInterval * findTimeEntryType()
{
    if (timer_interval_head == NULL)
    {
        return NULL;
    }
    else 
    {
        struct timerInterval *temp = timer_interval_head; // holds head value to be able to move
        while (system_timer_state != temp->timer_state && temp->next != NULL)
        {
            temp = temp->next;
        }
        if (system_timer_state == temp->timer_state)
        {
            return temp;
        }
        else 
        {
            return NULL;
        }
    }
}

// void save2_next_systemTimerState(bool forward): moves user to the next or previous system_timer_state based toggling bool forward to true or false
// args: bool forward - moves to the next state (true), previous state (false)
void save2_next_systemTimerState(bool forward)
{
    struct timerInterval *temp = findTimeEntryType();
    if (temp == NULL)
    {
        system_timer_state = MODE_DEFAULT;
    }
    else if (forward)
    {
        if (temp->next != NULL)
        {
            system_timer_state = temp->next->timer_state;
        }
        else
        {
            system_timer_state = MODE_TIMED_RUN;
            destroyList();
        }
    }
    else 
    {
        if (temp->prev != NULL)
        {
            system_timer_state = temp->prev->timer_state;
        }
        else
        {
            system_timer_state = MODE_MANUAL_ADJUST;
            destroyList();
        }
    }
}

// void no_entry_back_state()
// {
//     enter_state = NO_ENTRY;
//     switch (system_timer_state)
//     {
//     case ENTER_DAYS:
//         system_timer_state = MODE_MANUAL_ADJUST;
//         break;
//     case ENTER_HOURS:
//         system_timer_state = ENTER_DAYS;
//         break;
//     case ENTER_MINUTES:
//         system_timer_state = ENTER_HOURS;
//         break;
//     case ENTER_SECONDS:
//         system_timer_state = ENTER_MINUTES;
//         break;
//     case MODE_TIMED_RUN:
//         system_timer_state = ENTER_SECONDS;
//         break;
//     case MODE_DEFAULT:
//     case MODE_MANUAL_ADJUST:
//         break;
//     }
// }

void noEntryLogic(char stable_key)
{

    switch (stable_key)
    {
        case '0' ... '9':
            enter_state = FIRST_PRESS;
            start_new_blink_sequence();
            last_num_entered = stable_key;
            first_num_set = true;
            break;
        case 'B':
            save2_next_systemTimerState(false);
            break;
        default:
            break;
    }
}

void time_type_saved_save_state_1(char stable_key)
{
    int remaining_time = 10 * (stable_key - '0');
    // FIX: Switch on system_timer_state, not enter_state
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        remaining_days = remaining_time;
        break;
    case ENTER_HOURS:
        remaining_hours = remaining_time;
        break;
    case ENTER_MINUTES:
        remaining_minutes = remaining_time;
        break;
    case ENTER_SECONDS:
        remaining_seconds_seconds = remaining_time;
        break;
    default:
        break;
    }
}

void firstPressLogic(char stable_key)
{
    switch (stable_key)
    {
    case '*':
        enter_state = SAVE_STATE_1;
        time_type_saved_save_state_1(last_num_entered);
        printf("D: %d, H: %d, M: %d, S: %d\n", remaining_days, remaining_hours, remaining_minutes, remaining_seconds_seconds);
        first_num_set = false;
        break;
    case 'B':
        enter_state = NO_ENTRY;
        break;
    default:
        break;
    }
}

void save_state_1_logic(char stable_key)
{
    switch (stable_key)
    {
    case '0' ... '9':
        enter_state = SECOND_PRESS;
        start_new_blink_sequence();
        last_num_entered = stable_key;
        break;
    case 'B':
        enter_state = NO_ENTRY;
        break;
    default:
        break;
    }
}

void entry_too_large_logic()
{
    entry_too_large_error = true;
    // printf("HELLO???\n");

    start_new_blink_sequence();
}

int less_than_59_check(int remaining_time)
{
    return (remaining_time >= 0 && remaining_time <= 59);
}

void time_type_saved_save_state_2(char stable_key)
{
    // FIX: This is the ones digit, do not multiply by 10
    int remaining_time = (stable_key - '0');

    // FIX: Switch on system_timer_state, not enter_state
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        remaining_days += remaining_time;
        break;
    case ENTER_HOURS:
        remaining_hours += remaining_time;
        // if (remaining_hours > 23)
        //     remaining_hours = 0;
        // enter_state = NO_ENTRY;
        // entry_too_large_error = true;
        // start_new_blink_sequence();
        break;
    case ENTER_MINUTES:
        remaining_minutes += remaining_time;
        // if (less_than_59_check(remaining_minutes) == 0)
        // {
        //     entry_too_large_logic();
        // }

        break;
    case ENTER_SECONDS:
        remaining_seconds_seconds += remaining_time;
        // if (less_than_59_check(remaining_minutes) == 0)
        // {
        //     entry_too_large_logic();
        // }
        break;
    default:
        break;
    }
}

bool validEntry()
{
    printf("Checking_valid\n");
    switch (system_timer_state)
    {
    case ENTER_HOURS:
        if (remaining_hours > 23)
        {
            remaining_hours = 0;
            entry_too_large_logic();
            return false;
        }
        break;
    case ENTER_MINUTES:
        if (!less_than_59_check(remaining_minutes))
        {
            // printf("invalid\n");
            entry_too_large_logic();
            return false;
        }
        break;
    case ENTER_SECONDS:
        if ((!less_than_59_check(remaining_seconds_seconds)))
        {
            entry_too_large_logic();
            return false;
        }
        break;
    default:
        break;
    }
    return true;
}

void secondPressLogic(char stable_key)
{
    switch (stable_key)
    {
    case '*':
        enter_state = SAVE_STATE_2;
        time_type_saved_save_state_2(last_num_entered);
        printf("D: %d, H: %d, M: %d, S: %d\n", remaining_days, remaining_hours, remaining_minutes, remaining_seconds_seconds);
        break;
    case 'B':
        enter_state = NO_ENTRY;
        break;
    default:
        break;
    }
}


// void save2_next_systemTimerState(struct timerInterval * chosen_intervals) chooses the next enter_state based on the values in the linkedlist
// arguments: struct timerInterval * chosen_intervals stores the chosen states from the user input representing some sequence of 'A', 'B', 'C', 'D' corresponding to D, H, M, S
// switch
// void save2_next_systemTimerState()
// {
//     switch (system_timer_state)
//     {
//     case ENTER_DAYS:
//         system_timer_state = after_enter_days_state;
//         break;
//     case ENTER_HOURS:
//         system_timer_state = after_enter_hrs_state;
//         break;
//     case ENTER_MINUTES:
//         system_timer_state = after_enter_mins_state;
//         break;
//     case ENTER_SECONDS:
//         system_timer_state = MODE_TIMED_RUN;
//         break;
//     default:
//         break;
//     }
// }
// struct timerInterval * findTimeEntryType(): returns the Node that stores the current position of where the user is entering the time


void save_state_2_logic(char stable_key)
{
    enter_state = NO_ENTRY;
    switch (stable_key)
    {
    case 'D':
        time_val_for_fan_displayed = true;
        start_new_blink_sequence();
        if (validEntry())
            save2_next_systemTimerState(true); // 2. Then transition states safely
        if (system_timer_state == MODE_TIMED_RUN)
        {
            mode_switch_time_remaining_to_set = get_absolute_time();
        }
        // save2_next_systemTimerState(); // 2. Then transition states safely

        break;
    case 'B':
        break;
    default:
        enter_state = SAVE_STATE_2;
        break;
    }
}

void debug_enter_state()
{
    switch (enter_state)
    {
    case NO_ENTRY:
        printf("No Entry\n");
        break;
    case FIRST_PRESS:
        printf("1st\n");
        break;
    case SAVE_STATE_1:
        printf("SS1\n");
        break;
    case SECOND_PRESS:
        printf("2nd\n");
        break;
    case SAVE_STATE_2:
        printf("SS2\n");
        break;
    }
}

void debug_system_timer_state()
{
    switch (system_timer_state)
    {
    case MODE_DEFAULT:
        printf("Def\n");
        break;
    case MODE_MANUAL_ADJUST:
        printf("Man\n");
        break;
    case SEL_TIME_VALS:
        printf("Vals\n");
        break;
    case ENTER_DAYS:
        printf("Days\n");
        break;
    case ENTER_HOURS:
        printf("Hrs\n");
        break;
    case ENTER_MINUTES:
        printf("Mins\n");
        break;
    case ENTER_SECONDS:
        printf("Secs\n");
        break;
    case MODE_TIMED_RUN:
        printf("Timed\n");
        break;
    }
}