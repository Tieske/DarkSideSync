#ifndef dss_locking_h
#define dss_locking_h

int initLocks ();
void lockSocket ();
void unlockSocket ();
void lockQueue ();
void unlockQueue ();
void lockUtilList ();	// NOTE: queue and utillist share the same mutex!
void unlockUtilList (); // so beware of deadlocks

#endif  /* dss_locking_h */
