// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utc_blink_ewk_base.h"

class utc_blink_ewk_view_notification_closed : public utc_blink_ewk_base
{
 protected:
  utc_blink_ewk_view_notification_closed()
    : closed(false)
    , old_notification(NULL)
  {}

  /* Callback for notification permission request */
  static Eina_Bool notificationPermissionRequest(Evas_Object* webview, Ewk_Notification_Permission_Request* request, void* data)
  {
    utc_message("[notificationPermissionRequest] :: \n");
    assert(data != NULL);
    utc_blink_ewk_view_notification_closed* owner = NULL;
    OwnerFromVoid(data, &owner);
    if (!request) {
      owner->EventLoopStop(Failure);
      return EINA_FALSE;
    }

    //allow the notification
    ewk_notification_permission_reply(request, EINA_TRUE);
  }

  /* Callback for "notification,show" */
  static void notificationShow(void* data, Evas_Object* webview, void* event_info)
  {
    utc_message("[notificationShow] :: \n");
    if(!data)
      utc_fail();
    utc_blink_ewk_view_notification_closed* owner=static_cast<utc_blink_ewk_view_notification_closed*>(data);

    Ewk_Context* context = ewk_view_context_get(webview);
    if (!event_info || !context)
      utc_fail();

    owner->old_notification = static_cast<Ewk_Notification*>(event_info);
    uint64_t notification_id = ewk_notification_id_get(owner->old_notification);
    ewk_notification_showed(context, notification_id);
    owner->EventLoopStop(Success);
  }

  static void titleChanged(void* data, Evas_Object* webview, void* event_info)
  {
    const char* title = static_cast<const char*>(event_info);
    utc_message("[titleChanged] :: title = %s", title);
    utc_blink_ewk_view_notification_closed* owner = NULL;
    OwnerFromVoid(data, &owner);
    if (title && strcmp(title, "notification_closed") == 0) {
      owner->closed = true;
      owner->EventLoopStop(Success);
    }
  }

  /* Startup function */
  void PostSetUp()
  {
    old_notification=0;
    ewk_view_notification_permission_callback_set(GetEwkWebView(), notificationPermissionRequest, this);
    evas_object_smart_callback_add(GetEwkWebView(), "notification,show", notificationShow, this);
    evas_object_smart_callback_add(GetEwkWebView(), "title,changed", titleChanged, this);
  }

  /* Cleanup function */
  void PreTearDown()
  {
    ewk_view_notification_permission_callback_set(GetEwkWebView(), NULL, NULL);
    evas_object_smart_callback_del(GetEwkWebView(), "notification,show", notificationShow);
    evas_object_smart_callback_del(GetEwkWebView(), "title,changed", titleChanged);
  }

protected:
  static const char* const sample;
  bool closed;
  Ewk_Notification* old_notification;
};

const char*const utc_blink_ewk_view_notification_closed::sample="common/sample_notification_2.html";

/**
* @brief Positive test case for ewk_notification_showed()
*/
TEST_F(utc_blink_ewk_view_notification_closed, POS_TEST)
{
  std::string resource_url = GetResourceUrl(sample);
  ASSERT_EQ(EINA_TRUE, ewk_view_url_set(GetEwkWebView(), resource_url.c_str()));
  ASSERT_EQ(Success, EventLoopStart());

  Eina_List *list = NULL;
  list =  eina_list_append(list, old_notification);
  ASSERT_EQ(EINA_TRUE, ewk_view_notification_closed(GetEwkWebView(), list));
  ASSERT_EQ(Success, EventLoopStart());
  ASSERT_TRUE(closed);
}

/**
* @brief Checking whether function works properly in case of NULL value pass
*/
TEST_F(utc_blink_ewk_view_notification_closed, NEG_TEST)
{
  ewk_view_notification_closed(NULL, 0);
  /* If NULL argument passing won't give segmentation fault negative test case will pass */
  utc_pass();
}
