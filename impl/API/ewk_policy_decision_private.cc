// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ewk_policy_decision_private.h"

#include <content/public/browser/browser_thread.h>
#include <content/public/browser/render_view_host.h>
#include <net/http/http_response_headers.h>

#include <algorithm>
#include "web_contents_delegate_efl.h"

using content::BrowserThread;
using content::RenderFrameHost;
using content::RenderViewHost;
using namespace tizen_webview;

namespace {
  void FreeStringShare(void *data) {
    eina_stringshare_del(static_cast<char*>(data));
  }
}

_Ewk_Policy_Decision::_Ewk_Policy_Decision(const GURL &request_url,
                                           net::URLRequest* request,
                                           content::ResourceType resource_type,
                                           PolicyResponseDelegateEfl* delegate)
  : new_window_policy_delegate_(NULL)
  , policy_response_delegate_(delegate)
  , responseHeaders_(NULL)
  , decisionType_(TW_POLICY_DECISION_USE)
  , navigationType_(TW_POLICY_NAVIGATION_TYPE_OTHER)
  , isDecided_(false)
  , isSuspended_(false)
  , responseStatusCode_(0)
  , type_(POLICY_RESPONSE) {
  DCHECK(delegate);
  DCHECK(request);

  net::HttpResponseHeaders* response_headers = request->response_headers();
  DCHECK(response_headers);

  ParseUrl(request_url);
  if (response_headers) {
    responseStatusCode_ = response_headers->response_code();

    std::string mime_type;

    request->GetMimeType(&mime_type);
    responseMime_ = mime_type;

    if (!content::IsResourceTypeFrame(resource_type) &&
        !resource_type == content::RESOURCE_TYPE_FAVICON) {
      decisionType_ = TW_POLICY_DECISION_DOWNLOAD;
    }

    if (request_url.has_password() && request_url.has_username())
      SetAuthorizationIfNecessary(request_url);

    std::string set_cookie_;

    if (response_headers->EnumerateHeader(NULL, "Set-Cookie", &set_cookie_))
      cookie_ = set_cookie_;

    void* iter = NULL;
    std::string name;
    std::string value;
    responseHeaders_ = eina_hash_string_small_new(FreeStringShare);
    while (response_headers->EnumerateHeaderLines(&iter, &name, &value))
      eina_hash_add(responseHeaders_, name.c_str(), eina_stringshare_add(value.c_str()));
  }
}

_Ewk_Policy_Decision::_Ewk_Policy_Decision(const NavigationPolicyParams &params, content::RenderViewHost* rvh)
  : new_window_policy_delegate_(NULL)
  , navigation_policy_handler_(new NavigationPolicyHandlerEfl(rvh, params))
  , frame_(new tizen_webview::Frame(params))
  , cookie_(params.cookie)
  , httpMethod_(params.httpMethod)
  , responseHeaders_(NULL)
  , decisionType_(TW_POLICY_DECISION_USE)
  , navigationType_(static_cast<tizen_webview::Policy_Navigation_Type>(params.type))
  , isDecided_(false)
  , isSuspended_(false)
  , responseStatusCode_(0)
  , type_(POLICY_NAVIGATION) {
  ParseUrl(params.url);
  if (!params.auth.isEmpty())
    SetAuthorizationIfNecessary(params.auth.utf8());
  else if (params.url.has_password() && params.url.has_username())
    SetAuthorizationIfNecessary(params.url);
}

_Ewk_Policy_Decision::_Ewk_Policy_Decision(content::WebContentsDelegateEfl* view, const GURL& url)
  : new_window_policy_delegate_(view)
  , responseHeaders_(NULL)
  , decisionType_(TW_POLICY_DECISION_USE)
  , navigationType_(TW_POLICY_NAVIGATION_TYPE_OTHER)
  , isDecided_(false)
  , isSuspended_(false)
  , responseStatusCode_(0)
  , type_(POLICY_NEWWINDOW) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(view);
  ParseUrl(url);

  RenderFrameHost* rfh = NULL;
  // we can use main frame here
  if (view) {
    view->web_contents().GetMainFrame();
  }

  if (url.has_password() && url.has_username())
    SetAuthorizationIfNecessary(url);

  frame_.reset(new tizen_webview::Frame(rfh));
}

_Ewk_Policy_Decision::~_Ewk_Policy_Decision() {
  eina_hash_free(responseHeaders_);
}

void _Ewk_Policy_Decision::Use() {
  isDecided_ = true;
  switch (type_) {
    case POLICY_RESPONSE:
      policy_response_delegate_->UseResponse();
      break;
    case POLICY_NAVIGATION:
      navigation_policy_handler_->SetDecision(NavigationPolicyHandlerEfl::Unhandled);
      break;
    case POLICY_NEWWINDOW:
      new_window_policy_delegate_->set_new_window_policy(true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void _Ewk_Policy_Decision::Ignore() {
  isDecided_ = true;
  switch (type_) {
    case _Ewk_Policy_Decision::POLICY_RESPONSE:
      policy_response_delegate_->IgnoreResponse();
      break;
    case _Ewk_Policy_Decision::POLICY_NAVIGATION:
      navigation_policy_handler_->SetDecision(NavigationPolicyHandlerEfl::Handled);
      break;
    case _Ewk_Policy_Decision::POLICY_NEWWINDOW:
      new_window_policy_delegate_->set_new_window_policy(false);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void _Ewk_Policy_Decision::Download() {
  isDecided_ = true;
  switch (type_) {
    case _Ewk_Policy_Decision::POLICY_RESPONSE:
      policy_response_delegate_->UseResponse();
      break;
    case _Ewk_Policy_Decision::POLICY_NAVIGATION:
      navigation_policy_handler_->DownloadNavigation();
      break;
    case _Ewk_Policy_Decision::POLICY_NEWWINDOW:
      new_window_policy_delegate_->set_new_window_policy(false);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void _Ewk_Policy_Decision::Suspend() {
  isSuspended_ = true;
}

void _Ewk_Policy_Decision::InitializeOnUIThread() {
  DCHECK(type_ == _Ewk_Policy_Decision::POLICY_RESPONSE);
  DCHECK(policy_response_delegate_.get());

  if (policy_response_delegate_.get()) {
    RenderFrameHost *host = RenderFrameHost::FromID(policy_response_delegate_->GetRenderProcessId(), policy_response_delegate_->GetRenderFrameId());

    // Download request has no render frame id, they're detached. We override it with main frame from render view id
    if (!host) {
      RenderViewHost *viewhost = RenderViewHost::FromID(policy_response_delegate_->GetRenderProcessId(), policy_response_delegate_->GetRenderViewId());

      //DCHECK(viewhost);
      if (viewhost) {
        host = viewhost->GetMainFrame();
      }
    }

    if (host) {
      /*
       * In some situations there is no renderer associated to the response
       * Such case can be observed while running TC utc_blink_ewk_geolocation_permission_request_suspend_func
       */
      frame_.reset(new tizen_webview::Frame(host));
    }
  }
}

tizen_webview::Frame* _Ewk_Policy_Decision::GetFrameRef() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Ups, forgot to initialize something?
  DCHECK(frame_.get());
  return frame_.get();
}

void _Ewk_Policy_Decision::ParseUrl(const GURL& url) {
  url_ = url.spec();
  scheme_ = url.scheme();
  host_ = url.host();
}

void _Ewk_Policy_Decision::SetAuthorizationIfNecessary(const GURL& request_url) {
  // There is no need to check if username or password is empty.
  // It was checked befor in constructor
  AuthPassword_ = request_url.password();
  AuthUser_ = request_url.username();
}

void _Ewk_Policy_Decision::SetAuthorizationIfNecessary(const std::string request) {
  std::string type = request.substr(0, request.find_first_of(' '));
  std::transform(type.begin(), type.end(), type.begin(), ::toupper);

  if (type.compare(BASIC_AUTHORIZATION)) {
    AuthUser_.clear();
    AuthPassword_.clear();
    return;
  }

  std::size_t space = request.find(' ');
  std::size_t colon = request.find(':');

  DCHECK(space != std::string::npos && colon != std::string::npos && colon != request.length());
  if (space == std::string::npos || colon == std::string::npos || colon == request.length())
    return;

  AuthUser_ = request.substr(space + 1, request.length() - colon - 1);
  AuthPassword_ = request.substr(colon + 1);
}

const char* _Ewk_Policy_Decision::GetCookie() const {
  return cookie_.c_str();
}

const char* _Ewk_Policy_Decision::GetAuthUser() const {
  return AuthUser_.empty() ? NULL : AuthUser_.c_str();
}

const char* _Ewk_Policy_Decision::GetAuthPassword() const {
  return AuthPassword_.empty() ? NULL : AuthPassword_.c_str();
}

const char* _Ewk_Policy_Decision::GetUrl() const {
  return url_.empty() ? NULL : url_.c_str();
}

const char* _Ewk_Policy_Decision::GetHttpMethod() const {
  return httpMethod_.empty() ? NULL : httpMethod_.c_str();
}

const char* _Ewk_Policy_Decision::GetScheme() const {
  return scheme_.empty() ? NULL : scheme_.c_str();
}

const char* _Ewk_Policy_Decision::GetHost() const {
  return host_.empty() ? NULL : host_.c_str();
}

const char* _Ewk_Policy_Decision::GetResponseMime() const {
  return responseMime_.empty() ? NULL : responseMime_.c_str();
}
