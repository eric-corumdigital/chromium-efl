// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_contents_delegate_efl.h"

#include "API/ewk_console_message_private.h"
#include "API/ewk_error_private.h"
#include "API/ewk_certificate_private.h"
#include "API/ewk_policy_decision_private.h"
#include "API/ewk_user_media_private.h"
#include "browser/policy_response_delegate_efl.h"
#include "browser/renderer_host/render_widget_host_view_efl.h"
#include "browser/inputpicker/color_chooser_efl.h"
#include "common/render_messages_efl.h"
#include "eweb_view.h"
#include "eweb_view_callbacks.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/view_messages.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/common/favicon_url.h"
#include "content/common/date_time_suggestion.h"
#include "net/base/load_states.h"
#include "net/http/http_response_headers.h"
#include "printing/pdf_metafile_skia.h"
#include "url/gurl.h"
#include "browser/favicon/favicon_service.h"

#include "tizen_webview/public/tw_web_context.h"
#include "tizen_webview/public/tw_input_type.h"

#ifdef TIZEN_AUTOFILL_SUPPORT
#include "browser/autofill/autofill_manager_delegate_efl.h"
#include "browser/password_manager/password_manager_client_efl.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"

using autofill::AutofillDriverImpl;
using autofill::AutofillManager;
using autofill::AutofillManagerDelegateEfl;
#endif

using base::string16;
using namespace tizen_webview;
using namespace ui;

namespace content {

void WritePdfDataToFile(printing::PdfMetafileSkia* metafile, const base::FilePath& filename) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(metafile);
  base::File file(filename, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  metafile->SaveTo(&file);
  file.Close();
  delete metafile;
}

WebContentsDelegateEfl::WebContentsDelegateEfl(EWebView* view)
    : web_view_(view)
    , is_fullscreen_(false)
    , web_contents_(view->web_contents())
    , document_created_(false)
    , should_open_new_window_(true)
    , dialog_manager_(NULL)
    , forward_backward_list_count_(0)
    , WebContentsObserver(&view->web_contents())
    , weak_ptr_factory_(this) {
#ifdef TIZEN_AUTOFILL_SUPPORT
  AutofillManagerDelegateEfl::CreateForWebContents(&web_contents_);
  AutofillManagerDelegateEfl * autofill_manager =
    AutofillManagerDelegateEfl::FromWebContents(&web_contents_);
  autofill_manager->SetEWebView(view);
  AutofillDriverImpl::CreateForWebContentsAndDelegate(&web_contents_,
    autofill_manager, EWebView::GetPlatformLocale(), AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);
  PasswordManagerClientEfl::CreateForWebContents(&web_contents_);
#endif
}

WebContentsDelegateEfl::~WebContentsDelegateEfl() {
  // It's important to delete web_contents_ before dialog_manager_
  // destructor of web contents uses dialog_manager_

  delete dialog_manager_;
}

WebContents* WebContentsDelegateEfl::OpenURLFromTab(
  WebContents* source,
  const content::OpenURLParams& params) {

  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;

  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD) {
    Evas_Object* new_object = NULL;
    web_view_->SmartCallback<EWebViewCallbacks::CreateNewWindow>().call(&new_object);

    if (!new_object)
      return NULL;

    EWebView* wv = EWebView::FromEvasObject(new_object);
    DCHECK(wv);
    wv->SetURL(url.spec().c_str());
    return NULL;
  }

  ui::PageTransition transition(ui::PageTransitionFromInt(params.transition));
  source->GetController().LoadURL(url, params.referrer, transition,
                                  std::string());
  return source;
}

void WebContentsDelegateEfl::NavigationStateChanged(
    const WebContents* source, InvalidateTypes changed_flags) {
  if (changed_flags & content::INVALIDATE_TYPE_URL) {
    const char* url = source->GetVisibleURL().spec().c_str();
    web_view_->SmartCallback<EWebViewCallbacks::URLChanged>().call(url);
    web_view_->SmartCallback<EWebViewCallbacks::URIChanged>().call(url);
  }
}

void WebContentsDelegateEfl::LoadingStateChanged(WebContents* source,
                                                 bool to_different_document) {
  if (source->IsLoading())
    web_view_->SmartCallback<EWebViewCallbacks::LoadProgressStarted>().call();
  else
    web_view_->SmartCallback<EWebViewCallbacks::LoadProgressFinished>().call();
}

void WebContentsDelegateEfl::LoadProgressChanged(WebContents* source, double progress) {
  web_view_->SetProgressValue(progress);
  web_view_->SmartCallback<EWebViewCallbacks::LoadProgress>().call(&progress);
}

bool WebContentsDelegateEfl::ShouldCreateWebContents(
    WebContents* web_contents,
    int route_id,
    WindowContainerType window_container_type,
    const string16& /*frame_name*/,
    const GURL& target_url,
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  // this method is called ONLY when creating new window - no matter what type
  web_view_->set_policy_decision(new tizen_webview::PolicyDecision(this, target_url));
  web_view_->SmartCallback<EWebViewCallbacks::NewWindowPolicyDecision>().call(web_view_->get_policy_decision());
  // Chromium has sync API. We cannot block this calls on UI thread.
  CHECK(!web_view_->get_policy_decision()->isSuspended());
  if (web_view_->get_policy_decision()->isDecided())
    return should_open_new_window_;

  // By default we return false. If embedder is not prepared to handle new window creation then we prevent this behaviour.
  return false;
}

void WebContentsDelegateEfl::WebContentsCreated(
    WebContents* source_contents,
    int opener_render_frame_id,
    const string16& frame_name,
    const GURL& target_url,
    WebContents* new_contents) {
  // FIXME: we should have the specifications for the new window (size, position, etc.) to set it up correctly.

  RenderWidgetHostViewEfl* source_rwhv = reinterpret_cast<RenderWidgetHostViewEfl*>(source_contents->GetRenderWidgetHostView());
  DCHECK(source_rwhv);
  EWebView* source_view = source_rwhv->eweb_view();
  DCHECK(source_view);
  source_view->CreateNewWindow(new_contents);
}

void WebContentsDelegateEfl::CloseContents(WebContents* source) {
  web_view_->SmartCallback<EWebViewCallbacks::WindowClosed>().call();
}

void WebContentsDelegateEfl::ToggleFullscreenModeForTab(WebContents* web_contents,
      bool enter_fullscreen) {
  is_fullscreen_ = enter_fullscreen;
  if(enter_fullscreen)
    web_view_->SmartCallback<EWebViewCallbacks::EnterFullscreen>().call();
  else
    web_view_->SmartCallback<EWebViewCallbacks::ExitFullscreen>().call();
}

bool WebContentsDelegateEfl::IsFullscreenForTabOrPending(
      const WebContents* web_contents) const {
  return is_fullscreen_;
}

void WebContentsDelegateEfl::RegisterProtocolHandler(WebContents* web_contents,
        const std::string& protocol, const GURL& url, bool user_gesture) {
  scoped_ptr<tizen_webview::Custom_Handlers_Data> protocol_data(
      new tizen_webview::Custom_Handlers_Data(protocol.c_str(),
          url.host().c_str(), url.spec().c_str()));
  web_view_->SmartCallback<EWebViewCallbacks::RegisterProtocolHandler>().call(protocol_data.get());
}

#if defined(TIZEN_MULTIMEDIA_SUPPORT)
WebContentsDelegateEfl::PendingAccessRequest::PendingAccessRequest(
  const content::MediaStreamRequest& request,
  const content::MediaResponseCallback& callback)
  : request(request)
  , callback(callback) {
}

WebContentsDelegateEfl::PendingAccessRequest::~PendingAccessRequest() {
}

void WebContentsDelegateEfl::OnAccessRequestResponse(bool allowed) {
  MediaStreamDevices devices;
  if (requests_queue_.empty()) {
    LOG(ERROR) << __FUNCTION__ << " Empty Queue ";
    return;
  }
  PendingAccessRequest pending_request = requests_queue_.front();
  if (pending_request.callback.is_null()) {
    requests_queue_.pop_front();
    LOG(ERROR) << __FUNCTION__ << " Invalid Callback ";
    return;
  }
  if (allowed) {
    if (pending_request.request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
      devices.push_back(MediaStreamDevice(pending_request.request.audio_type,
                                          "default", "Default"));
    }
    if (pending_request.request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
#if defined(OS_TIZEN_MOBILE)
      devices.push_back(MediaStreamDevice(pending_request.request.video_type,
                                          "1", "1"));
#elif defined(OS_TIZEN_TV)
      devices.push_back(MediaStreamDevice(pending_request.request.video_type,
                                          "0", "0"));
#else
      devices.push_back(MediaStreamDevice(pending_request.request.video_type,
                                          "/dev/video0", "1"));
#endif
    }
    pending_request.callback.Run(devices, MEDIA_DEVICE_OK,scoped_ptr<MediaStreamUI>());
  } else {
    LOG(ERROR) << __FUNCTION__ << " Decline request with empty list";
    pending_request.callback.Run(MediaStreamDevices(),
                                 MEDIA_DEVICE_NOT_SUPPORTED,
                                 scoped_ptr<MediaStreamUI>());
  }
  requests_queue_.pop_front();
}

bool WebContentsDelegateEfl::CheckMediaAccessPermission(
    WebContents* web_contents,
    const GURL& security_origin,
    MediaStreamType type) {
  return true;
}

void WebContentsDelegateEfl::RequestMediaAccessPermission(
        WebContents* web_contents,
        const MediaStreamRequest& request,
        const MediaResponseCallback& callback) {
  //send callback to application to request for user permission.
  _Ewk_User_Media_Permission_Request* media_permission_request =
    new _Ewk_User_Media_Permission_Request(web_view_, request,this);
  requests_queue_.push_back(PendingAccessRequest(request, callback));
  web_view_->SmartCallback<EWebViewCallbacks::UserMediaPermission>().call(
    media_permission_request);
}
#endif

void WebContentsDelegateEfl::OnAuthRequired(net::URLRequest* request,
                                            const std::string& realm,
                                            LoginDelegateEfl* login_delegate) {
  web_view_->InvokeAuthCallback(login_delegate, request->url(), realm);
}

void WebContentsDelegateEfl::DidStartProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                                             const GURL& validated_url,
                                                             bool is_error_page,
                                                             bool is_iframe_srcdoc) {
  web_view_->SmartCallback<EWebViewCallbacks::ProvisionalLoadStarted>().call();
}

void WebContentsDelegateEfl::DidCommitProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                                              const GURL& url,
                                                              ui::PageTransition transition_type) {
  web_view_->SmartCallback<EWebViewCallbacks::LoadCommitted>().call();
}

void WebContentsDelegateEfl::DidNavigateAnyFrame(RenderFrameHost* render_frame_host, const LoadCommittedDetails& details, const FrameNavigateParams& params) {
  web_view_->SmartCallback<EWebViewCallbacks::ProvisionalLoadRedirect>().call();
  static_cast<BrowserContextEfl*>(web_contents_.GetBrowserContext())->AddVisitedURLs(params.redirects);
}

void WebContentsDelegateEfl::DidFinishLoad(RenderFrameHost* render_frame_host,
                                           const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;

  NavigationEntry *entry = web_contents().GetController().GetVisibleEntry();
  DCHECK(entry);
  FaviconStatus &favicon = entry->GetFavicon();

  if (favicon.valid) {
    // check/update the url and favicon url in favicon database
    FaviconService fs;
    fs.SetFaviconURLForPageURL(favicon.url, validated_url);

    // download favicon if there is no such in database
    if (!fs.ExistsForFaviconURL(favicon.url)) {
      LOG(ERROR) << "[DidFinishLoad] :: no favicon in database for URL: "
                 << favicon.url.spec();
      favicon_downloader_.reset(
        new FaviconDownloader(&web_contents_,
                              favicon.url,
                              base::Bind(
                                &WebContentsDelegateEfl::DidDownloadFavicon,
                                weak_ptr_factory_.GetWeakPtr())));
      favicon_downloader_->Start();
    } else {
      web_view_->SmartCallback<EWebViewCallbacks::IconReceived>().call();
    }
  }

  web_view_->SmartCallback<EWebViewCallbacks::LoadFinished>().call();
}

void WebContentsDelegateEfl::DidStartLoading(RenderViewHost* render_view_host) {
  web_view_->SmartCallback<EWebViewCallbacks::LoadStarted>().call();
}

void WebContentsDelegateEfl::DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) {
  // select and set proper favicon
  for (unsigned int i = 0; i < candidates.size(); ++i) {
    FaviconURL favicon = candidates[i];
    if (favicon.icon_type == FaviconURL::FAVICON && !favicon.icon_url.is_empty()) {
      NavigationEntry *entry = web_contents_.GetController().GetVisibleEntry();
      if (!entry)
        return;
      entry->GetFavicon().url = favicon.icon_url;
      entry->GetFavicon().valid = true;
      return;
    }
  }
  return;
}

void WebContentsDelegateEfl::DidDownloadFavicon(bool success, const GURL& icon_url, const SkBitmap& bitmap) {
  favicon_downloader_.reset();
  if (success) {
    FaviconService fs;
    fs.SetBitmapForFaviconURL(bitmap, icon_url);
    // emit "icon,received"
    web_view_->SmartCallback<EWebViewCallbacks::IconReceived>().call();
  }
}

void WebContentsDelegateEfl::RequestCertificateConfirm(WebContents* /*web_contents*/,
                                                      int cert_error,
                                                      const net::SSLInfo& ssl_info,
                                                      const GURL& url,
                                                      ResourceType /*resource_type*/,
                                                      bool /*overridable*/,
                                                      bool /*strict_enforcement*/,
                                                      const base::Callback<void(bool)>& callback,
                                                      CertificateRequestResultType* result) {
  DCHECK(result);
  std::string pem_certificate;
  if (!net::X509Certificate::GetPEMEncoded(ssl_info.cert->os_cert_handle(), &pem_certificate)) {
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
    return;
  }
  // |result| can be used to deny/cancel request synchronously.
  // ewk api does not need it. We don't use it.
  scoped_ptr<_Ewk_Certificate_Policy_Decision> policy(new _Ewk_Certificate_Policy_Decision(url,
                                                                                         pem_certificate,
                                                                                         cert_error,
                                                                                         callback));
  web_view_->SmartCallback<EWebViewCallbacks::RequestCertificateConfirm>().call(policy.get());

  if (!policy->isSuspended())
    policy->setDecision(true);
  else
    policy.release(); // if policy is suspended, the API takes over the policy object lifetime
                      // and policy will be deleted after decision is made
}

void WebContentsDelegateEfl::SetContentSecurityPolicy(const std::string& policy, tizen_webview::ContentSecurityPolicyType header_type) {
  // Might makes sense as it only uses existing functionality already exposed for javascript. Needs extra api at blink side.
  // Not necessary for eflwebview bringup.
#if !defined(EWK_BRINGUP)
  if (document_created_) {
    RenderViewHost* rvh = web_contents_.GetRenderViewHost();
    rvh->Send(new EwkViewMsg_SetCSP(rvh->GetRoutingID(), policy, header_type));
  } else {
    DCHECK(!pending_content_security_policy_.get());
    pending_content_security_policy_.reset(new ContentSecurityPolicy(policy, header_type));
  }
#endif
}

void WebContentsDelegateEfl::ShowPopupMenu(RenderFrameHost* render_frame_host,
                                           const gfx::Rect& rect,
                                           blink::TextDirection textDirection,
                                           double pageScaleFactor,
                                           const std::vector<MenuItem>& items,
                                           int data,
                                           int selectedIndex,
                                           bool multiple) {
  web_view_->ShowPopupMenu(rect,
                           textDirection,
                           pageScaleFactor,
                           items,
                           data,
                           selectedIndex,
                           multiple);
}

void WebContentsDelegateEfl::HidePopupMenu() {
  web_view_->HidePopupMenu();
}

void WebContentsDelegateEfl::UpdateFormNavigation(int formElementCount,
    int currentNodeIndex, bool prevState, bool nextState) {
  web_view_->UpdateFormNavigation(formElementCount, currentNodeIndex,
      prevState, nextState);
}

void WebContentsDelegateEfl::FindReply(WebContents* web_contents,
                                       int request_id,
                                       int number_of_matches,
                                       const gfx::Rect& selection_rect,
                                       int active_match_ordinal,
                                       bool final_update) {
  if (final_update && request_id == web_view_->current_find_request_id()) {
    unsigned int uint_number_of_matches = static_cast<unsigned int>(number_of_matches);
    web_view_->SmartCallback<EWebViewCallbacks::TextFound>().call(&uint_number_of_matches);
  }
}

JavaScriptDialogManager* WebContentsDelegateEfl::GetJavaScriptDialogManager() {
  if (!dialog_manager_)
    dialog_manager_ = new JavaScriptDialogManagerEfl();
  return dialog_manager_;
}

void WebContentsDelegateEfl::OnUpdateSettings(const Ewk_Settings *settings) {
#ifdef TIZEN_AUTOFILL_SUPPORT
  PasswordManagerClientEfl *client =
      PasswordManagerClientEfl::FromWebContents(&web_contents_);
  if(client) {
    client->SetPasswordManagerSavingEnabled(settings->autofillPasswordForm());
    client->SetPasswordManagerFillingEnabled(settings->autofillProfileForm());
  }
#endif
}

bool WebContentsDelegateEfl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebContentsDelegateEfl, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(EwkHostMsg_GetContentSecurityPolicy, OnGetContentSecurityPolicy)
    IPC_MESSAGE_HANDLER(EwkHostMsg_DidPrintPagesToPdf, OnPrintedMetafileReceived)
    IPC_MESSAGE_HANDLER(EwkHostMsg_WrtMessage, OnWrtPluginMessage)
    IPC_MESSAGE_HANDLER(EwkHostMsg_FormSubmit, OnFormSubmit)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(EwkHostMsg_WrtSyncMessage, OnWrtPluginSyncMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebContentsDelegateEfl::OnFormSubmit(const GURL&url) {
  web_view_->SmartCallback<EWebViewCallbacks::FormSubmit>().call(url.GetContent().c_str());
}

void WebContentsDelegateEfl::OnWrtPluginMessage(const tizen_webview::WrtIpcMessageData& data) {
  scoped_ptr<tizen_webview::WrtIpcMessageData> p(new tizen_webview::WrtIpcMessageData);
  p->type = data.type;
  p->value = data.value;
  p->id = data.id;
  p->reference_id = data.reference_id;

  web_view_->SmartCallback<EWebViewCallbacks::WrtPluginsMessage>().call(p.get());
}

void WebContentsDelegateEfl::OnWrtPluginSyncMessage(const tizen_webview::WrtIpcMessageData& data,
                                                    IPC::Message* reply) {
  scoped_ptr<tizen_webview::WrtIpcMessageData> tmp(new tizen_webview::WrtIpcMessageData);
  tmp->type = data.type;
  web_view_->SmartCallback<EWebViewCallbacks::WrtPluginsMessage>().call(tmp.get());
  EwkHostMsg_WrtSyncMessage::WriteReplyParams(reply, tmp->value);
  Send(reply);
}

void WebContentsDelegateEfl::DidFirstVisuallyNonEmptyPaint() {
  web_view_->SmartCallback<EWebViewCallbacks::LoadNonEmptyLayoutFinished>().call();
  web_view_->SmartCallback<EWebViewCallbacks::FrameRendered>().call(0);
}

void WebContentsDelegateEfl::OnGetContentSecurityPolicy(IPC::Message* reply_msg) {
  document_created_ = true;
  if (!pending_content_security_policy_.get()) {
    EwkHostMsg_GetContentSecurityPolicy::WriteReplyParams(reply_msg, std::string(), TW_CSP_DEFAULT_POLICY);
  } else {
    EwkHostMsg_GetContentSecurityPolicy::WriteReplyParams(reply_msg,
        pending_content_security_policy_->policy, pending_content_security_policy_->header_type);
    pending_content_security_policy_.reset();
  }
  Send(reply_msg);
}

void WebContentsDelegateEfl::OnPrintedMetafileReceived(const DidPrintPagesParams& params) {
  base::SharedMemory shared_buf(params.metafile_data_handle, true);
  if (!shared_buf.Map(params.data_size)) {
     NOTREACHED() << "couldn't map";
     return;
  }
  scoped_ptr<printing::PdfMetafileSkia> metafile(new printing::PdfMetafileSkia);
  if (!metafile->InitFromData(shared_buf.memory(), params.data_size)) {
    NOTREACHED() << "Invalid metafile header";
    return;
  }
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&WritePdfDataToFile, metafile.release(), params.filename));
}

void WebContentsDelegateEfl::NavigationEntryCommitted(const LoadCommittedDetails& load_details) {
  int forward_backward_list_count = web_contents_.GetController().GetEntryCount();
  if (forward_backward_list_count != forward_backward_list_count_) {
    web_view_->InvokeBackForwardListChangedCallback();
    forward_backward_list_count_ = forward_backward_list_count;
  }
}

void WebContentsDelegateEfl::RenderProcessGone(base::TerminationStatus status) {
  // See RenderWidgetHostViewEfl::RenderProcessGone.
  if (status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION
      || status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED
      || status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
    web_view_->HandleRendererProcessCrash();
  }
}

bool WebContentsDelegateEfl::AddMessageToConsole(WebContents* source,
                                              int32 level,
                                              const string16& message,
                                              int32 line_no,
                                              const string16& source_id) {
  scoped_ptr<_Ewk_Console_Message> console_message(new _Ewk_Console_Message(level,
                                                                          UTF16ToUTF8(message).c_str(),
                                                                          line_no,
                                                                          source->GetVisibleURL().spec().c_str()));
  web_view_->SmartCallback<EWebViewCallbacks::ConsoleMessage>().call(console_message.get());
  return true;
}

void WebContentsDelegateEfl::RunFileChooser(WebContents* web_contents, const FileChooserParams& params) {
  web_view_->ShowFileChooser(params);
}

content::ColorChooser* WebContentsDelegateEfl::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<ColorSuggestion>& suggestions) {
  ColorChooserEfl* color_chooser_efl = new ColorChooserEfl(*web_contents);
  web_view_->RequestColorPicker(SkColorGetR(color), SkColorGetG(color), SkColorGetB(color), SkColorGetA(color));

  return color_chooser_efl;
}

void WebContentsDelegateEfl::OpenDateTimeDialog(
    ui::TextInputType dialog_type,
    double dialog_value,
    double min,
    double max,
    double step,
    const std::vector<DateTimeSuggestion>& suggestions) {
  int inputPickerType;

  switch (dialog_type) {
    case ui::TEXT_INPUT_TYPE_DATE:
      inputPickerType = TW_INPUT_TYPE_DATE;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
      inputPickerType = TW_INPUT_TYPE_DATETIME;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      inputPickerType = TW_INPUT_TYPE_DATETIMELOCAL;
      break;
    case ui::TEXT_INPUT_TYPE_TIME:
      inputPickerType = TW_INPUT_TYPE_TIME;
      break;
    case ui::TEXT_INPUT_TYPE_WEEK:
      inputPickerType = TW_INPUT_TYPE_WEEK;
      break;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      inputPickerType = TW_INPUT_TYPE_TEXT;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      inputPickerType = TW_INPUT_TYPE_DATETIME;
      break;
    case ui::TEXT_INPUT_TYPE_MONTH:
      inputPickerType = TW_INPUT_TYPE_MONTH;
      break;
    default:
      inputPickerType = TW_INPUT_TYPE_TEXT;
      break;
  }
  web_view_->InputPickerShow(static_cast<tizen_webview::Input_Type>(inputPickerType), dialog_value);
}
} //namespace content
