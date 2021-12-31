/*
 *  Written by Joel Sherrill <joel.sherrill@OARcorp.com>.
 *
 *  COPYRIGHT (c) 1989-2013, 2015.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  Permission to use, copy, modify, and distribute this software for any
 *  purpose without fee is hereby granted, provided that this entire notice
 *  is included in all copies of any software which is or includes a copy
 *  or modification of this software.
 *
 *  THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 *  WARRANTY.  IN PARTICULAR,  THE AUTHOR MAKES NO REPRESENTATION
 *  OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
 *  SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#ifndef __PTHREAD_h
#define __PTHREAD_h

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>

#include <sys/types.h>
#include <time.h>
#include <sched.h>
#include <sys/cdefs.h>

/* Features that we support which need to be defined */
#ifndef _POSIX_THREAD_CPUTIME
#define _POSIX_THREAD_CPUTIME 1
#endif
#ifndef _POSIX_SPIN_LOCKS
#define _POSIX_SPIN_LOCKS
#endif

struct _pthread_cleanup_context {
  void (*_routine)(void *);
  void *_arg;
  int _canceltype;
  struct _pthread_cleanup_context *_previous;
};

/* Register Fork Handlers */
int	pthread_atfork (void (*prepare)(void), void (*parent)(void),
                   void (*child)(void));
          
/* Mutex Initialization Attributes, P1003.1c/Draft 10, p. 81 */

int	pthread_mutexattr_init (pthread_mutexattr_t *__attr);
int	pthread_mutexattr_destroy (pthread_mutexattr_t *__attr);
int	pthread_mutexattr_getpshared (const pthread_mutexattr_t *__attr,
				      int  *__pshared);
int	pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr,
				      int __pshared);

/* Single UNIX Specification 2 Mutex Attributes types */

int pthread_mutexattr_gettype (const pthread_mutexattr_t *__attr, int *__kind);
int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind);

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

/* Initializing and Destroying a Mutex, P1003.1c/Draft 10, p. 87 */

int	pthread_mutex_init (pthread_mutex_t *__mutex,
			    const pthread_mutexattr_t *__attr);
int	pthread_mutex_destroy (pthread_mutex_t *__mutex);

/* This is used to statically initialize a pthread_mutex_t. Example:
  
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
 */

#define PTHREAD_MUTEX_INITIALIZER _PTHREAD_MUTEX_INITIALIZER

/*  Locking and Unlocking a Mutex, P1003.1c/Draft 10, p. 93
    NOTE: P1003.4b/D8 adds pthread_mutex_timedlock(), p. 29 */

int	pthread_mutex_lock (pthread_mutex_t *__mutex);
int	pthread_mutex_trylock (pthread_mutex_t *__mutex);
int	pthread_mutex_unlock (pthread_mutex_t *__mutex);

/* Condition Variable Initialization Attributes, P1003.1c/Draft 10, p. 96 */
 
int	pthread_condattr_init (pthread_condattr_t *__attr);
int	pthread_condattr_destroy (pthread_condattr_t *__attr);

int	pthread_condattr_getclock (const pthread_condattr_t *__restrict __attr,
				   clockid_t *__restrict __clock_id);
int	pthread_condattr_setclock (pthread_condattr_t *__attr,
				   clockid_t __clock_id);

int	pthread_condattr_getpshared (const pthread_condattr_t *__attr,
				     int *__pshared);
int	pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared);
 
/* Initializing and Destroying a Condition Variable, P1003.1c/Draft 10, p. 87 */
 
int	pthread_cond_init (pthread_cond_t *__cond,
			   const pthread_condattr_t *__attr);
int	pthread_cond_destroy (pthread_cond_t *__mutex);
 
/* This is used to statically initialize a pthread_cond_t. Example:
  
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
 */
 
#define PTHREAD_COND_INITIALIZER _PTHREAD_COND_INITIALIZER
 
/* Broadcasting and Signaling a Condition, P1003.1c/Draft 10, p. 101 */
 
int	pthread_cond_signal (pthread_cond_t *__cond);
int	pthread_cond_broadcast (pthread_cond_t *__cond);
 
/* Waiting on a Condition, P1003.1c/Draft 10, p. 105 */
 
int	pthread_cond_wait (pthread_cond_t *__cond, pthread_mutex_t *__mutex);
 
int	pthread_cond_timedwait (pthread_cond_t *__cond,
				pthread_mutex_t *__mutex,
				const struct timespec *__abstime);
 
int	pthread_attr_setschedparam (pthread_attr_t *__attr,
				    const struct sched_param *__param);
int	pthread_attr_getschedparam (const pthread_attr_t *__attr,
				    struct sched_param *__param);

int	pthread_getname_np(pthread_t, char *, size_t) __nonnull((2));

int	pthread_setname_np(pthread_t, const char *) __nonnull((2));

/* Thread Creation Attributes, P1003.1c/Draft 10, p, 140 */

int	pthread_attr_init (pthread_attr_t *__attr);
int	pthread_attr_destroy (pthread_attr_t *__attr);
int	pthread_attr_setstack (pthread_attr_t *attr,
	void *__stackaddr, size_t __stacksize);
int	pthread_attr_getstack (const pthread_attr_t *attr,
	void **__stackaddr, size_t *__stacksize);
int	pthread_attr_getstacksize (const pthread_attr_t *__attr,
				   size_t *__stacksize);
int	pthread_attr_setstacksize (pthread_attr_t *__attr, size_t __stacksize);
int	pthread_attr_getstackaddr (const pthread_attr_t *__attr,
				   void **__stackaddr);
int	pthread_attr_setstackaddr (pthread_attr_t  *__attr, void *__stackaddr);
int	pthread_attr_getdetachstate (const pthread_attr_t *__attr,
				     int *__detachstate);
int	pthread_attr_setdetachstate (pthread_attr_t *__attr, int __detachstate);
int	pthread_attr_getguardsize (const pthread_attr_t *__attr,
				   size_t *__guardsize);
int	pthread_attr_setguardsize (pthread_attr_t *__attr, size_t __guardsize);

/* Thread Creation, P1003.1c/Draft 10, p. 144 */

int	pthread_create (pthread_t *__pthread, const pthread_attr_t  *__attr,
			void *(*__start_routine)(void *), void *__arg);

/* Wait for Thread Termination, P1003.1c/Draft 10, p. 147 */

int	pthread_join (pthread_t __pthread, void **__value_ptr);

/* Detaching a Thread, P1003.1c/Draft 10, p. 149 */

int	pthread_detach (pthread_t __pthread);

/* Thread Termination, p1003.1c/Draft 10, p. 150 */

void	pthread_exit (void *__value_ptr) __dead2;

/* Get Calling Thread's ID, p1003.1c/Draft 10, p. XXX */

pthread_t	pthread_self (void);

/* Compare Thread IDs, p1003.1c/Draft 10, p. 153 */

int	pthread_equal (pthread_t __t1, pthread_t __t2);

/* Retrieve ID of a Thread's CPU Time Clock */
int	pthread_getcpuclockid (pthread_t thread, clockid_t *clock_id);

/* Get/Set Current Thread's Concurrency Level */
int	pthread_setconcurrency (int new_level);
int	pthread_getconcurrency (void);

void	pthread_yield (void);

/* Dynamic Package Initialization */

/* This is used to statically initialize a pthread_once_t. Example:
  
    pthread_once_t once = PTHREAD_ONCE_INIT;
  
    NOTE:  This is named inconsistently -- it should be INITIALIZER.  */
 
#define PTHREAD_ONCE_INIT _PTHREAD_ONCE_INIT
 
int	pthread_once (pthread_once_t *__once_control,
		      void (*__init_routine)(void));

/* Thread-Specific Data Key Create, P1003.1c/Draft 10, p. 163 */

int	pthread_key_create (pthread_key_t *__key,
			    void (*__destructor)(void *));

/* Thread-Specific Data Management, P1003.1c/Draft 10, p. 165 */

int	pthread_setspecific (pthread_key_t __key, const void *__value);
void *	pthread_getspecific (pthread_key_t __key);

/* Thread-Specific Data Key Deletion, P1003.1c/Draft 10, p. 167 */

int	pthread_key_delete (pthread_key_t __key);

/* Execution of a Thread, P1003.1c/Draft 10, p. 181 */

#define PTHREAD_CANCEL_ENABLE  0
#define PTHREAD_CANCEL_DISABLE 1

#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1

#define PTHREAD_CANCELED ((void *) -1)

int	pthread_cancel (pthread_t __pthread);

/* Setting Cancelability State, P1003.1c/Draft 10, p. 183 */

int	pthread_setcancelstate (int __state, int *__oldstate);
int	pthread_setcanceltype (int __type, int *__oldtype);
void 	pthread_testcancel (void);

/* Establishing Cancellation Handlers, P1003.1c/Draft 10, p. 184 */

void	_pthread_cleanup_push (struct _pthread_cleanup_context *_context,
			       void (*_routine)(void *), void *_arg);

void	_pthread_cleanup_pop (struct _pthread_cleanup_context *_context,
			      int _execute);

/* It is intentional to open and close the scope in two different macros */
#define pthread_cleanup_push(_routine, _arg) \
  do { \
    struct _pthread_cleanup_context _pthread_clup_ctx; \
    _pthread_cleanup_push(&_pthread_clup_ctx, (_routine), (_arg))

#define pthread_cleanup_pop(_execute) \
    _pthread_cleanup_pop(&_pthread_clup_ctx, (_execute)); \
  } while (0)

void	_pthread_cleanup_push_defer (struct _pthread_cleanup_context *_context,
				     void (*_routine)(void *), void *_arg);

void	_pthread_cleanup_pop_restore (struct _pthread_cleanup_context *_context,
				      int _execute);

/* It is intentional to open and close the scope in two different macros */
#define pthread_cleanup_push_defer_np(_routine, _arg) \
  do { \
    struct _pthread_cleanup_context _pthread_clup_ctx; \
    _pthread_cleanup_push_defer(&_pthread_clup_ctx, (_routine), (_arg))

#define pthread_cleanup_pop_restore_np(_execute) \
    _pthread_cleanup_pop_restore(&_pthread_clup_ctx, (_execute)); \
  } while (0)

/* Accessing a Thread CPU-time Clock, P1003.4b/D8, p. 58 */
 
int	pthread_getcpuclockid (pthread_t __pthread_id, clockid_t *__clock_id);

int	pthread_spin_init (pthread_spinlock_t *__spinlock, int __pshared);
int	pthread_spin_destroy (pthread_spinlock_t *__spinlock);
int	pthread_spin_lock (pthread_spinlock_t *__spinlock);
int	pthread_spin_trylock (pthread_spinlock_t *__spinlock);
int	pthread_spin_unlock (pthread_spinlock_t *__spinlock);

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
