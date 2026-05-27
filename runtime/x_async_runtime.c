#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifndef X_ASYNC_DEFAULT_TICK_BUDGET
#define X_ASYNC_DEFAULT_TICK_BUDGET 1024ull
#endif

typedef uint64_t (*x_async_step_fn)(uint64_t task_handle);

enum x_task_state {
  X_TASK_RUNNABLE,
  X_TASK_WAITING,
  X_TASK_DONE,
};

enum x_wait_kind {
  X_WAIT_NONE,
  X_WAIT_READ,
  X_WAIT_WRITE,
  X_WAIT_SLEEP,
};

struct x_task {
  x_async_step_fn step;
  uint64_t *frame;
  uint64_t frame_slots;
  uint64_t pc;
  uint64_t ticks;
  uint64_t result;
  enum x_task_state state;
  struct x_task *next;
  int queued;

  enum x_wait_kind wait_kind;
  int wait_fd;
  uint64_t wait_deadline_ms;
  struct x_task *wait_next;
  int waiting;
};

static struct x_task *x_runq_head;
static struct x_task *x_runq_tail;
static struct x_task *x_waitq_head;
static uint64_t x_tick_budget;
static int x_tick_budget_initialized;

static uint64_t x_get_tick_budget(void)
{
  if (!x_tick_budget_initialized) {
    const char *env = getenv("X_ASYNC_PREEMPT_TICKS");
    x_tick_budget = X_ASYNC_DEFAULT_TICK_BUDGET;
    if (env && *env) {
      char *end = NULL;
      uint64_t parsed = strtoull(env, &end, 10);
      if (end != env && (!end || *end == '\0')) {
        x_tick_budget = parsed;
      } else {
        fprintf(stderr,
                "async runtime: ignoring invalid X_ASYNC_PREEMPT_TICKS=%s\n",
                env);
      }
    }
    x_tick_budget_initialized = 1;
  }
  return x_tick_budget;
}

void __x_scheduler_set_tick_budget(uint64_t budget)
{
  x_tick_budget = budget;
  x_tick_budget_initialized = 1;
}

static uint64_t x_now_ms(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    perror("clock_gettime");
    abort();
  }
  return (uint64_t) ts.tv_sec * 1000ull + (uint64_t) ts.tv_nsec / 1000000ull;
}

static uint64_t x_deadline_after_ms(uint64_t delay_ms)
{
  uint64_t now = x_now_ms();
  if (UINT64_MAX - now < delay_ms) {
    return UINT64_MAX;
  }
  return now + delay_ms;
}

static int x_timeout_to_int_ms(uint64_t timeout_ms)
{
  if (timeout_ms > (uint64_t) INT_MAX) {
    return INT_MAX;
  }
  return (int) timeout_ms;
}

static struct x_task *x_checked_task(uint64_t handle)
{
  struct x_task *task = (struct x_task *) (uintptr_t) handle;
  if (!task) {
    fprintf(stderr, "async runtime: null task handle\n");
    abort();
  }
  return task;
}

static void x_enqueue(struct x_task *task)
{
  if (!task || task->state == X_TASK_DONE || task->queued) {
    return;
  }

  task->state = X_TASK_RUNNABLE;
  task->next = NULL;
  task->queued = 1;
  if (x_runq_tail) {
    x_runq_tail->next = task;
  } else {
    x_runq_head = task;
  }
  x_runq_tail = task;
}

static struct x_task *x_dequeue(void)
{
  struct x_task *task = x_runq_head;
  if (!task) {
    return NULL;
  }

  x_runq_head = task->next;
  if (!x_runq_head) {
    x_runq_tail = NULL;
  }
  task->next = NULL;
  task->queued = 0;
  return task;
}

static int x_waitq_has_tasks(void)
{
  return x_waitq_head != NULL;
}

static short x_wait_events_for_kind(enum x_wait_kind kind)
{
  switch (kind) {
    case X_WAIT_READ:
      return POLLIN;
    case X_WAIT_WRITE:
      return POLLOUT;
    default:
      return 0;
  }
}

static int x_fd_ready_now(int fd, short events)
{
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;

  for (;;) {
    int rc = poll(&pfd, 1, 0);
    if (rc >= 0) {
      return rc > 0 &&
             (pfd.revents & (events | POLLERR | POLLHUP | POLLNVAL)) != 0;
    }
    if (errno == EINTR) {
      continue;
    }
    perror("poll");
    abort();
  }
}

static void x_waitq_add(struct x_task *task)
{
  if (task->waiting) {
    return;
  }
  task->wait_next = x_waitq_head;
  x_waitq_head = task;
  task->waiting = 1;
}

static void x_task_wait_fd(uint64_t handle, uint64_t raw_fd,
                           enum x_wait_kind kind)
{
  struct x_task *task = x_checked_task(handle);
  int fd = (int) raw_fd;
  short events = x_wait_events_for_kind(kind);

  if (task->state == X_TASK_DONE) {
    return;
  }

  if (events && x_fd_ready_now(fd, events)) {
    return;
  }

  task->state = X_TASK_WAITING;
  task->wait_kind = kind;
  task->wait_fd = fd;
  task->wait_deadline_ms = 0;

  x_waitq_add(task);
}

void __x_task_wait_read(uint64_t handle, uint64_t fd)
{
  x_task_wait_fd(handle, fd, X_WAIT_READ);
}

void __x_task_wait_write(uint64_t handle, uint64_t fd)
{
  x_task_wait_fd(handle, fd, X_WAIT_WRITE);
}

void __x_task_sleep_ms(uint64_t handle, uint64_t delay_ms)
{
  struct x_task *task = x_checked_task(handle);
  if (task->state == X_TASK_DONE || delay_ms == 0) {
    return;
  }

  task->state = X_TASK_WAITING;
  task->wait_kind = X_WAIT_SLEEP;
  task->wait_fd = -1;
  task->wait_deadline_ms = x_deadline_after_ms(delay_ms);

  x_waitq_add(task);
}

static int x_blocking_poll(int fd, short events, int timeout_ms)
{
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;

  for (;;) {
    int rc = poll(&pfd, 1, timeout_ms);
    if (rc >= 0) {
      return rc;
    }
    if (errno == EINTR) {
      continue;
    }
    perror("poll");
    abort();
  }
}

uint64_t __x_io_wait_read(uint64_t fd)
{
  x_blocking_poll((int) fd, POLLIN, -1);
  return 0;
}

uint64_t __x_io_wait_write(uint64_t fd)
{
  x_blocking_poll((int) fd, POLLOUT, -1);
  return 0;
}

/* This function is the blocking fallback for synchronous code,
 * while `__x_task_sleep_ms` is the real async sleep primitive.  */
uint64_t __x_sleep_ms(uint64_t delay_ms)
{
  /* Compute the deadline.  */
  uint64_t deadline = x_deadline_after_ms(delay_ms);
  for (;;) {
    /* Have we reached the deadline yet?  */
    uint64_t now = x_now_ms();
    if (now >= deadline) {
      return 0;
    }

    /* If not, sleep for timeout.  */
    int timeout = x_timeout_to_int_ms(deadline - now);
    int rc = poll(NULL, 0, timeout);
    if (rc >= 0) {
      continue;
    }

    /* Continue on interrupts.  */
    if (errno == EINTR) {
      continue;
    }

    perror("poll");
    abort();
  }
}

uint64_t __x_io_set_nonblocking(uint64_t raw_fd)
{
  int fd = (int) raw_fd;

  /* Get this fd's status flags.  */
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    perror("fcntl(F_GETFL)");
    return 1;
  }

  /* Append `O_NONBLOCK` to this fd's status flags.  */
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("fcntl(F_SETFL)");
    return 1;
  }

  return 0;
}

/* Removes one task from the wait queue, clears its wait metadata,
 * and puts it back on the run queue.  */
static void x_wake_waiter(struct x_task ***link_ptr, struct x_task *task)
{
  **link_ptr = task->wait_next;
  task->wait_next = NULL;
  task->waiting = 0;
  task->wait_kind = X_WAIT_NONE;
  task->wait_fd = -1;
  task->wait_deadline_ms = 0;
  x_enqueue(task);
}

/* Scan the wait queue and for each task, depending on whether
 * it's fd-based or timer-based, compute its readiness, and wake up
 * if ready.
 *
 * Returns 1 if at least one task is woken up, 0 otherwise.  */
static int x_collect_ready_waiters(void)
{
  uint64_t now = x_now_ms();
  int woke = 0;
  struct x_task **link = &x_waitq_head;

  while (*link) {
    struct x_task *task = *link;
    int ready = 0;

    switch (task->wait_kind) {
      case X_WAIT_READ:
      case X_WAIT_WRITE:
        ready = x_fd_ready_now(task->wait_fd,
                               x_wait_events_for_kind(task->wait_kind));
        break;
      case X_WAIT_SLEEP:
        ready = now >= task->wait_deadline_ms;
        break;
      default:
        ready = 1;
        break;
    }

    if (ready) {
      x_wake_waiter(&link, task);
      woke = 1;
    } else {
      link = &task->wait_next;
    }
  }

  return woke;
}

/* The mental model:
 *
 * Runnable tasks exist?
 *   run one
 *
 * No runnable tasks, but waiting tasks exist?
 *   ask the kernel: "wake me when one fd is ready or one timer expires"
 *
 * No runnable tasks and no waiting tasks?
 *   event loop is done
 *
 * This function handles the middle case. */
static int x_wait_for_events(void)
{
  /* As long as there are parked tasks, try to wake one or more.
   * If the wait queue becomes empty, there is nothing left to do.
   *
   * Returning 0 tells the scheduler:
   *
   * No runnable tasks.
   * No waiting tasks.
   * The event loop is empty.  */
  while (x_waitq_has_tasks()) {
    /* This scans the wait queue without blocking.
     *
     * It checks:
     *
     * For fd waiters:
     *   is fd already readable/writable?
     *
     * For sleep waiters:
     *   has deadline already passed?
     *
     * This matters because readiness might have happened before this
     * function got called.  */
    if (x_collect_ready_waiters()) {
      return 1;
    }

    /* The function needs two pieces of information before calling poll():
     *
     * How many fds do I need to pass to poll?
     * How long should poll sleep before the next timer expires?  */
    int fd_count = 0;
    uint64_t now = x_now_ms();
    uint64_t min_timeout = UINT64_MAX;

    /* Scan the wait queue.  */
    for (struct x_task *task = x_waitq_head; task; task = task->wait_next) {
      /* For fd-based tasks, it increments the `fd_count`.  */
      if (task->wait_kind == X_WAIT_READ || task->wait_kind == X_WAIT_WRITE) {
        fd_count += 1;
      } else if (task->wait_kind == X_WAIT_SLEEP) {
        /* For timeout-based tasks, it computes the timeout.
         *
         * deadline = absolute wake time
         * timeout = deadline - now
         * min_timeout = smallest timeout among all sleepers  */
        uint64_t timeout =
            task->wait_deadline_ms > now ? task->wait_deadline_ms - now : 0;

        /* Find min timeout.  */
        if (timeout < min_timeout) {
          min_timeout = timeout;
        }
      }
    }

    /* If we found at least one sleeping task, min_timeout ain't gonna be
     * UINT64_MAX.  */
    int timeout_ms = -1;
    if (min_timeout != UINT64_MAX) {
      timeout_ms = x_timeout_to_int_ms(min_timeout);
    }

    /* Fill in the array */
    struct pollfd *pfds = NULL;
    if (fd_count > 0) {
      pfds = calloc((size_t) fd_count, sizeof(*pfds));
      if (!pfds) {
        perror("calloc");
        abort();
      }

      int i = 0;
      for (struct x_task *task = x_waitq_head; task; task = task->wait_next) {
        if (task->wait_kind == X_WAIT_READ || task->wait_kind == X_WAIT_WRITE) {
          pfds[i].fd = task->wait_fd;
          pfds[i].events = x_wait_events_for_kind(task->wait_kind);
          pfds[i].revents = 0;
          i += 1;
        }
      }
    }

    /* Block in the kernel.
     *
     *   rc > 0  -> number of pollfd entries with events
     *   rc = 0  -> timeout expired
     *   rc < 0  -> error  */
    int rc;
    do {
      rc = poll(pfds, (nfds_t) fd_count, timeout_ms);
    } while (rc < 0 && errno == EINTR);

    free(pfds);

    /* After the retry loop, rc < 0 means a real error happened, not just EINTR.
     */
    if (rc < 0) {
      perror("poll");
      abort();
    }

    /* Collect ready tasks again.  */
    if (x_collect_ready_waiters()) {
      return 1;
    }
  }

  return 0;
}

static int x_run_one(void)
{
  struct x_task *task;

  /* Try to pop a task from the run queue.
   * If we got one, continue.
   * If run queue is empty, wait for I/O/timers.
   * If waiting produced no task, scheduler is empty.  */
  for (;;) {
    task = x_dequeue();
    if (task) {
      break;
    }
    if (!x_wait_for_events()) {
      return 0;
    }
  }

  /* Ignore already-done tasks.  */
  if (task->state == X_TASK_DONE) {
    return 1;
  }

  /* Reeset logical tick counter.  */
  task->ticks = 0;
  uint64_t done = task->step((uint64_t) (uintptr_t) task);
  if (done) {
    /* The step function returned nonzero.
     * That means the async function reached its final return.
     * It does not re-enqueue the task. The task is finished.  */
    task->state = X_TASK_DONE;
  } else if (task->state == X_TASK_WAITING) {
    /* The task is suspended on external readiness.
     * Leave it parked.
     *
     * Then return 1 because the scheduler did successfully run one step.  */
    return 1;
  } else {
    /* The step returned 0, but the task is not waiting.
     * That means it suspended voluntarily but can be run again later.  */
    x_enqueue(task);
  }

  /* Scheduler made progress.  */
  return 1;
}

uint64_t __x_task_new(void *step, uint64_t frame_slots)
{
  if (!step) {
    fprintf(stderr, "async runtime: null async step function\n");
    abort();
  }

  struct x_task *task = calloc(1, sizeof(*task));
  if (!task) {
    perror("calloc");
    abort();
  }

  task->step = (x_async_step_fn) step;
  task->frame_slots = frame_slots;
  if (frame_slots > 0) {
    task->frame = calloc((size_t) frame_slots, sizeof(uint64_t));
    if (!task->frame) {
      perror("calloc");
      abort();
    }
  }
  task->state = X_TASK_RUNNABLE;
  task->pc = 0;
  task->wait_kind = X_WAIT_NONE;
  task->wait_fd = -1;

  x_enqueue(task);
  return (uint64_t) (uintptr_t) task;
}

uint64_t __x_task_frame_get(uint64_t handle, uint64_t slot)
{
  struct x_task *task = x_checked_task(handle);
  if (slot >= task->frame_slots) {
    fprintf(stderr, "async runtime: frame get out of bounds: %llu >= %llu\n",
            (unsigned long long) slot, (unsigned long long) task->frame_slots);
    abort();
  }
  return task->frame[slot];
}

void __x_task_frame_set(uint64_t handle, uint64_t slot, uint64_t value)
{
  struct x_task *task = x_checked_task(handle);
  if (slot >= task->frame_slots) {
    fprintf(stderr, "async runtime: frame set out of bounds: %llu >= %llu\n",
            (unsigned long long) slot, (unsigned long long) task->frame_slots);
    abort();
  }
  task->frame[slot] = value;
}

uint64_t __x_task_get_pc(uint64_t handle)
{
  return x_checked_task(handle)->pc;
}

void __x_task_set_pc(uint64_t handle, uint64_t pc)
{
  x_checked_task(handle)->pc = pc;
}

uint64_t __x_task_tick(uint64_t handle)
{
  struct x_task *task = x_checked_task(handle);
  uint64_t budget = x_get_tick_budget();

  if (budget == 0) {
    return 0;
  }

  task->ticks += 1;
  if (task->ticks >= budget) {
    task->ticks = 0;
    return 1;
  }

  return 0;
}

void __x_task_set_result(uint64_t handle, uint64_t result)
{
  x_checked_task(handle)->result = result;
}

uint64_t __x_task_take_result(uint64_t handle)
{
  struct x_task *task = x_checked_task(handle);
  if (task->state != X_TASK_DONE) {
    fprintf(stderr,
            "async runtime: attempted to read unfinished task result\n");
    abort();
  }
  return task->result;
}

uint64_t __x_task_is_done(uint64_t handle)
{
  return x_checked_task(handle)->state == X_TASK_DONE ? 1 : 0;
}

uint64_t __x_task_await(uint64_t handle)
{
  struct x_task *task = x_checked_task(handle);

  while (task->state != X_TASK_DONE) {
    if (!x_run_one()) {
      fprintf(stderr, "async runtime: task cannot make progress\n");
      abort();
    }
  }

  return task->result;
}

void __x_scheduler_run(void)
{
  while (x_run_one()) {
  }
}

void __x_async_bad_pc(uint64_t handle)
{
  struct x_task *task = x_checked_task(handle);
  fprintf(stderr, "async runtime: invalid async pc %llu\n",
          (unsigned long long) task->pc);
  abort();
}
