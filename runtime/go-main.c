/* libgodc/runtime/go-main.c - program entry point */

#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kos.h>

extern void runtime_init(void);
extern void runtime_main(void);
extern void scheduler_run_loop(void);
extern G *__go_go(void (*fn)(void *), void *arg);

static int gargc = 0;
static char **gargv = NULL;

__attribute__((no_split_stack)) void runtime_args(int argc, char **argv)
{
    gargc = argc;
    gargv = argv;
}

__attribute__((no_split_stack)) int runtime_argc(void) { return gargc; }

__attribute__((no_split_stack)) char **runtime_argv(void) { return gargv; }

static volatile int main_completed = 0;

static void main_wrapper(void *arg)
{
    (void)arg;

    extern void go_init_main(void) __asm__("___go_init_main");
    go_init_main();

    extern void main_dot_main(void) __asm__("_main.main");
    main_dot_main();

    main_completed = 1;
}

__attribute__((no_split_stack)) int main(int argc, char **argv)
{
    runtime_args(argc, argv);
    runtime_init();
    __go_go(main_wrapper, NULL);
    scheduler_run_loop();
    arch_exit();
    return 0;
}

__attribute__((weak, no_split_stack)) void __go_go_library(void) {}
