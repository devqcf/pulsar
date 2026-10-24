// glibc 2.23 compatibility for the Mini (the armhf target is older than the Lite 2's 2.30).
//
// zig's libc++ headers use pthread_cond_clockwait (glibc 2.30) for condition_variable::wait_until
// and the ESPHome key loader uses getrandom (glibc 2.25). Both symbols are absent from the Mini's
// libc; implement them on top of the older interfaces instead of raising the target and requiring
// glibc 2.30 at runtime.
#include <pthread.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

// Only the 32-bit armhf build targets glibc 2.23; aarch64 (glibc 2.36) provides both symbols.
#if defined(__arm__)

extern "C" int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mutex, clockid_t clockid,
                                      const struct timespec *abstime) {
  struct timespec rt;
  if (clockid == CLOCK_MONOTONIC) {
    // pthread_cond_timedwait only takes CLOCK_REALTIME: shift the monotonic deadline by the
    // current offset between the two clocks.
    struct timespec mono, real;
    if (clock_gettime(CLOCK_MONOTONIC, &mono) < 0 || clock_gettime(CLOCK_REALTIME, &real) < 0) return errno;
    int64_t sec = int64_t(abstime->tv_sec) - mono.tv_sec;
    int64_t nsec = int64_t(abstime->tv_nsec) - mono.tv_nsec;
    if (nsec < 0) {
      sec--;
      nsec += 1000000000;
    }
    real.tv_sec += sec;
    real.tv_nsec += nsec;
    if (real.tv_nsec >= 1000000000) {
      real.tv_sec++;
      real.tv_nsec -= 1000000000;
    }
    rt = real;
  } else {
    rt = *abstime;
  }
  return pthread_cond_timedwait(cond, mutex, &rt);
}

extern "C" ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
  return syscall(SYS_getrandom, buf, buflen, flags);
}

#endif  // __arm__
