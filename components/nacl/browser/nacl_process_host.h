// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_
#define COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_

#include "build/build_config.h"

#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "components/nacl/common/nacl_types.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "ipc/ipc_channel_handle.h"
#include "net/socket/socket_descriptor.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/gurl.h"

namespace content {
class BrowserChildProcessHost;
class BrowserPpapiHost;
}

namespace IPC {
class ChannelProxy;
}

namespace nacl {

class NaClHostMessageFilter;
void* AllocateAddressSpaceASLR(base::ProcessHandle process, size_t size);

// Represents the browser side of the browser <--> NaCl communication
// channel. There will be one NaClProcessHost per NaCl process
// The browser is responsible for starting the NaCl process
// when requested by the renderer.
// After that, most of the communication is directly between NaCl plugin
// running in the renderer and NaCl processes.
class NaClProcessHost : public content::BrowserChildProcessHostDelegate {
 public:
  // manifest_url: the URL of the manifest of the Native Client plugin being
  // executed.
  // permissions: PPAPI permissions, to control access to private APIs.
  // render_view_id: RenderView routing id, to control access to private APIs.
  // permission_bits: controls which interfaces the NaCl plugin can use.
  // uses_irt: whether the launched process should use the IRT.
  // uses_nonsfi_mode: whether the program should be loaded under non-SFI mode.
  // enable_dyncode_syscalls: whether the launched process should allow dyncode
  //                          and mmap with PROT_EXEC.
  // enable_exception_handling: whether the launched process should allow
  //                            hardware exception handling.
  // enable_crash_throttling: whether a crash of this process contributes
  //                          to the crash throttling statistics, and also
  //                          whether this process should not start when too
  //                          many crashes have been observed.
  // off_the_record: was the process launched from an incognito renderer?
  // profile_directory: is the path of current profile directory.
  NaClProcessHost(const GURL& manifest_url,
                  ppapi::PpapiPermissions permissions,
                  int render_view_id,
                  uint32 permission_bits,
                  bool uses_irt,
                  bool uses_nonsfi_mode,
                  bool enable_dyncode_syscalls,
                  bool enable_exception_handling,
                  bool enable_crash_throttling,
                  bool off_the_record,
                  const base::FilePath& profile_directory);
  virtual ~NaClProcessHost();

  virtual void OnProcessCrashed(int exit_status) OVERRIDE;

  // Do any minimal work that must be done at browser startup.
  static void EarlyStartup();

  // Specifies throttling time in milliseconds for PpapiHostMsg_Keepalive IPCs.
  static void SetPpapiKeepAliveThrottleForTesting(unsigned milliseconds);

  // Initialize the new NaCl process. Result is returned by sending ipc
  // message reply_msg.
  void Launch(NaClHostMessageFilter* nacl_host_message_filter,
              IPC::Message* reply_msg,
              const base::FilePath& manifest_path);

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

#if defined(OS_WIN)
  void OnProcessLaunchedByBroker(base::ProcessHandle handle);
  void OnDebugExceptionHandlerLaunchedByBroker(bool success);
#endif

  bool Send(IPC::Message* msg);

  content::BrowserChildProcessHost* process() { return process_.get(); }
  content::BrowserPpapiHost* browser_ppapi_host() { return ppapi_host_.get(); }

 private:
  // Internal class that holds the NaClHandle objecs so that
  // nacl_process_host.h doesn't include NaCl headers.  Needed since it's
  // included by src\content, which can't depend on the NaCl gyp file because it
  // depends on chrome.gyp (circular dependency).
  struct NaClInternal;

  bool LaunchNaClGdb();

  // Mark the process as using a particular GDB debug stub port and notify
  // listeners (if the port is not kGdbDebugStubPortUnknown).
  void SetDebugStubPort(int port);

#if defined(OS_POSIX)
  // Create bound TCP socket in the browser process so that the NaCl GDB debug
  // stub can use it to accept incoming connections even when the Chrome sandbox
  // is enabled.
  net::SocketDescriptor GetDebugStubSocketHandle();
#endif

#if defined(OS_WIN)
  // Called when the debug stub port has been selected.
  void OnDebugStubPortSelected(uint16_t debug_stub_port);
#endif

  bool LaunchSelLdr();

  // BrowserChildProcessHostDelegate implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnProcessLaunched() OVERRIDE;

  void OnResourcesReady();

  // Enable the PPAPI proxy only for NaCl processes corresponding to a renderer.
  bool enable_ppapi_proxy() { return render_view_id_ != 0; }

  // Sends the reply message to the renderer who is waiting for the plugin
  // to load. Returns true on success.
  bool ReplyToRenderer(
      const IPC::ChannelHandle& ppapi_channel_handle,
      const IPC::ChannelHandle& trusted_channel_handle,
      const IPC::ChannelHandle& manifest_service_channel_handle);

  // Sends the reply with error message to the renderer.
  void SendErrorToRenderer(const std::string& error_message);

  // Sends the reply message to the renderer. Either result or
  // error message must be empty.
  void SendMessageToRenderer(const NaClLaunchResult& result,
                             const std::string& error_message);

  // Sends the message to the NaCl process to load the plugin. Returns true
  // on success.
  bool StartNaClExecution();

  // Does post-process-launching tasks for starting the NaCl process once
  // we have a connection.
  //
  // Returns false on failure.
  bool StartWithLaunchedProcess();

  // Message handlers for validation caching.
  void OnQueryKnownToValidate(const std::string& signature, bool* result);
  void OnSetKnownToValidate(const std::string& signature);
  void OnResolveFileToken(uint64 file_token_lo, uint64 file_token_hi,
                          IPC::Message* reply_msg);
  void FileResolved(const base::FilePath& file_path,
                    IPC::Message* reply_msg,
                    base::File file);

#if defined(OS_WIN)
  // Message handler for Windows hardware exception handling.
  void OnAttachDebugExceptionHandler(const std::string& info,
                                     IPC::Message* reply_msg);
  bool AttachDebugExceptionHandler(const std::string& info,
                                   IPC::Message* reply_msg);
#endif

  // Called when the PPAPI IPC channels to the browser/renderer have been
  // created.
  void OnPpapiChannelsCreated(
      const IPC::ChannelHandle& browser_channel_handle,
      const IPC::ChannelHandle& ppapi_renderer_channel_handle,
      const IPC::ChannelHandle& trusted_renderer_channel_handle,
      const IPC::ChannelHandle& manifest_service_channel_handle);

  GURL manifest_url_;
  ppapi::PpapiPermissions permissions_;

#if defined(OS_WIN)
  // This field becomes true when the broker successfully launched
  // the NaCl loader.
  bool process_launched_by_broker_;
#endif
  // The NaClHostMessageFilter that requested this NaCl process.  We use
  // this for sending the reply once the process has started.
  scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter_;

  // The reply message to send. We must always send this message when the
  // sub-process either succeeds or fails to unblock the renderer waiting for
  // the reply. NULL when there is no reply to send.
  IPC::Message* reply_msg_;
#if defined(OS_WIN)
  bool debug_exception_handler_requested_;
  scoped_ptr<IPC::Message> attach_debug_exception_handler_reply_msg_;
#endif

  // The file path to the manifest is passed to nacl-gdb when it is used to
  // debug the NaCl loader.
  base::FilePath manifest_path_;

  // Socket pairs for the NaCl process and renderer.
  scoped_ptr<NaClInternal> internal_;

  base::WeakPtrFactory<NaClProcessHost> weak_factory_;

  scoped_ptr<content::BrowserChildProcessHost> process_;

  bool uses_irt_;
  bool uses_nonsfi_mode_;

  bool enable_debug_stub_;
  bool enable_dyncode_syscalls_;
  bool enable_exception_handling_;
  bool enable_crash_throttling_;

  bool off_the_record_;

  const base::FilePath profile_directory_;

  // Channel proxy to terminate the NaCl-Browser PPAPI channel.
  scoped_ptr<IPC::ChannelProxy> ipc_proxy_channel_;
  // Browser host for plugin process.
  scoped_ptr<content::BrowserPpapiHost> ppapi_host_;

  int render_view_id_;

  // Throttling time in milliseconds for PpapiHostMsg_Keepalive IPCs.
  static unsigned keepalive_throttle_interval_milliseconds_;

  DISALLOW_COPY_AND_ASSIGN(NaClProcessHost);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_PROCESS_HOST_H_
