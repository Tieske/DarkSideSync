#ifndef dss_waithandle_c
#define dss_waithandle_c

#include <lua.h>
#include <lauxlib.h>
#include "waithandle.h"

/*
** ===============================================================
**  Create a new waithandle
** ===============================================================
*/
// Returns NULL upon failure
// initial state of the handle returned is 'signalled'
pDSS_waithandle DSS_waithandle_create()
{
	pDSS_waithandle wh;

	// TODO: implement

	DSS_waithandle_signal(wh);
	return wh;
}

/*
** ===============================================================
**  Resets the waithandle (closes the gate)
** ===============================================================
*/
void DSS_waithandle_reset(pDSS_waithandle wh)
{
	// TODO: implement
}

/*
** ===============================================================
**  Signals the waithandle (opens the gate)
** ===============================================================
*/
void DSS_waithandle_signal(pDSS_waithandle wh)
{
	// TODO: implement
}

/*
** ===============================================================
**  Waits for the waithandle to be signalled (tries passing the gate)
** ===============================================================
*/
void DSS_waithandle_wait(pDSS_waithandle wh)
{
	// TODO: implement
}

/*
** ===============================================================
**  Destroys the waithandle
** ===============================================================
*/
void DSS_waithandle_delete(pDSS_waithandle wh)
{
	// TODO: implement
}

#endif
