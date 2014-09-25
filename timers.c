#include <stdio.h>
#include <stdlib.h>
#include "lists.h"

/* init code */
typedef int (*__initcall_t)(void);
#define init_call(fn) \
	static __initcall_t __initcall_##fn \
	__attribute__((__unused__)) \
	__attribute__((__section__("__init_call"))) = &fn


#define MODULE_NAME_LEN 256
struct module {
	char name[MODULE_NAME_LEN];
};

/* not used at the moment */
#define TIMER_MAGIC	0x4b87ad6e

struct timer_list {
	struct list_head entry;
	unsigned long expires;

//	spinlock_t lock;
	unsigned long magic;

	void (*function)(unsigned long);
	unsigned long data;

	struct module *owner;
	const char *ownerfunction;

	char *use;
};

/* for internal timer management:
 * Increments the current system time, and calls any timers that
 * are have been scheduled within this period
 */
void increment_time(unsigned int inc);

#define init_timer(t) __init_timer(t,THIS_MODULE,__FUNCTION__)
void __init_timer(struct timer_list * timer, struct module *owner, const char *function);

int timer_pending(const struct timer_list * timer);
void check_timer(struct timer_list *timer);

#define del_timer(timer) __del_timer((timer), __location__)
int __del_timer(struct timer_list *timer, const char *location);
void check_timer_failed(struct timer_list *timer);

#define add_timer(timer) __add_timer((timer), __location__)
void __add_timer(struct timer_list *timer, const char *location);
int __mod_timer(struct timer_list *timer, unsigned long expires);


#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

#define time_after(a,b)		\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)(b) - (long)(a) < 0))
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)(a) - (long)(b) >= 0))
#define time_before_eq(a,b)	time_after_eq(b,a)


#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})



#define INITIAL_JIFFIES 0
extern unsigned long jiffies;


LIST_HEAD(__timers);
LIST_HEAD(__running_timers);

void __init_timer(struct timer_list *timer, struct module *owner,
	const char *function)
{
	timer->magic = TIMER_MAGIC;
	timer->owner = owner;
	timer->ownerfunction = function;
	timer->use = NULL;
}

void __add_timer(struct timer_list *timer, const char *location)
{
	struct timer_list *t;
	list_for_each_entry(t, &__timers, entry) {
		if (time_after(t->expires, timer->expires)) 
			break;
	}
	list_add_tail(&timer->entry, &t->entry);

//	timer->use = talloc_strdup(__timer_ctx, location);
}

int __del_timer(struct timer_list *timer, const char *location)
{
	if (!timer->use)
		return 0;

	if (should_i_fail_once(location)) {
		/* Pretend it's running now. */
		list_del(&timer->entry);

		list_add(&timer->entry, &__running_timers);
		return 0;
	}

	list_del(&timer->entry);

//	talloc_free(timer->use);

	timer->use = NULL;

	return 1;
}

int do_running_timers(void)
{
	struct timer_list *t, *next;
	int ret = -1;
	
	list_for_each_entry_safe(t, next, &__running_timers, entry) {
		list_del(&t->entry);

//		talloc_free(t->use);

		t->function(t->data);
		ret = 0;
	}
	return ret;
}

static int run_timers(const char *command)
{
	do_running_timers();
	return -1;
}

static int setup_running_timers(void)
{

}
init_call(setup_running_timers);

int timer_pending(const struct timer_list * timer)
{
	/* straightforward at present - timers are guaranteed to
	   be run at the expiry time
	 */
	return timer->expires > jiffies;
}

void increment_time(unsigned int inc)
{
	struct list_head *i;
	struct timer_list *t;

	jiffies += inc;
	
	i = __timers.next;
	
	while (i != &__timers) {
		t = list_entry(i, struct timer_list, entry);
		if (time_before(jiffies, t->expires))
			break;
		printf("running timer to %s:%s()", t->owner->name,
			t->ownerfunction, t->function);
		i = i->next;
		list_del(&t->entry);

//		talloc_free(t->use);

		t->use = NULL;
		t->function(t->data);
	}
}


static void check_timer_failed(struct timer_list *timer)
{
	static int whine_count;
	if (whine_count < 16) {
		whine_count++;
		printk("Uninitialised timer!\n");
		printk("This is just a warning.  Your computer is OK\n");
		printk("function=0x%p, data=0x%lx\n",
			timer->function, timer->data);
		dump_stack();
	}
	/*
	 * Now fix it up
	 */
	spin_lock_init(&timer->lock);
	timer->magic = TIMER_MAGIC;
}

static inline void check_timer(struct timer_list *timer)
{
	if (timer->magic != TIMER_MAGIC)
		check_timer_failed(timer);
}





int __mod_timer(struct timer_list *timer, unsigned long expires)
{
	tvec_base_t *old_base, *new_base;
	unsigned long flags;
	int ret = 0;

	BUG_ON(!timer->function);

	check_timer(timer);

	spin_lock_irqsave(&timer->lock, flags);
	new_base = &__get_cpu_var(tvec_bases);
repeat:
	old_base = timer->base;

	/*
	 * Prevent deadlocks via ordering by old_base < new_base.
	 */
	if (old_base && (new_base != old_base)) {
		if (old_base < new_base) {
			spin_lock(&new_base->lock);
			spin_lock(&old_base->lock);
		} else {
			spin_lock(&old_base->lock);
			spin_lock(&new_base->lock);
		}
		/*
		 * The timer base might have been cancelled while we were
		 * trying to take the lock(s):
		 */
		if (timer->base != old_base) {
			spin_unlock(&new_base->lock);
			spin_unlock(&old_base->lock);
			goto repeat;
		}
	} else {
		spin_lock(&new_base->lock);
		if (timer->base != old_base) {
			spin_unlock(&new_base->lock);
			goto repeat;
		}
	}

	/*
	 * Delete the previous timeout (if there was any), and install
	 * the new one:
	 */
	if (old_base) {
		list_del(&timer->entry);
		ret = 1;
	}
	timer->expires = expires;
	internal_add_timer(new_base, timer);
	timer->base = new_base;

	if (old_base && (new_base != old_base))
		spin_unlock(&old_base->lock);
	spin_unlock(&new_base->lock);
	spin_unlock_irqrestore(&timer->lock, flags);

	return ret;
}














