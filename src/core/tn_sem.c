/*******************************************************************************
 *
 * TNeoKernel: real-time kernel initially based on TNKernel
 *
 *    TNKernel:                  copyright � 2004, 2013 Yuri Tiomkin.
 *    PIC32-specific routines:   copyright � 2013, 2014 Anders Montonen.
 *    TNeoKernel:                copyright � 2014       Dmitry Frank.
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


/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/

//-- common tnkernel headers
#include "tn_common.h"
#include "tn_sys.h"
#include "tn_internal.h"

//-- header of current module
#include "tn_sem.h"

//-- header of other needed modules
#include "tn_tasks.h"




/*******************************************************************************
 *    PRIVATE FUNCTIONS
 ******************************************************************************/

//-- Additional param checking {{{
#if TN_CHECK_PARAM
static inline enum TN_RCode _check_param_generic(
      struct TN_Sem *sem
      )
{
   enum TN_RCode rc = TN_RC_OK;

   if (sem == NULL){
      rc = TN_RC_WPARAM;
   } else if (sem->id_sem != TN_ID_SEMAPHORE){
      rc = TN_RC_INVALID_OBJ;
   }

   return rc;
}

static inline enum TN_RCode _check_param_create(
      struct TN_Sem *sem,
      int start_count,
      int max_count
      )
{
   enum TN_RCode rc = TN_RC_OK;

   if (sem == NULL){
      rc = TN_RC_WPARAM;
   } else if (0
         || sem->id_sem == TN_ID_SEMAPHORE
         || max_count <= 0
         || start_count < 0
         || start_count > max_count
         )
   {
      rc = TN_RC_WPARAM;
   }

   return rc;
}

#else
#  define _check_param_generic(sem)                            (TN_RC_OK)
#  define _check_param_create(sem, start_count, max_count)     (TN_RC_OK)
#endif
// }}}


static inline enum TN_RCode _sem_job_perform(
      struct TN_Sem *sem,
      int (p_worker)(struct TN_Sem *sem),
      unsigned long timeout
      )
{
   TN_INTSAVE_DATA;
   enum TN_RCode rc = TN_RC_OK;
   BOOL waited_for_sem = FALSE;

   rc = _check_param_generic(sem);
   if (rc != TN_RC_OK){
      goto out;
   }

   if (!tn_is_task_context()){
      rc = TN_RC_WCONTEXT;
      goto out;
   }

   TN_INT_DIS_SAVE();

   rc = p_worker(sem);

   if (rc == TN_RC_TIMEOUT && timeout != 0){
      _tn_task_curr_to_wait_action(
            &(sem->wait_queue), TN_WAIT_REASON_SEM, timeout
            );

      //-- rc will be set later thanks to waited_for_sem
      waited_for_sem = TRUE;
   }

#if TN_DEBUG
   if (!_tn_need_context_switch() && waited_for_sem){
      _TN_FATAL_ERROR("");
   }
#endif

   TN_INT_RESTORE();
   _tn_switch_context_if_needed();
   if (waited_for_sem){
      rc = tn_curr_run_task->task_wait_rc;
   }

out:
   return rc;
}

static inline enum TN_RCode _sem_job_iperform(
      struct TN_Sem *sem,
      int (p_worker)(struct TN_Sem *sem)
      )
{
   TN_INTSAVE_DATA_INT;
   enum TN_RCode rc = TN_RC_OK;

   rc = _check_param_generic(sem);
   if (rc != TN_RC_OK){
      goto out;
   }

   if (!tn_is_isr_context()){
      rc = TN_RC_WCONTEXT;
      goto out;
   }

   TN_INT_IDIS_SAVE();

   rc = p_worker(sem);

   TN_INT_IRESTORE();

out:
   return rc;
}

static inline enum TN_RCode _sem_signal(struct TN_Sem *sem)
{
   enum TN_RCode rc = TN_RC_OK;

   if (!(tn_is_list_empty(&(sem->wait_queue)))){
      struct TN_Task *task;
      //-- there are tasks waiting for that semaphore,
      //   so, wake up first one

      //-- get first task from semaphore's wait_queue
      task = tn_list_first_entry(
            &(sem->wait_queue), typeof(*task), task_queue
            );

      //-- wake it up
      _tn_task_wait_complete(task, TN_RC_OK);
   } else {
      //-- no tasks are waiting for that semaphore,
      //   so, just increase its count if possible.
      if (sem->count < sem->max_count){
         sem->count++;
      } else {
         rc = TN_RC_OVERFLOW;
      }
   }

   return rc;
}

static inline enum TN_RCode _sem_acquire(struct TN_Sem *sem)
{
   enum TN_RCode rc = TN_RC_OK;

   if (sem->count >= 1){
      sem->count--;
   } else {
      rc = TN_RC_TIMEOUT;
   }

   return rc;
}





/*******************************************************************************
 *    PUBLIC FUNCTIONS
 ******************************************************************************/

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_create(
      struct TN_Sem *sem,
      int start_count,
      int max_count
      )
{
   enum TN_RCode rc = TN_RC_OK;

   rc = _check_param_create(sem, start_count, max_count);
   if (rc != TN_RC_OK){
      goto out;
   }

   if (!tn_is_task_context()){
      rc = TN_RC_WCONTEXT;
      goto out;
   }

   tn_list_reset(&(sem->wait_queue));

   sem->count     = start_count;
   sem->max_count = max_count;
   sem->id_sem    = TN_ID_SEMAPHORE;

out:
   return rc;
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_delete(struct TN_Sem * sem)
{
   TN_INTSAVE_DATA;
   enum TN_RCode rc = TN_RC_OK;

   rc = _check_param_generic(sem);
   if (rc != TN_RC_OK){
      goto out;
   }

   if (!tn_is_task_context()){
      rc = TN_RC_WCONTEXT;
      goto out;
   }

   TN_INT_DIS_SAVE();

   _tn_wait_queue_notify_deleted(&(sem->wait_queue));

   sem->id_sem = 0; //-- Semaphore does not exist now

   TN_INT_RESTORE();

   //-- we might need to switch context if _tn_wait_queue_notify_deleted()
   //   has woken up some high-priority task
   _tn_switch_context_if_needed();

out:
   return rc;
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_signal(struct TN_Sem *sem)
{
   return _sem_job_perform(sem, _sem_signal, 0);
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_isignal(struct TN_Sem *sem)
{
   return _sem_job_iperform(sem, _sem_signal);
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_acquire(struct TN_Sem *sem, TN_Timeout timeout)
{
   return _sem_job_perform(sem, _sem_acquire, timeout);
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_acquire_polling(struct TN_Sem *sem)
{
   return _sem_job_perform(sem, _sem_acquire, 0);
}

/*
 * See comments in the header file (tn_sem.h)
 */
enum TN_RCode tn_sem_iacquire_polling(struct TN_Sem *sem)
{
   return _sem_job_iperform(sem, _sem_acquire);
}

