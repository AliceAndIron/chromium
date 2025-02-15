// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stub methods to allow the closure compiler to compile
 * successfully for external dependencies which cannot be included in
 * compiled_resources2.gyp.
 */

/**
 * Represents a voice as sent from the TTS Handler class. |languageCode| is
 * the language, not the locale, i.e. 'en' rather than 'en-us'. |name| is the
 * user-facing voice name, and |id| is the unique ID for that voice name (which
 * is generated in tts_subpage.js and not passed from tts_handler.cc).
 * |displayLanguage| is the user-facing display string, i.e. 'English'.
 * |fullLanguageCode| is the code with locale, i.e. 'en-us' or 'en-gb'.
 * @typedef {{languageCode: string, name: string, displayLanguage: string,
 *   extensionId: string, id: string, fullLanguageCode: string}}
 */
let TtsHandlerVoice;

/**
 * @typedef {{name: string, extensionId: string, optionsPage: string}}
 */
let TtsHandlerExtension;
