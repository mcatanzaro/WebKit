/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WPEToplevelWayland_h
#define WPEToplevelWayland_h

#if !defined(__WPE_WAYLAND_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wayland/wpe-wayland.h> can be included directly."
#endif

#include <glib-object.h>
#include <wayland-client.h>
#include <wpe/wpe-platform.h>
#include <wpe/wayland/WPEDisplayWayland.h>

G_BEGIN_DECLS

#define WPE_TYPE_TOPLEVEL_WAYLAND (wpe_toplevel_wayland_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEToplevelWayland, wpe_toplevel_wayland, WPE, TOPLEVEL_WAYLAND, WPEToplevel)

WPE_API WPEToplevel       *wpe_toplevel_wayland_new            (WPEDisplayWayland  *display,
                                                                guint               max_views);
WPE_API struct wl_surface *wpe_toplevel_wayland_get_wl_surface (WPEToplevelWayland *toplevel);

G_END_DECLS

#endif /* WPEToplevelWayland_h */
