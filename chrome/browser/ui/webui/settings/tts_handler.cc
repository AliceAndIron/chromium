// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/tts_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_controller_impl.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

void TtsHandler::HandleGetAllTtsVoiceData(const base::ListValue* args) {
  OnVoicesChanged();
}

void TtsHandler::HandleGetTtsExtensions(const base::ListValue* args) {
  base::ListValue responses;
  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  const std::set<std::string> extensions =
      TtsEngineExtensionObserver::GetInstance(profile)->GetTtsExtensions();
  std::set<std::string>::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const std::string extension_id = *iter;
    const extensions::Extension* extension =
        registry->GetInstalledExtension(extension_id);
    if (!extension) {
      // The extension is still loading from OnVoicesChange call to
      // TtsControllerImpl::GetVoices(). Don't do any work, voices will
      // be updated again after extension load.
      continue;
    }
    base::DictionaryValue response;
    response.SetPath({"name"}, base::Value(extension->name()));
    response.SetPath({"extensionId"}, base::Value(extension_id));
    if (extensions::OptionsPageInfo::HasOptionsPage(extension)) {
      response.SetPath(
          {"optionsPage"},
          base::Value(
              extensions::OptionsPageInfo::GetOptionsPage(extension).spec()));
    }
    responses.GetList().push_back(std::move(response));
  }

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("tts-extensions-updated"), responses);
}

void TtsHandler::OnVoicesChanged() {
  TtsControllerImpl* impl = TtsControllerImpl::GetInstance();
  std::vector<VoiceData> voices;
  impl->GetVoices(Profile::FromWebUI(web_ui()), &voices);
  base::ListValue responses;
  for (const auto& voice : voices) {
    base::DictionaryValue response;
    std::string language_code = l10n_util::GetLanguage(voice.lang);
    response.SetPath({"name"}, base::Value(voice.name));
    response.SetPath({"languageCode"}, base::Value(language_code));
    response.SetPath({"fullLanguageCode"}, base::Value(voice.lang));
    response.SetPath({"extensionId"}, base::Value(voice.extension_id));
    response.SetPath(
        {"displayLanguage"},
        base::Value(l10n_util::GetDisplayNameForLocale(
            language_code, g_browser_process->GetApplicationLocale(), true)));
    responses.GetList().push_back(std::move(response));
  }
  AllowJavascript();
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("all-voice-data-updated"), responses);

  // Also refresh the TTS extensions in case they have changed.
  HandleGetTtsExtensions(nullptr);
}

void TtsHandler::HandlePreviewTtsVoice(const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string text;
  std::string voice_id;
  args->GetString(0, &text);
  args->GetString(1, &voice_id);

  if (text.empty() || voice_id.empty())
    return;

  std::unique_ptr<base::DictionaryValue> json =
      base::DictionaryValue::From(base::JSONReader::Read(voice_id));
  std::string name;
  std::string extension_id;
  json->GetString("name", &name);
  json->GetString("extension", &extension_id);

  Utterance* utterance = new Utterance(Profile::FromWebUI(web_ui()));
  utterance->set_text(text);
  utterance->set_voice_name(name);
  utterance->set_extension_id(extension_id);
  utterance->set_src_url(GURL("chrome://settings/manageAccessibility/tts"));
  utterance->set_event_delegate(nullptr);
  TtsController::GetInstance()->Stop();
  TtsController::GetInstance()->SpeakOrEnqueue(utterance);
}

void TtsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceData",
      base::BindRepeating(&TtsHandler::HandleGetAllTtsVoiceData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTtsExtensions",
      base::BindRepeating(&TtsHandler::HandleGetTtsExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewTtsVoice", base::BindRepeating(&TtsHandler::HandlePreviewTtsVoice,
                                             base::Unretained(this)));
}

void TtsHandler::OnJavascriptAllowed() {
  TtsController::GetInstance()->AddVoicesChangedDelegate(this);
}

void TtsHandler::OnJavascriptDisallowed() {
  TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
}

}  // namespace settings
