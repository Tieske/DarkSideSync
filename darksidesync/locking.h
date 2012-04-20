#ifndef dss_locking_h
#define dss_locking_h

int DSS_mutexInit(DSS_mutex_t m)
void DSS_mutexDestroy(DSS_mutex_t m)
void DSS_mutexLock(DSS_mutex_t m)
void DSS_mutexUnlock(DSS_mutex_t m)

#endif  /* dss_locking_h */
