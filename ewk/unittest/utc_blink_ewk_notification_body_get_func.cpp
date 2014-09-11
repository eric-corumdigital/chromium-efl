/*
 * chromium EFL
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "utc_blink_ewk_base.h"

class utc_blink_ewk_notification_body_get : public utc_blink_ewk_base {
protected:
  /* Startup function */
  virtual void PostSetUp()
  {
    evas_object_smart_callback_add(GetEwkWebView(), "notification,permission,request", notificationPermissionRequest, this);
    evas_object_smart_callback_add(GetEwkWebView(), "notification,show", notificationShow, this);
  }

  /* Cleanup function */
  virtual void PreTearDown()
  {
    evas_object_smart_callback_del(GetEwkWebView(), "notification,permission,request", notificationPermissionRequest);
    evas_object_smart_callback_del(GetEwkWebView(), "notification,show", notificationShow);
  }

  /* Callback for "notification,permission,request" */
  static void notificationPermissionRequest(void* data, Evas_Object* webview, void* event_info)
  {
    utc_message("[notificationPermissionRequest] :: ");

    if (!event_info)
      utc_fail();

    //allow the notification
    ewk_notification_permission_request_set((Ewk_Notification_Permission_Request*)event_info, EINA_TRUE);
  }

  /* Callback for "notification,show" */
  static void notificationShow(void* data, Evas_Object* webview, void* event_info)
  {
    utc_message("[notificationShow] :: ");

    if (!event_info) {
      utc_fail();
    }

    if (!data) {
      utc_fail();
    }

    utc_blink_ewk_notification_body_get* owner = static_cast<utc_blink_ewk_notification_body_get*>(data);

    //call ewk_notification API
    const char* body = ewk_notification_body_get((Ewk_Notification*)event_info);
    if (strcmp(body, owner->notification_body_ref) == 0) {
      owner->EventLoopStop(Success);
    } else {
      owner->EventLoopStop(Failure);
    }
  }

protected:
  static const char* const resource_relative_path;
  static const char* const notification_body_ref;
};

const char* const utc_blink_ewk_notification_body_get::resource_relative_path = "/common/sample_notification_1.html";
const char* const utc_blink_ewk_notification_body_get::notification_body_ref = "Notification body content";

/**
* @brief Positive test case for ewk_notification_body_get(). Text returned by api is compared against expected text and result is set in notificationShow()
*/
TEST_F(utc_blink_ewk_notification_body_get, POS_TEST)
{
  std::string resource_url = GetResourceUrl(resource_relative_path);
  Eina_Bool result = ewk_view_url_set(GetEwkWebView(), resource_url.c_str());

  MainLoopResult loop_result = EventLoopStart();

  if (loop_result != Success) {
    FAIL();
  }
}

/**
* @brief Checking whether function works properly in case of NULL of a webview.
*/
TEST_F(utc_blink_ewk_notification_body_get, NEG_TEST)
{
  const char *bodyText = ewk_notification_body_get(NULL);
  utc_check_eq(NULL, bodyText);
}