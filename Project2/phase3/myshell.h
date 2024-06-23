#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#include "csapp.h"
#include <stdlib.h>

/* enum of job condition */
enum e_condition {
    FG = 0,
    BG,
    DIE,
    STOP,
    DONE
};

#endif