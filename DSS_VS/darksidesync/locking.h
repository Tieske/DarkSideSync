#ifndef dss_locking_h
#define dss_locking_h

int initLocks ();
void lockSocket ();
void unlockSocket ();
void lockQueue ();
void unlockQueue ();

#endif  /* dss_locking_h */
