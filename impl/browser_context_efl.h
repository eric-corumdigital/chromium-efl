// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_CONTEXT_EFL
#define BROWSER_CONTEXT_EFL

#include <vector>

#include "url_request_context_getter_efl.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "browser/download_manager_delegate_efl.h"
#include "browser/geolocation/geolocation_permission_context_efl.h"
#include "browser/notification/notification_controller_efl.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_context.h"

class CookieManager;
class EWebContext;

namespace visitedlink {
class VisitedLinkMaster;
}

typedef std::map<std::string, std::string> HTTPCustomHeadersEflMap;

namespace content {

class BrowserContextEfl
  : public BrowserContext,
    public visitedlink::VisitedLinkDelegate {
 public:
  class ResourceContextEfl : public ResourceContext {
   public:
    ResourceContextEfl();
    virtual ~ResourceContextEfl();

    bool HTTPCustomHeaderAdd(const char* name, const char* value);
    bool HTTPCustomHeaderRemove(const char* name);
    void HTTPCustomHeaderClear();
    const HTTPCustomHeadersEflMap GetHTTPCustomHeadersEflMap() const;


    virtual net::HostResolver* GetHostResolver() override;
    virtual net::URLRequestContext* GetRequestContext() override;

    void set_url_request_context_getter(
        scoped_refptr<URLRequestContextGetterEfl> getter);

    base::WeakPtr<CookieManager> GetCookieManager() const;

#if defined(ENABLE_NOTIFICATIONS)
    scoped_refptr<NotificationControllerEfl> GetNotificationController() const;
    void set_notification_controller_efl(
        const scoped_refptr<NotificationControllerEfl> &controller);
#endif

   private:
    scoped_refptr<URLRequestContextGetterEfl> getter_;
    HTTPCustomHeadersEflMap http_custom_headers_;
    mutable base::Lock http_custom_headers_lock_;
#if defined(ENABLE_NOTIFICATIONS)
    scoped_refptr<NotificationControllerEfl> notification_controller_efl_;
#endif

    DISALLOW_COPY_AND_ASSIGN(ResourceContextEfl);
  };

  BrowserContextEfl(EWebContext*, bool incognito = false);
  ~BrowserContextEfl();

  virtual bool IsOffTheRecord() const override { return incognito_; }
  virtual net::URLRequestContextGetter* GetRequestContext() override;
  URLRequestContextGetterEfl* GetRequestContextEfl()
  { return request_context_getter_.get(); }
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(int) override
  { return GetRequestContext(); }
  virtual net::URLRequestContextGetter* GetMediaRequestContext() override
  { return GetRequestContext(); }
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(int) override
  { return GetRequestContext(); }
  virtual net::URLRequestContextGetter* GetMediaRequestContextForStoragePartition(
      const base::FilePath&, bool) override
  { return GetRequestContext(); }

  // These methods map to Add methods in visitedlink::VisitedLinkMaster.
  void AddVisitedURLs(const std::vector<GURL>& urls);
  // visitedlink::VisitedLinkDelegate implementation.
  virtual void RebuildTable(
      const scoped_refptr<URLEnumerator>& enumerator) override;
  // Reset visitedlink master and initialize it.
  void InitVisitedLinkMaster();

  virtual ResourceContext* GetResourceContext() override;

  virtual content::DownloadManagerDelegate* GetDownloadManagerDelegate() override
  { return &download_manager_delegate_; }

  virtual BrowserPluginGuestManager* GetGuestManager() override
  { return 0; }
  virtual ResourceContextEfl* GetResourceContextEfl();

  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override
  { return 0; }

  virtual PushMessagingService* GetPushMessagingService() override
  { return 0; }

  virtual const GeolocationPermissionContextEfl&
      GetGeolocationPermissionContext() const;

  virtual base::FilePath GetPath() const override;

  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors);
  void SetCertificate(const char* certificate_file);
  EWebContext* WebContext() const
  { return web_context_; }

#if defined(ENABLE_NOTIFICATIONS)
  scoped_refptr<content::NotificationControllerEfl>
      GetNotificationController() const;
#endif

 private:
  static void ReadCertificateAndAdd(base::FilePath* file_path);
  virtual SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  mutable scoped_ptr<GeolocationPermissionContextEfl>
      geolocation_permission_context_;
  scoped_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  ResourceContextEfl* resource_context_;
  scoped_refptr<URLRequestContextGetterEfl> request_context_getter_;
  EWebContext* web_context_;
#if defined(ENABLE_NOTIFICATIONS)
  scoped_refptr<NotificationControllerEfl> notification_controller_efl_;
#endif
  DownloadManagerDelegateEfl download_manager_delegate_;
  base::ScopedTempDir temp_dir_;
  bool temp_dir_creation_attempted_;
  const bool incognito_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextEfl);
};

}

#endif
