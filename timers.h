#define TIMER_MAGIC	0x4b87ad6e


struct timer_list {
	struct list_head entry;
	unsigned long expires;

	spinlock_t lock;
	unsigned long magic;

	void (*function)(unsigned long);
	unsigned long data;
};


/***
 * init_timer - initialize a timer.
 * @timer: the timer to be initialized
 *
 * init_timer() must be done to a timer prior calling *any* of the
 * other timer functions.
 */
static inline void init_timer(struct timer_list * timer)
{
	timer->base = NULL;
	timer->magic = TIMER_MAGIC;
	spin_lock_init(&timer->lock);
}


/***
 * add_timer - start a timer
 * @timer: the timer to be added
 *
 * The kernel will do a ->function(->data) callback from the
 * timer interrupt at the ->expired point in the future. The
 * current time is 'jiffies'.
 *
 * The timer's ->expired, ->function (and if the handler uses it, ->data)
 * fields must be set prior calling this function.
 *
 * Timers with an ->expired field in the past will be executed in the next
 * timer tick.
 */
static inline void add_timer(struct timer_list * timer)
{
	__mod_timer(timer, timer->expires);
}
