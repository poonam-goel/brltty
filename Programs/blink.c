/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2013 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

#include "prologue.h"

#include "blink.h"
#include "prefs.h"
#include "async_alarm.h"

struct BlinkDescriptorStruct {
  const unsigned char *const isEnabled;
  const unsigned char *const visibleTime;
  const unsigned char *const invisibleTime;
  unsigned const initialState:1;

  unsigned isVisible:1;
  AsyncHandle alarmHandle;
};

BlinkDescriptor cursorBlinkDescriptor = {
  .isEnabled = &prefs.blinkingCursor,
  .visibleTime = &prefs.cursorVisibleTime,
  .invisibleTime = &prefs.cursorInvisibleTime,
  .initialState = 0
};

BlinkDescriptor attributesBlinkDescriptor = {
  .isEnabled = &prefs.blinkingAttributes,
  .visibleTime = &prefs.attributesVisibleTime,
  .invisibleTime = &prefs.attributesInvisibleTime,
  .initialState = 0
};

BlinkDescriptor capitalsBlinkDescriptor = {
  .isEnabled = &prefs.blinkingCapitals,
  .visibleTime = &prefs.capitalsVisibleTime,
  .invisibleTime = &prefs.capitalsInvisibleTime,
  .initialState = 1
};

BlinkDescriptor speechCursorBlinkDescriptor = {
  .isEnabled = &prefs.blinkingSpeechCursor,
  .visibleTime = &prefs.speechCursorVisibleTime,
  .invisibleTime = &prefs.speechCursorInvisibleTime,
  .initialState = 0
};

static BlinkDescriptor *const blinkDescriptors[] = {
  &cursorBlinkDescriptor,
  &attributesBlinkDescriptor,
  &capitalsBlinkDescriptor,
  &speechCursorBlinkDescriptor,
  NULL
};

int
isBlinkEnabled (const BlinkDescriptor *blink) {
  return !!*blink->isEnabled;
}

int
isBlinkVisible (const BlinkDescriptor *blink) {
  if (!isBlinkEnabled(blink)) return 1;
  return blink->isVisible;
}

static void setBlinkAlarm (BlinkDescriptor *blink, int duration);

void
setBlinkState (BlinkDescriptor *blink, int visible) {
  blink->isVisible = visible;

  if (isBlinkEnabled(blink)) {
    unsigned char time = blink->isVisible? *blink->visibleTime: *blink->invisibleTime;

    setBlinkAlarm(blink, PREFERENCES_TIME(time));
  } else if (blink->alarmHandle) {
    asyncCancelRequest(blink->alarmHandle);
    blink->alarmHandle = NULL;
  }
}

static void
handleBlinkAlarm (const AsyncAlarmResult *result) {
  BlinkDescriptor *blink = result->data;

  asyncDiscardHandle(blink->alarmHandle);
  blink->alarmHandle = NULL;

  setBlinkState(blink, !blink->isVisible);
}

static void
setBlinkAlarm (BlinkDescriptor *blink, int duration) {
  if (blink->alarmHandle) {
    asyncResetAlarmIn(blink->alarmHandle, duration);
  } else {
    asyncSetAlarmIn(&blink->alarmHandle, duration, handleBlinkAlarm, blink);
  }
}

void
resetBlinkDescriptor (BlinkDescriptor *blink) {
  setBlinkState(blink, blink->initialState);
}

void
resetBlinkDescriptors (void) {
  BlinkDescriptor *const *blink = blinkDescriptors;

  while (*blink) {
    resetBlinkDescriptor(*blink);
    blink += 1;
  }
}
