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

typedef struct {
   int (*open) (void);
   int (*generate) (int frequency, int duration);
   void (*close) (int immediate);
} ToneGenerator;

typedef ToneGenerator *(ToneProcedure) (void);
extern ToneProcedure toneSpeaker;
extern ToneProcedure toneSoundCard;
extern ToneProcedure toneSequencer;
extern ToneProcedure toneAdLib;
