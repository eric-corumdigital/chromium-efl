// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEB_CONTENTS_UTILS_H
#define WEB_CONTENTS_UTILS_H

namespace content {
class WebContents;
} // namespace content

namespace web_contents_utils {
    content::WebContents* WebContentsFromViewID(int render_process_id, int render_view_id);
    content::WebContents* WebContentsFromFrameID(int render_process_id, int render_frame_id);
} // namespace web_contents_utils

#endif // WEB_CONTENTS_UTILS_H

