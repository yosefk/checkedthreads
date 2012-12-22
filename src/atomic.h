#ifndef CT_ATOMIC_H_
#define CT_ATOMIC_H_

/* TODO: support other intrinsics in an #ifdef if necessary. */

/* these give the old value and increment in-place by the given amount.
   if others call them, then everybody gets a unique value - one of the
   intermediate results - though it's unspecified who gets what. */
#define ATOMIC_FETCH_THEN_INCR(ptr,incr) __sync_fetch_and_add(ptr,incr)
#define ATOMIC_FETCH_THEN_DERC(ptr,decr) __sync_fetch_and_sub(ptr,decr)

#endif
