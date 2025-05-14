/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebColorPickerGtk.h"

#include "WebPageProxy.h"
#include <WebCore/Color.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <glib/gi18n-lib.h>

namespace WebKit {
using namespace WebCore;

Ref<WebColorPickerGtk> WebColorPickerGtk::create(WebPageProxy& page, const Color& initialColor, const IntRect& rect)
{
    return adoptRef(*new WebColorPickerGtk(page, initialColor, rect));
}

WebColorPickerGtk::WebColorPickerGtk(WebPageProxy& page, const Color& initialColor, const IntRect&)
    : WebColorPicker(&page.colorPickerClient())
    , m_initialColor(initialColor)
    , m_webView(page.viewWidget())
#if GTK_CHECK_VERSION(4, 10, 0)
    , m_colorDialog(adoptGRef(gtk_color_dialog_new()))
    , m_cancellable(adoptGRef(g_cancellable_new()))
#else
    , m_colorChooser(nullptr)
#endif
{
}

WebColorPickerGtk::~WebColorPickerGtk()
{
#if GTK_CHECK_VERSION(4, 10, 0)
    g_cancellable_cancel(m_cancellable.get());
#endif

    endPicker();
}

void WebColorPickerGtk::cancel()
{
    setSelectedColor(m_initialColor);
}

void WebColorPickerGtk::endPicker()
{
#if !GTK_CHECK_VERSION(4, 10, 0)
    g_clear_pointer(&m_colorChooser, gtk_widget_destroy);
#endif

    WebColorPicker::endPicker();
}

void WebColorPickerGtk::didChooseColor(const Color& color)
{
    if (CheckedPtr client = this->client())
        client->didChooseColor(color);
}

#if GTK_CHECK_VERSION(4, 10, 0)
void WebColorPickerGtk::colorDialogChooseCallback(GtkColorDialog* colorDialog, GAsyncResult* result, WebColorPickerGtk* colorPicker)
{
    GUniqueOutPtr<GError> error;
    GUniquePtr<GdkRGBA> rgba(gtk_color_dialog_choose_rgba_finish(colorDialog, result, &error.outPtr()));
    if (error) {
        if (!g_error_matches(error.get(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) && !g_error_matches(error.get(), GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
            g_warning("Failed to run color dialog: %s", error->message);
        colorPicker->cancel();
    } else
        colorPicker->didChooseColor(*rgba.get());
    colorPicker->endPicker();
}
#else
void WebColorPickerGtk::colorChooserDialogRGBAChangedCallback(GtkColorChooser* colorChooser, GParamSpec*, WebColorPickerGtk* colorPicker)
{
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(colorChooser, &rgba);
    colorPicker->didChooseColor(rgba);
}

void WebColorPickerGtk::colorChooserDialogResponseCallback(GtkColorChooser*, int responseID, WebColorPickerGtk* colorPicker)
{
    if (responseID != GTK_RESPONSE_OK)
        colorPicker->cancel();
    colorPicker->endPicker();
}
#endif

void WebColorPickerGtk::showColorPicker(const Color& color)
{
    if (!client())
        return;

    m_initialColor = color;

#if GTK_CHECK_VERSION(4, 10, 0)
    GtkWidget* toplevel = gtk_widget_get_toplevel(m_webView);
    gtk_color_dialog_choose_rgba(m_colorDialog.get(), toplevel ? GTK_WINDOW(toplevel) : nullptr, &m_initialColor, m_cancellable.get(), reinterpret_cast<GAsyncReadyCallback>(WebColorPickerGtk::colorDialogChooseCallback), this);
#else
    if (!m_colorChooser) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(m_webView);
        m_colorChooser = gtk_color_chooser_dialog_new(_("Select Color"), WebCore::widgetIsOnscreenToplevelWindow(toplevel) ? GTK_WINDOW(toplevel) : nullptr);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorChooser), &m_initialColor);
        g_signal_connect(m_colorChooser, "notify::rgba", G_CALLBACK(WebColorPickerGtk::colorChooserDialogRGBAChangedCallback), this);
        g_signal_connect(m_colorChooser, "response", G_CALLBACK(WebColorPickerGtk::colorChooserDialogResponseCallback), this);
    } else
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorChooser), &m_initialColor);

    gtk_widget_show(m_colorChooser);
#endif
}

} // namespace WebKit
