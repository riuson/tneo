/*******************************************************************************
 *
 * TNeoKernel: real-time kernel initially based on TNKernel
 *
 *    TNKernel:                  copyright © 2004, 2013 Yuri Tiomkin.
 *    PIC32-specific routines:   copyright © 2013, 2014 Anders Montonen.
 *    TNeoKernel:                copyright © 2014       Dmitry Frank.
 *
 *    TNeoKernel was born as a thorough review and re-implementation of
 *    TNKernel. The new kernel has well-formed code, inherited bugs are fixed
 *    as well as new features being added, and it is tested carefully with
 *    unit-tests.
 *
 *    API is changed somewhat, so it's not 100% compatible with TNKernel,
 *    hence the new name: TNeoKernel.
 *
 *    Permission to use, copy, modify, and distribute this software in source
 *    and binary forms and its documentation for any purpose and without fee
 *    is hereby granted, provided that the above copyright notice appear
 *    in all copies and that both that copyright notice and this permission
 *    notice appear in supporting documentation.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE DMITRY FRANK AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DMITRY FRANK OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *    THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/**
 * \file
 *
 * Architecture-dependent routines declaration.
 */

#ifndef _TN_ARCH_H
#define _TN_ARCH_H


/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/

#include "../core/tn_common.h"



/*******************************************************************************
 *    ACTUAL PORT IMPLEMENTATION
 ******************************************************************************/

#if defined(__PIC32MX__)
#include "pic32/tn_arch_pic32.h"
#else
#error "unknown platform"
#endif



#ifdef __cplusplus
extern "C"  {  /*}*/
#endif

/*******************************************************************************
 *    PUBLIC FUNCTION PROTOTYPES
 ******************************************************************************/


/**
 * Unconditionally disable interrupts
 */
void tn_arch_int_dis(void);

/**
 * Unconditionally enable interrupts
 */
void tn_arch_int_en(void);

/**
 * Disable interrupts and return previous value of status register,
 * atomically
 *
 * @see `tn_arch_sr_restore()`
 */
unsigned int tn_arch_sr_save_int_dis(void);

/**
 * Restore previously saved status register
 *
 * @param sr   status register value previously from
 *             `tn_arch_sr_save_int_dis()`
 *
 * @see `tn_arch_sr_save_int_dis()`
 */
void tn_arch_sr_restore(unsigned int sr);




/**
 * Should return start stack address, which may be either the lowest address of
 * the stack array or the highest one, depending on the architecture.
 *
 * @param   stack_low_address    start address of the stack array.
 * @param   stack_size           size of the stack in `int`-s, not in bytes.
 */
TN_Word *_tn_arch_stack_start_get(
      TN_Word *stack_low_address,
      int stack_size
      );

/**
 * Should initialize stack for new task and return current stack pointer.
 *
 * @param task_func
 *    Pointer to task body function.
 * @param stack_start
 *    Start address of the stack, returned by `_tn_arch_stack_start_get()`.
 * @param param
 *    User-provided parameter for task body function.
 *
 * @return current stack pointer (top of the stack)
 */
unsigned int *_tn_arch_stack_init(
      TN_TaskBody   *task_func,
      TN_Word       *stack_start,
      void          *param
      );

/**
 * Should return 1 if ISR is currently running, 0 otherwise
 */
int _tn_arch_inside_isr(void);

/**
 * Called whenever we need to switch context to other task.
 *
 * **Preconditions:**
 *
 * * interrupts are enabled;
 * * `tn_curr_run_task` points to currently running (preempted) task;
 * * `tn_next_task_to_run` points to new task to run.
 *    
 * **Actions to perform:**
 *
 * * save context of the preempted task to its stack;
 * * set `tn_curr_run_task` to `tn_next_task_to_run`;
 * * switch context to it.
 *
 * @see `tn_curr_run_task`
 * @see `tn_next_task_to_run`
 */
void _tn_arch_context_switch(void);

/**
 * Called when some task calls `tn_task_exit()`.
 *
 * **Preconditions:**
 *
 * * interrupts are disabled;
 * * `tn_next_task_to_run` is already set to other task.
 *    
 * **Actions to perform:**
 *
 * * set `tn_curr_run_task` to `tn_next_task_to_run`;
 * * switch context to it.
 *
 * @see `tn_curr_run_task`
 * @see `tn_next_task_to_run`
 */
void _tn_arch_context_switch_exit(void);

/**
 * Should perform first context switch (to the task pointed to by 
 * `tn_next_task_to_run`).
 *
 * **Preconditions:**
 *
 * * no interrupts are set up yet, so, it's like interrupts disabled
 * * `tn_next_task_to_run` is already set to idle task.
 *    
 * **Actions to perform:**
 *
 * * set `#TN_STATE_FLAG__SYS_RUNNING` flag in the `tn_sys_state` variable;
 * * set `tn_curr_run_task` to `tn_next_task_to_run`;
 * * switch context to it.
 *
 * @see `#TN_STATE_FLAG__SYS_RUNNING`
 * @see `tn_sys_state`
 * @see `tn_curr_run_task`
 * @see `tn_next_task_to_run`
 */
void _tn_arch_system_start(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif









#endif  /* _TN_ARCH_H */
