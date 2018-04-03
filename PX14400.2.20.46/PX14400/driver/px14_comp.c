/** @file 	px14_comp.c
    @brief	Alternate implementation of kernel completion object

    Kerenel versions less than 2.6.11 do not implement the kernel's
    completion API so we're re-implementing what we need for these older 
    kernels.
*/
#include "px14_drv.h"
#include "px14_comp.h"
#include <linux/sched.h>

#ifdef PX14PP_NEED_COMPLETION_IMP

static void __wake_up_common(wait_queue_head_t *q, unsigned int mode,
                             int nr_exclusive, int sync, void *key)
{
  struct list_head *tmp, *next;

  list_for_each_safe(tmp, next, &q->task_list) {
    wait_queue_t *curr;
    unsigned flags;
    curr = list_entry(tmp, wait_queue_t, task_list);
    flags = curr->flags;
    if (curr->func(curr, mode, sync, key) &&
	(flags & WQ_FLAG_EXCLUSIVE) &&
	!--nr_exclusive)
      break;
  }
}


int my_wait_for_completion_interruptible(struct my_completion *x)
{
  int ret = 0;

  might_sleep();

  spin_lock_irq(&x->wait.lock);
  if (!x->done) {
    DECLARE_WAITQUEUE(wait, current);

    wait.flags |= WQ_FLAG_EXCLUSIVE;
    __add_wait_queue_tail(&x->wait, &wait);
    do {
      if (signal_pending(current)) {
	ret = -ERESTARTSYS;
	__remove_wait_queue(&x->wait, &wait);
	goto out;
      }
      __set_current_state(TASK_INTERRUPTIBLE);
      spin_unlock_irq(&x->wait.lock);
      schedule();
      spin_lock_irq(&x->wait.lock);
    } while (!x->done);
    __remove_wait_queue(&x->wait, &wait);
  }
  x->done--;
 out:
  spin_unlock_irq(&x->wait.lock);

  return ret;
}

unsigned long my_wait_for_completion_interruptible_timeout(struct my_completion *x,
							   unsigned long timeout)
{
  might_sleep();

  spin_lock_irq(&x->wait.lock);
  if (!x->done) {
    DECLARE_WAITQUEUE(wait, current);

    wait.flags |= WQ_FLAG_EXCLUSIVE;
    __add_wait_queue_tail(&x->wait, &wait);
    do {
      if (signal_pending(current)) {
	timeout = -ERESTARTSYS;
	__remove_wait_queue(&x->wait, &wait);
	goto out;
      }
      __set_current_state(TASK_INTERRUPTIBLE);
      spin_unlock_irq(&x->wait.lock);
      timeout = schedule_timeout(timeout);
      spin_lock_irq(&x->wait.lock);
      if (!timeout) {
	__remove_wait_queue(&x->wait, &wait);
	goto out;
      }
    } while (!x->done);
    __remove_wait_queue(&x->wait, &wait);
  }
  x->done--;
 out:
  spin_unlock_irq(&x->wait.lock);
  return timeout;
}

unsigned long my_wait_for_completion_timeout(struct my_completion *x,
					     unsigned long timeout)
{
  might_sleep();

  spin_lock_irq(&x->wait.lock);
  if (!x->done) {
    DECLARE_WAITQUEUE(wait, current);

    wait.flags |= WQ_FLAG_EXCLUSIVE;
    __add_wait_queue_tail(&x->wait, &wait);
    do {
      __set_current_state(TASK_UNINTERRUPTIBLE);
      spin_unlock_irq(&x->wait.lock);
      timeout = schedule_timeout(timeout);
      spin_lock_irq(&x->wait.lock);
      if (!timeout) {
	__remove_wait_queue(&x->wait, &wait);
	goto out;
      }
    } while (!x->done);
    __remove_wait_queue(&x->wait, &wait);
  }
  x->done--;
 out:
  spin_unlock_irq(&x->wait.lock);
  return timeout;
}

void my_complete_all(struct my_completion *x)
{
  unsigned long flags;

  spin_lock_irqsave(&x->wait.lock, flags);
  x->done += UINT_MAX/2;
  __wake_up_common(&x->wait,
		   TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE, 0, 0, NULL);
  spin_unlock_irqrestore(&x->wait.lock, flags);
}




#endif  // PX14PP_NEED_COMPLETION_IMP

