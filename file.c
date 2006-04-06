/*	$OpenBSD$	*/

/* This file is in the public domain. */

/*
 *	File commands.
 */

#include "def.h"

#include <libgen.h>

/*
 * Insert a file into the current buffer.  Real easy - just call the
 * insertfile routine with the file name.
 */
/* ARGSUSED */
int
fileinsert(int f, int n)
{
	char	 fname[NFILEN], *bufp, *adjf;

	bufp = eread("Insert file: ", fname, NFILEN, EFNEW | EFCR | EFFILE);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(bufp);
	if (adjf == NULL)
		return (FALSE);
	return (insertfile(adjf, NULL, FALSE));
}

/*
 * Select a file for editing.  Look around to see if you can find the file
 * in another buffer; if you can find it, just switch to the buffer.  If
 * you cannot find the file, create a new buffer, read in the text, and
 * switch to the new buffer.
 */
/* ARGSUSED */
int
filevisit(int f, int n)
{
	struct buffer	*bp;
	char	 fname[NFILEN], *bufp, *adjf, *slash;
	int	 status;

	if (curbp->b_fname && curbp->b_fname[0] != '\0') {
		if (strlcpy(fname, curbp->b_fname, sizeof(fname)) >= sizeof(fname))
			return (FALSE);
		if ((slash = strrchr(fname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	}
	else
		fname[0] = '\0';

	bufp = eread("Find file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(fname);
	if (adjf == NULL)
		return (FALSE);
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFHARD) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

/*
 * Replace the current file with an alternate one. Semantics for finding
 * the replacement file are the same as 'filevisit', except the current
 * buffer is killed before the switch. If the kill fails, or is aborted,
 * revert to the original file.
 */
/* ARGSUSED */
int
filevisitalt(int f, int n)
{
	struct buffer	*bp;
	char	 fname[NFILEN], *bufp, *adjf, *slash;
	int	 status;

	if (curbp->b_fname && curbp->b_fname[0] != '\0') {
		if (strlcpy(fname, curbp->b_fname, sizeof(fname)) >= sizeof(fname))
			return (FALSE);
		if ((slash = strrchr(fname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else
		fname[0] = '\0';

	bufp = eread("Find alternate file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	status = killbuffer(curbp);
	if (status == ABORT || status == FALSE)
		return (ABORT);

	adjf = adjustname(fname);
	if (adjf == NULL)
		return (FALSE);
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFHARD) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

int
filevisitro(int f, int n)
{
	int error;

	error = filevisit(f, n);
	if (error != TRUE)
		return (error);
	curbp->b_flag |= BFREADONLY;
	return (TRUE);
}

/*
 * Pop to a file in the other window.  Same as the last function, but uses
 * popbuf instead of showbuffer.
 */
/* ARGSUSED */
int
poptofile(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 fname[NFILEN], *adjf, *bufp;
	int	 status;

	if ((bufp = eread("Find file in other window: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	adjf = adjustname(fname);
	if (adjf == NULL)
		return (FALSE);
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == '\0') {
		if ((status = readin(adjf)) != TRUE)
			killbuffer(bp);
		return (status);
	}
	return (TRUE);
}

/*
 * Given a file name, either find the buffer it uses, or create a new
 * empty buffer to put it in.
 */
struct buffer *
findbuffer(char *fn)
{
	struct buffer	*bp;
	char		bname[NBUFN], fname[NBUFN];

	if (strlcpy(fname, fn, sizeof(fname)) >= sizeof(fname)) {
		ewprintf("filename too long");
		return (NULL);
	}

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (strcmp(bp->b_fname, fname) == 0)
			return (bp);
	}
	/* Not found. Create a new one, adjusting name first */
	if (baugname(bname, fname, sizeof(bname)) == FALSE)
		return (NULL);

	bp = bfind(bname, TRUE);
	return (bp);
}

/*
 * Read the file "fname" into the current buffer.  Make all of the text
 * in the buffer go away, after checking for unsaved changes.  This is
 * called by the "read" command, the "visit" command, and the mainline
 * (for "mg file").
 */
int
readin(char *fname)
{
	struct mgwin	*wp;
	int	 status, i, ro = FALSE;
	PF	*ael;

	/* might be old */
	if (bclear(curbp) != TRUE)
		return (TRUE);
	if ((status = insertfile(fname, fname, TRUE)) != TRUE) {
		ewprintf("File is not readable: %s", fname);
		return (FALSE);
	}

	/*
	 * Call auto-executing function if we need to.
	 */
	if ((ael = find_autoexec(fname)) != NULL) {
		for (i = 0; ael[i] != NULL; i++)
			(*ael[i])(0, 1);
		free(ael);
	}

	/* no change */
	curbp->b_flag &= ~BFCHG;
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_dotp = wp->w_linep = lforw(curbp->b_linep);
			wp->w_doto = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
		}
	}

	/*
	 * We need to set the READONLY flag after we insert the file,
	 * unless the file is a directory.
	 */
	if (access(fname, W_OK) && errno != ENOENT)
		ro = TRUE;
	if (fisdir(fname) == TRUE)
		ro = TRUE;
	if (ro == TRUE)
		curbp->b_flag |= BFREADONLY;
	else
		curbp->b_flag &=~ BFREADONLY;

	if (startrow)
		gotoline(FFARG, startrow);

	return (status);
}

/*
 * NB, getting file attributes is done here under control of a flag
 * rather than in readin, which would be cleaner.  I was concerned
 * that some operating system might require the file to be open
 * in order to get the information.  Similarly for writing.
 */

/*
 * Insert a file in the current buffer, after dot. If file is a directory,
 * and 'replacebuf' is TRUE, invoke dired mode, else die with an error.
 * If file is a regular file, set mark at the end of the text inserted;
 * point at the beginning.  Return a standard status. Print a summary
 * (lines read, error message) out as well. This routine also does the
 * read end of backup processing.  The BFBAK flag, if set in a buffer,
 * says that a backup should be taken.  It is set when a file is read in,
 * but not on a new file. You don't need to make a backup copy of nothing.
 */

static char	*line = NULL;
static int	linesize = 0;

int
insertfile(char *fname, char *newname, int replacebuf)
{
	struct buffer	*bp;
	struct line	*lp1, *lp2;
	struct line	*olp;			/* line we started at */
	struct mgwin	*wp;
	int	 nbytes, s, nline, siz, x = -1, x2;
	int	 opos;			/* offset we started at */

	if (replacebuf == TRUE)
		x = undo_enable(FALSE);

	lp1 = NULL;
	if (line == NULL) {
		line = malloc(NLINE);
		if (line == NULL)
			panic("out of memory");
		linesize = NLINE;
	}

	/* cheap */
	bp = curbp;
	if (newname != NULL)
		(void)strlcpy(bp->b_fname, newname, sizeof bp->b_fname);

	/* hard file open */
	if ((s = ffropen(fname, (replacebuf == TRUE) ? bp : NULL)) == FIOERR)
		goto out;
	if (s == FIOFNF) {
		/* file not found */
		if (newname != NULL)
			ewprintf("(New file)");
		else
			ewprintf("(File not found)");
		goto out;
	} else if (s == FIODIR) {
		/* file was a directory */
		if (replacebuf == FALSE) {
			ewprintf("Cannot insert: file is a directory, %s",
			    fname);
			goto cleanup;
		}
		killbuffer(bp);
		if ((bp = dired_(fname)) == NULL)
			return (FALSE);
		undo_enable(x);
		curbp = bp;
		return (showbuffer(bp, curwp, WFHARD | WFMODE));
	}
	opos = curwp->w_doto;

	/* open a new line, at point, and start inserting after it */
	x2 = undo_enable(FALSE);
	(void)lnewline();
	olp = lback(curwp->w_dotp);
	if (olp == curbp->b_linep) {
		/* if at end of buffer, create a line to insert before */
		(void)lnewline();
		curwp->w_dotp = lback(curwp->w_dotp);
	}
	undo_enable(x2);

	/* don't count fake lines at the end */
	nline = 0;
	siz = 0;
	while ((s = ffgetline(line, linesize, &nbytes)) != FIOERR) {
doneread:
		siz += nbytes + 1;
		switch (s) {
		case FIOSUC:
			++nline;
			/* FALLTHRU */
		case FIOEOF:
			/* the last line of the file */
			if ((lp1 = lalloc(nbytes)) == NULL) {
				/* keep message on the display */
				s = FIOERR;
				goto endoffile;
			}
			bcopy(line, &ltext(lp1)[0], nbytes);
			lp2 = lback(curwp->w_dotp);
			lp2->l_fp = lp1;
			lp1->l_fp = curwp->w_dotp;
			lp1->l_bp = lp2;
			curwp->w_dotp->l_bp = lp1;
			if (s == FIOEOF)
				goto endoffile;
			break;
		case FIOLONG: {
				/* a line too long to fit in our buffer */
				char	*cp;
				int	newsize;

				newsize = linesize * 2;
				if (newsize < 0 ||
				    (cp = malloc(newsize)) == NULL) {
					ewprintf("Could not allocate %d bytes",
					    newsize);
						s = FIOERR;
						goto endoffile;
				}
				bcopy(line, cp, linesize);
				free(line);
				line = cp;
				s = ffgetline(line + linesize, linesize,
				    &nbytes);
				nbytes += linesize;
				linesize = newsize;
				if (s == FIOERR)
					goto endoffile;
				goto doneread;
			}
		default:
			ewprintf("Unknown code %d reading file", s);
			s = FIOERR;
			break;
		}
	}
endoffile:
	undo_add_insert(olp, opos, siz-1);

	/* ignore errors */
	ffclose(NULL);
	/* don't zap an error */
	if (s == FIOEOF) {
		if (nline == 1)
			ewprintf("(Read 1 line)");
		else
			ewprintf("(Read %d lines)", nline);
	}
	/* set mark at the end of the text */
	curwp->w_dotp = curwp->w_markp = lback(curwp->w_dotp);
	curwp->w_marko = llength(curwp->w_markp);
	(void)ldelnewline();
	curwp->w_dotp = olp;
	curwp->w_doto = opos;
	if (olp == curbp->b_linep)
		curwp->w_dotp = lforw(olp);
	if (newname != NULL)
		bp->b_flag |= BFCHG | BFBAK;	/* Need a backup.	 */
	else
		bp->b_flag |= BFCHG;
	/*
	 * If the insert was at the end of buffer, set lp1 to the end of
	 * buffer line, and lp2 to the beginning of the newly inserted text.
	 * (Otherwise lp2 is set to NULL.)  This is used below to set
	 * pointers in other windows correctly if they are also at the end of
	 * buffer.
	 */
	lp1 = bp->b_linep;
	if (curwp->w_markp == lp1) {
		lp2 = curwp->w_dotp;
	} else {
		/* delete extraneous newline */
		(void)ldelnewline();
out:		lp2 = NULL;
	}
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == curbp) {
			wp->w_flag |= WFMODE | WFEDIT;
			if (wp != curwp && lp2 != NULL) {
				if (wp->w_dotp == lp1)
					wp->w_dotp = lp2;
				if (wp->w_markp == lp1)
					wp->w_markp = lp2;
				if (wp->w_linep == lp1)
					wp->w_linep = lp2;
			}
		}
	}
cleanup:
	undo_enable(x);

	/* return FALSE if error */
	return (s != FIOERR);
}

/*
 * Ask for a file name and write the contents of the current buffer to that
 * file.  Update the remembered file name and clear the buffer changed flag.
 * This handling of file names is different from the earlier versions and
 * is more compatible with Gosling EMACS than with ITS EMACS.
 */
/* ARGSUSED */
int
filewrite(int f, int n)
{
	int	 s;
	char	 fname[NFILEN], bn[NBUFN];
	char	*adjfname, *bufp;

	if ((bufp = eread("Write file: ", fname, NFILEN,
	    EFNEW | EFCR | EFFILE)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	adjfname = adjustname(fname);
	if (adjfname == NULL)
		return (FALSE);
	/* old attributes are no longer current */
	bzero(&curbp->b_fi, sizeof(curbp->b_fi));
	if ((s = writeout(curbp, adjfname)) == TRUE) {
		(void)strlcpy(curbp->b_fname, adjfname, sizeof(curbp->b_fname));
		free(curbp->b_bname);
		if (baugname(bn, basename(curbp->b_fname), sizeof(bn))
		    == FALSE)
			return (FALSE);
		if ((curbp->b_bname = strdup(bn)) == NULL)
			return (FALSE);
		curbp->b_flag &= ~(BFBAK | BFCHG);
		upmodes(curbp);
	}
	return (s);
}

/*
 * Save the contents of the current buffer back into its associated file.
 */
#ifndef	MAKEBACKUP
#define	MAKEBACKUP TRUE
#endif /* !MAKEBACKUP */
static int	makebackup = MAKEBACKUP;

/* ARGSUSED */
int
filesave(int f, int n)
{
	return (buffsave(curbp));
}

/*
 * Save the contents of the buffer argument into its associated file.  Do
 * nothing if there have been no changes (is this a bug, or a feature?).
 * Error if there is no remembered file name. If this is the first write
 * since the read or visit, then a backup copy of the file is made.
 * Allow user to select whether or not to make backup files by looking at
 * the value of makebackup.
 */
int
buffsave(struct buffer *bp)
{
	int	 s;

	/* return, no changes */
	if ((bp->b_flag & BFCHG) == 0) {
		ewprintf("(No changes need to be saved)");
		return (TRUE);
	}

	/* must have a name */
	if (bp->b_fname[0] == '\0') {
		ewprintf("No file name");
		return (FALSE);
	}

	if (makebackup && (bp->b_flag & BFBAK)) {
		s = fbackupfile(bp->b_fname);
		/* hard error */
		if (s == ABORT)
			return (FALSE);
		/* softer error */
		if (s == FALSE &&
		    (s = eyesno("Backup error, save anyway")) != TRUE)
			return (s);
	}
	if ((s = writeout(bp, bp->b_fname)) == TRUE) {
		bp->b_flag &= ~(BFCHG | BFBAK);
		upmodes(bp);
	}
	return (s);
}

/*
 * Since we don't have variables (we probably should) this is a command
 * processor for changing the value of the make backup flag.  If no argument
 * is given, sets makebackup to true, so backups are made.  If an argument is
 * given, no backup files are made when saving a new version of a file. Only
 * used when BACKUP is #defined.
 */
/* ARGSUSED */
int
makebkfile(int f, int n)
{
	if (f & FFARG)
		makebackup = n > 0;
	else
		makebackup = !makebackup;
	ewprintf("Backup files %sabled", makebackup ? "en" : "dis");
	return (TRUE);
}

/*
 * NB: bp is passed to both ffwopen and ffclose because some
 * attribute information may need to be updated at open time
 * and others after the close.  This is OS-dependent.  Note
 * that the ff routines are assumed to be able to tell whether
 * the attribute information has been set up in this buffer
 * or not.
 */

/*
 * This function performs the details of file writing; writing the file
 * in buffer bp to file fn. Uses the file management routines in the
 * "fileio.c" package. Most of the grief is checking of some sort.
 */
int
writeout(struct buffer *bp, char *fn)
{
	int	 s;

	/* open writes message */
	if ((s = ffwopen(fn, bp)) != FIOSUC)
		return (FALSE);
	s = ffputbuf(bp);
	if (s == FIOSUC) {
		/* no write error */
		s = ffclose(bp);
		if (s == FIOSUC)
			ewprintf("Wrote %s", fn);
	} else
		/* ignore close error if it is a write error */
		(void)ffclose(bp);
	return (s == FIOSUC);
}

/*
 * Tag all windows for bp (all windows if bp == NULL) as needing their
 * mode line updated.
 */
void
upmodes(struct buffer *bp)
{
	struct mgwin	*wp;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (bp == NULL || curwp->w_bufp == bp)
			wp->w_flag |= WFMODE;
}
