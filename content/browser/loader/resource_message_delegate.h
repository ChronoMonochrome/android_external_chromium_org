// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_DELEGATE_
#define CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_DELEGATE_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"

namespace IPC {
class Message;
}

namespace net {
class URLRequest;
}

namespace content {

// A ResourceMessageDelegate receives IPC ResourceMsg_* messages for a specified
// URLRequest. The delegate should implement its own IPC handler. It will
// receive the message _after_ the ResourceDispatcherHost has handled it.
class CONTENT_EXPORT ResourceMessageDelegate {
 public:
  ResourceMessageDelegate(const net::URLRequest* request);
  virtual ~ResourceMessageDelegate();

  // Called when the ResourceDispatcherHostImpl receives a message specifically
  // for this delegate.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) = 0;

 private:
  GlobalRequestID id_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceMessageDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_DELEGATE_