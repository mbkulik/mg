/*	$OpenBSD$	*/

/* This file is in the public domain. */

/*
 * This file contains the string that gets written
 * out by the emacs-version command.
 */

#include "def.h"

const char	version[] = "Mg 2a";

/*
 * Display the version. All this does
 * is copy the version string onto the echo line.
 */
/* ARGSUSED */
int
showversion(int f, int n)
{
	ewprintf("%s", version);
	return (TRUE);
}
