/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2021 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any
 * later version. Please see the file LICENSE-LGPL for details.
 *
 * Web Page: http://brltty.app/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

package org.a11y.brltty.android.settings;
import org.a11y.brltty.android.*;
import org.a11y.brltty.core.CoreWrapper;

public class SpeechSupportSetting extends SingleSelectionSetting {
  public SpeechSupportSetting (SettingsFragment fragment) {
    super(fragment, R.string.PREF_KEY_SPEECH_SUPPORT);
  }

  @Override
  protected final void onSelectionChanged (final String newSelection) {
    CoreWrapper.runOnCoreThread(
      new Runnable() {
        @Override
        public void run () {
          CoreWrapper.changeSpeechDriver(newSelection);
          CoreWrapper.restartSpeechDriver();
        }
      }
    );
  }
}
