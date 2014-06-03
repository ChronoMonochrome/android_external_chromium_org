// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/google/google_url_tracker_infobar_delegate.h"
#include "chrome/browser/google/google_url_tracker_navigation_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GoogleURLTracker;

namespace infobars {
class InfoBarManager;
}

class GoogleURLTrackerMapEntry : public content::NotificationObserver {
 public:
  GoogleURLTrackerMapEntry(
      GoogleURLTracker* google_url_tracker,
      infobars::InfoBarManager* infobar_manager,
      scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper);
  virtual ~GoogleURLTrackerMapEntry();

  bool has_infobar_delegate() const { return !!infobar_delegate_; }
  GoogleURLTrackerInfoBarDelegate* infobar_delegate() {
    return infobar_delegate_;
  }
  void SetInfoBarDelegate(GoogleURLTrackerInfoBarDelegate* infobar_delegate);

  GoogleURLTrackerNavigationHelper* navigation_helper() {
    // This object gives ownership of |navigation_helper_| to the infobar
    // delegate in SetInfoBarDelegate().
    return has_infobar_delegate() ?
        infobar_delegate_->navigation_helper() : navigation_helper_.get();
  }

  void Close(bool redo_search);

 private:
  friend class GoogleURLTrackerTest;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  GoogleURLTracker* const google_url_tracker_;
  const infobars::InfoBarManager* const infobar_manager_;
  GoogleURLTrackerInfoBarDelegate* infobar_delegate_;
  scoped_ptr<GoogleURLTrackerNavigationHelper> navigation_helper_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLTrackerMapEntry);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_URL_TRACKER_MAP_ENTRY_H_
