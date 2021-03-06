// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/policy_response_delegate_efl.h"

#include "browser/resource_throttle_efl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"

#include "API/ewk_policy_decision_private.h"
#include "web_contents_delegate_efl.h"
#include "common/web_contents_utils.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::RenderFrameHost;
using content::ResourceDispatcherHost;
using content::ResourceRequestInfo;
using content::WebContents;
using content::ResourceController;

using web_contents_utils::WebContentsFromFrameID;

namespace {

static content::WebContentsDelegateEfl* WebContentsDelegateFromFrameId(int render_process_id, int render_frame_id) {
  WebContents* web_contents = WebContentsFromFrameID(render_process_id, render_frame_id);
  if (!web_contents)
    return NULL;

  return static_cast<content::WebContentsDelegateEfl*>(web_contents->GetDelegate());
}

}

PolicyResponseDelegateEfl::PolicyResponseDelegateEfl(
    net::URLRequest* request,
    content::ResourceType resource_type,
    ResourceThrottleEfl* throttle)
    : policy_decision_(new tizen_webview::PolicyDecision(request->url(),
                                                         request,
                                                         resource_type,
                                                         this)),
      throttle_(throttle),
      render_process_id_(0),
      render_frame_id_(0),
      render_view_id_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  if (info) {
    info->GetAssociatedRenderFrame(&render_process_id_, &render_frame_id_);
    render_view_id_ = info->GetRouteID();

  } else {
    ResourceRequestInfo::GetRenderFrameForRequest(request, &render_process_id_, &render_frame_id_);
  }

  /*
   * In some situations there is no render_process and render_frame associated with
   * request. Such situation happens in TC utc_blink_ewk_geolocation_permission_request_suspend_func
   */
  //DCHECK(render_process_id_ > 0);
  //DCHECK(render_frame_id_ > 0 || render_view_id_ > 0);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&PolicyResponseDelegateEfl::HandlePolicyResponseOnUIThread, this));
}

void PolicyResponseDelegateEfl::HandlePolicyResponseOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(policy_decision_.get());

  policy_decision_->GetImpl()->InitializeOnUIThread();
  // Delegate may be retrieved ONLY on UI thread
  content::WebContentsDelegateEfl *delegate = WebContentsDelegateFromFrameId(render_process_id_, render_frame_id_);
  if (!delegate) {
    UseResponse();
    return;
  }

  // web_view_ takes owenership of Ewk_Policy_Decision. This is same as WK2/Tizen
  delegate->web_view()->InvokePolicyResponseCallback(policy_decision_.release());
}

void PolicyResponseDelegateEfl::UseResponse() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PolicyResponseDelegateEfl::UseResponseOnIOThread, this));
}

void PolicyResponseDelegateEfl::IgnoreResponse() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PolicyResponseDelegateEfl::IgnoreResponseOnIOThread, this));
}

void PolicyResponseDelegateEfl::UseResponseOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (throttle_) {
    throttle_->Resume();
    // decision is already taken so there is no need to use throttle anymore.
    throttle_ = NULL;
  }
}

void PolicyResponseDelegateEfl::IgnoreResponseOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (throttle_) {
    throttle_->Ignore();
    throttle_ = NULL;

  }
}

void PolicyResponseDelegateEfl::ThrottleDestroyed(){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
   // The throttle has gone so don't try to do anything about it further.
  throttle_ = NULL;
}
