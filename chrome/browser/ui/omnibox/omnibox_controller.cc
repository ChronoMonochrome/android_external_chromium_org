// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "extensions/common/constants.h"
#include "ui/gfx/rect.h"


OmniboxController::OmniboxController(OmniboxEditModel* omnibox_edit_model,
                                     Profile* profile)
    : omnibox_edit_model_(omnibox_edit_model),
      profile_(profile) {
  autocomplete_controller_.reset(new AutocompleteController(profile, this,
      chrome::IsInstantExtendedAPIEnabled() ?
          AutocompleteClassifier::kInstantExtendedOmniboxProviders :
          AutocompleteClassifier::kDefaultOmniboxProviders));
}

OmniboxController::~OmniboxController() {
}

void OmniboxController::StartAutocomplete(
    string16 user_text,
    size_t cursor_position,
    const GURL& current_url,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    int omnibox_start_margin) const {
  ClearPopupKeywordMode();
  popup_->SetHoveredLine(OmniboxPopupModel::kNoMatch);

  if (chrome::IsInstantExtendedAPIEnabled()) {
    autocomplete_controller_->search_provider()->
        SetOmniboxStartMargin(omnibox_start_margin);
  }

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(AutocompleteInput(
      user_text, cursor_position, string16(), current_url,
      prevent_inline_autocomplete, prefer_keyword, allow_exact_keyword_match,
      AutocompleteInput::ALL_MATCHES));
}

void OmniboxController::OnResultChanged(bool default_match_changed) {
  // TODO(beaudoin): There should be no need to access the popup when using
  // instant extended, remove this reference.
  const bool was_open = popup_->IsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    const AutocompleteResult& result = this->result();
    const AutocompleteResult::const_iterator match(result.default_match());
    if (match != result.end()) {
      current_match_ = *match;
      // TODO(beaudoin): This code could be made simpler if AutocompleteMatch
      // had an |inline_autocompletion| instead of |inline_autocomplete_offset|.
      // The |fill_into_edit| we get may not match what we have in the view at
      // that time. We're only interested in the inline_autocomplete part, so
      // update this here.
      current_match_.fill_into_edit = omnibox_edit_model_->user_text();
      if (match->inline_autocomplete_offset < match->fill_into_edit.length()) {
        current_match_.inline_autocomplete_offset =
            current_match_.fill_into_edit.length();
        current_match_.fill_into_edit += match->fill_into_edit.substr(
            match->inline_autocomplete_offset);
      } else {
        current_match_.inline_autocomplete_offset = string16::npos;
      }

      if (!prerender::IsOmniboxEnabled(profile_))
        DoPreconnect(*match);
      omnibox_edit_model_->OnCurrentMatchChanged();
    } else {
      InvalidateCurrentMatch();
      popup_->OnResultChanged();
      omnibox_edit_model_->OnPopupDataChanged(string16(), NULL, string16(),
                                              false);
    }
  } else {
    popup_->OnResultChanged();
  }

  if (!popup_->IsOpen() && was_open) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    omnibox_edit_model_->AcceptTemporaryTextAsUserText();
  }
}

void OmniboxController::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  // TODO(jered): Delete this.
}

void OmniboxController::InvalidateCurrentMatch() {
  current_match_ = AutocompleteMatch();
}

void OmniboxController::ClearPopupKeywordMode() const {
  if (popup_->IsOpen() &&
      popup_->selected_line_state() == OmniboxPopupModel::KEYWORD)
    popup_->SetSelectedLineState(OmniboxPopupModel::NORMAL);
}

void OmniboxController::DoPreconnect(const AutocompleteMatch& match) {
  if (!match.destination_url.SchemeIs(extensions::kExtensionScheme)) {
    // Warm up DNS Prefetch cache, or preconnect to a search service.
    UMA_HISTOGRAM_ENUMERATION("Autocomplete.MatchType", match.type,
                              AutocompleteMatchType::NUM_TYPES);
    if (profile_->GetNetworkPredictor()) {
      profile_->GetNetworkPredictor()->AnticipateOmniboxUrl(
          match.destination_url,
          predictors::AutocompleteActionPredictor::IsPreconnectable(match));
    }
    // We could prefetch the alternate nav URL, if any, but because there
    // can be many of these as a user types an initial series of characters,
    // the OS DNS cache could suffer eviction problems for minimal gain.
  }
}

bool OmniboxController::UseVerbatimInstant(bool just_deleted_text) const {
#if defined(OS_MACOSX)
  // TODO(suzhe): Fix Mac port to display Instant suggest in a separated NSView,
  // so that we can display Instant suggest along with composition text.
  const AutocompleteInput& input = autocomplete_controller_->input();
  if (input.prevent_inline_autocomplete())
    return true;
#endif

  // The value of input.prevent_inline_autocomplete() is determined by the
  // following conditions:
  // 1. If the caret is at the end of the text.
  // 2. If it's in IME composition mode.
  // We send the caret position to Instant (so it can determine #1 itself), and
  // we use a separated widget for displaying the Instant suggest (so it doesn't
  // interfere with #2). So, we don't need to care about the value of
  // input.prevent_inline_autocomplete() here.
  return just_deleted_text || popup_->selected_line() != 0;
}

InstantController* OmniboxController::GetInstantController() const {
  return omnibox_edit_model_->GetInstantController();
}

void OmniboxController::CreateAndSetInstantMatch(
    string16 query_string,
    string16 input_text,
    AutocompleteMatchType::Type match_type) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service)
    return;

  TemplateURL* template_url =
      template_url_service->GetDefaultSearchProvider();
  if (!template_url)
    return;

  current_match_ = SearchProvider::CreateSearchSuggestion(
      NULL, 0, match_type, template_url, query_string, input_text,
      AutocompleteInput(), false, 0, -1, true);
}