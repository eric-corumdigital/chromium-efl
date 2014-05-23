/*
    Copyright (C) 2013 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef GEOLOCATION_PERMISSION_CONTEXT_EFL_H
#define GEOLOCATION_PERMISSION_CONTEXT_EFL_H

#include "content/public/browser/geolocation_permission_context.h"

namespace content {

class BrowserContext;

class GeolocationPermissionContextEfl : public GeolocationPermissionContext {
public:
    GeolocationPermissionContextEfl() { }

    virtual void RequestGeolocationPermission(int, int, int, const GURL&, base::Callback<void(bool)>) OVERRIDE;

    // The renderer is cancelling a pending permission request.
    virtual void CancelGeolocationPermissionRequest(int, int, int, const GURL&) OVERRIDE;

private:
    void RequestGeolocationPermissionOnUIThread(int, int, int, const GURL&, base::Callback<void(bool)>);
};

} // namespace
#endif // GEOLOCATION_PERMISSION_CONTEXT_EFL_H
