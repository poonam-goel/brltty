/*
 * BRLTTY - A background process providing access to the Linux console (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2001 by The BRLTTY Team. All rights reserved.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

/* CombiBraille/brl.c - Braille display library
 * For Tieman B.V.'s CombiBraille (serial interface only)
 * Was maintained by Nikhil Nair <nn201@cus.cam.ac.uk>
 * $Id: brl.c,v 1.3 1996/09/24 01:04:29 nn201 Exp $
 */

#define __EXTENSIONS__  /* for termios.h */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/termios.h>
#include <string.h>

#include "../brl.h"
#include "../misc.h"
#include "brlconf.h"
#include "../brl_driver.h"

/* Command translation table: */
static unsigned char cmdtrans[256] = {
   #include "cmdtrans.h"		/* for keybindings */
};

static unsigned char combitrans[256];	/* dot mapping table (output) */
int brl_fd;			/* file descriptor for Braille display */
static unsigned char *prevdata;	/* previously received data */
static unsigned char status[5], oldstatus[5];	/* status cells - always five */
unsigned char *rawdata;		/* writebrl() buffer for raw Braille data */
static short rawlen;			/* length of rawdata buffer */
static struct termios oldtio;		/* old terminal settings */

/* Function prototypes: */
static int getbrlkey (void);		/* get a keystroke from the CombiBraille */


static void
identbrl (void)
{
  LogAndStderr(LOG_NOTICE, "Tieman B.V. CombiBraille driver");
  LogAndStderr(LOG_INFO, "   Copyright (C) 1995, 1996 by Nikhil Nair.");
}


static void initbrl (char **parameters, brldim *brl, const char *brldev)
{
  brldim res;			/* return result */
  struct termios newtio;	/* new terminal settings */
  short i, n, success;		/* loop counters, flags, etc. */
  unsigned char *init_seq = INIT_SEQ;	/* bytewise accessible copies */
  unsigned char *init_ack = INIT_ACK;
  unsigned char c;
  char id = -1;
  unsigned char standard[8] =
  {0, 1, 2, 3, 4, 5, 6, 7};	/* BRLTTY standard mapping */
  unsigned char Tieman[8] =
  {0, 7, 1, 6, 2, 5, 3, 4};	/* Tieman standard */

  res.disp = prevdata = rawdata = NULL;		/* clear pointers */

  /* No need to load translation tables, as these are now
   * defined in tables.h
   */

  /* Now open the Braille display device for random access */
  brl_fd = open (brldev, O_RDWR | O_NOCTTY);
  if (brl_fd < 0)
    goto failure;
  tcgetattr (brl_fd, &oldtio);	/* save current settings */

  /* Set bps, flow control and 8n1, enable reading */
  newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;

  /* Ignore bytes with parity errors and make terminal raw and dumb */
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;		/* raw output */
  newtio.c_lflag = 0;		/* don't echo or generate signals */
  newtio.c_cc[VMIN] = 0;	/* set nonblocking read */
  newtio.c_cc[VTIME] = 0;
  tcflush (brl_fd, TCIFLUSH);	/* clean line */
  tcsetattr (brl_fd, TCSANOW, &newtio);		/* activate new settings */

  /* CombiBraille initialisation procedure: */
  success = 0;
  /* Try MAX_ATTEMPTS times, or forever if MAX_ATTEMPTS is 0: */
#if MAX_ATTEMPTS == 0
  while (!success)
#else
  for (i = 0; i < MAX_ATTEMPTS && !success; i++)
#endif
    {
      if (init_seq[0])
	if (write (brl_fd, init_seq + 1, init_seq[0]) != init_seq[0])
	  continue;
      timeout_yet (0);		/* initialise timeout testing */
      n = 0;
      do
	{
	  delay (20);
	  if (read (brl_fd, &c, 1) == 0)
	    continue;
	  if (n < init_ack[0] && c != init_ack[1 + n])
	    continue;
	  if (n == init_ack[0])
	    id = c, success = 1;
	  n++;
	}
      while (!timeout_yet (ACK_TIMEOUT) && n <= init_ack[0]);
    }
  if (!success)
    {
      tcsetattr (brl_fd, TCSANOW, &oldtio);
      goto failure;
    }

  res.x = BRLCOLS (id);		/* initialise size of display */
  res.y = BRLROWS;
  if ((brl->x = res.x) == -1)
    return;

  /* Allocate space for buffers */
  res.disp = (unsigned char *) malloc (res.x * res.y);
  prevdata = (unsigned char *) malloc (res.x * res.y);
  /* rawdata has to have room for the pre- and post-data sequences,
   * the status cells, and escaped 0x1b's: */
  rawdata = (unsigned char *) malloc (20 + res.x * res.y * 2);
  if (!res.disp || !prevdata || !rawdata)
    goto failure;

  /* Generate dot mapping table: */
  memset (combitrans, 0, 256);
  for (n = 0; n < 256; n++)
    for (i = 0; i < 8; i++)
      if (n & 1 << standard[i])
	combitrans[n] |= 1 << Tieman[i];
  *brl = res;
  return;

failure:;
  if (res.disp)
    free (res.disp);
  if (prevdata)
    free (prevdata);
  if (rawdata)
    free (rawdata);
  if (brl_fd >= 0)
    close (brl_fd);
  brl->x = -1;
  return;
}


static void
closebrl (brldim *brl)
{
  unsigned char *pre_data = PRE_DATA;	/* bytewise accessible copies */
  unsigned char *post_data = POST_DATA;
  unsigned char *close_seq = CLOSE_SEQ;

  rawlen = 0;
  if (pre_data[0])
    {
      memcpy (rawdata + rawlen, pre_data + 1, pre_data[0]);
      rawlen += pre_data[0];
    }
  /* Clear the five status cells and the main display: */
  memset (rawdata + rawlen, 0, 5 + brl->x * brl->y);
  rawlen += 5 + brl->x * brl->y;
  if (post_data[0])
    {
      memcpy (rawdata + rawlen, post_data + 1, post_data[0]);
      rawlen += post_data[0];
    }

  /* Send closing sequence: */
  if (close_seq[0])
    {
      memcpy (rawdata + rawlen, close_seq + 1, close_seq[0]);
      rawlen += close_seq[0];
    }
  write (brl_fd, rawdata, rawlen);

  free (brl->disp);
  free (prevdata);
  free (rawdata);

#if 0
  tcsetattr (brl_fd, TCSANOW, &oldtio);		/* restore terminal settings */
#endif
  close (brl_fd);
}


static void
setbrlstat (const unsigned char *s)
{
  short i;

  /* Dot mapping: */
  for (i = 0; i < 5; status[i] = combitrans[s[i]], i++);
}


static void
writebrl (brldim *brl)
{
  short i;			/* loop counter */
  unsigned char *pre_data = PRE_DATA;	/* bytewise accessible copies */
  unsigned char *post_data = POST_DATA;

  /* Only refresh display if the data has changed: */
  if (memcmp (brl->disp, prevdata, brl->x * brl->y) || \
      memcmp (status, oldstatus, 5))
    {
      /* Save new Braille data: */
      memcpy (prevdata, brl->disp, brl->x * brl->y);

      /* Dot mapping from standard to CombiBraille: */
      for (i = 0; i < brl->x * brl->y; brl->disp[i] = combitrans[brl->disp[i]], \
	   i++);

      rawlen = 0;
      if (pre_data[0])
	{
	  memcpy (rawdata + rawlen, pre_data + 1, pre_data[0]);
	  rawlen += pre_data[0];
	}
      for (i = 0; i < 5; i++)
	{
	  rawdata[rawlen++] = status[i];
	  if (status[i] == 0x1b)	/* CombiBraille hack */
	    rawdata[rawlen++] = 0x1b;
	}
      for (i = 0; i < brl->x * brl->y; i++)
	{
	  rawdata[rawlen++] = brl->disp[i];
	  if (brl->disp[i] == 0x1b)	/* CombiBraille hack */
	    rawdata[rawlen++] = brl->disp[i];
	}
      if (post_data[0])
	{
	  memcpy (rawdata + rawlen, post_data + 1, post_data[0]);
	  rawlen += post_data[0];
	}
      write (brl_fd, rawdata, rawlen);
    }
}


static int
readbrl (DriverCommandContext cmds)
{
  int c;
  static short status = 0;	/* cursor routing keys mode */

  c = getbrlkey ();
  if (c == EOF)
    return EOF;
  c = cmds != CMDS_MESSAGE ? cmdtrans[c] : CMD_NOOP;
  if (c == 1 || c == 2)
    {
      status = c;
      return EOF;
    }
  if (c & 0x80)			/* cursor routing keys */
    switch (status)
      {
      case 0:			/* ordinary cursor routing */
	return (c ^ 0x80) + CR_ROUTEOFFSET;
      case 1:			/* begin block */
	status = 0;
	return (c ^ 0x80) + CR_BEGBLKOFFSET;
      case 2:			/* end block */
	status = 0;
	return (c ^ 0x80) + CR_ENDBLKOFFSET;
      }
  status = 0;
  return c;
}


static int
getbrlkey (void)
{
  static short ptr = 0;		/* input queue pointer */
  static unsigned char q[4];	/* input queue */
  unsigned char c;		/* character buffer */

  while (read (brl_fd, &c, 1))
    {
      if (ptr == 0 && c != 27)
	continue;
      if (ptr == 1 && c != 'K' && c != 'C')
	{
	  ptr = 0;
	  continue;
	}
      q[ptr++] = c;
      if (ptr < 3 || (ptr == 3 && q[1] == 'K' && !q[2]))
	continue;
      ptr = 0;
      if (q[1] == 'K')
	return (q[2] ? q[2] : q[3] | 0x0060);
      return ((int) q[2] | 0x80);
    }
  return EOF;
}
