/** @file px14_comp.h
 */

#ifndef px14_comp_header_defined
#define px14_comp_header_defined

struct my_completion {
  unsigned int done;
  wait_queue_head_t wait;
};

extern int
my_wait_for_completion_interruptible(struct my_completion *x);

extern unsigned long
my_wait_for_completion_interruptible_timeout(struct my_completion *x,
					     unsigned long timeout);

extern unsigned long
my_wait_for_completion_timeout(struct my_completion *x,
			       unsigned long timeout);

extern void
my_complete_all(struct my_completion *);

static inline void my_init_completion(struct my_completion *x)
{
  x->done = 0;
  init_waitqueue_head(&x->wait);
}


#endif // px14_comp_header_defined



