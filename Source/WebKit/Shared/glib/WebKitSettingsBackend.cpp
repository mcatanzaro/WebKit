/*
 * Copyright 2026 Red Hat
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
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
#include "WebKitSettingsBackend.h"

#include "AuxiliaryProcess.h"
#include "GSettingsProviderMessages.h"
#include "GSettingsValue.h"
#include <gio/gio.h>
#include <limits>
#include <wtf/CheckedPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CStringView.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RuntimeApplicationChecks.h>

// FIXME: need runtime application check somewhere else to double-check this is not broken.

// FIXME
using namespace WebKit;

// WebKitSettingsBackend is a GSettings backend that proxies everything from an
// auxiliary process to the UI process. The goal is to reduce precious inotify
// instances by centralizing settings monitoring in the UI process, so we do not
// use GKeyfileSettingsBackend except in the UI process.
//
// https://gitlab.gnome.org/GNOME/epiphany/-/issues/1454

static ASCIILiteral backendName = "WebKit"_s;

class WebKitSettingsBackendMessageReceiver : public IPC::MessageReceiver
{
};

struct _WebKitSettingsBackendPrivate {
    WTF_MAKE_TZONE_ALLOCATED(_WebKitSettingsBackendPrivate);
public:
    _WebKitSettingsBackendPrivate() {
        RELEASE_ASSERT(isInAuxiliaryProcess());
    }

    CheckedPtr<AuxiliaryProcess> auxiliaryProcess;
// FIXME
//    NeverDestroyed<WebKitSettingsBackendMessageReceiver> messageReceiver;
};

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE (WebKitSettingsBackend, webkit_settings_backend, G_TYPE_SETTINGS_BACKEND, GSettingsBackend,
    g_io_extension_point_register(G_SETTINGS_BACKEND_EXTENSION_POINT_NAME);
    g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME, g_define_type_id, backendName, isInAuxiliaryProcess() ? std::numeric_limits<int>::max() : 0)
)

void webkitSettingsBackendRegister(AuxiliaryProcess& process)
{
    if (const char *backend = g_getenv("GSETTINGS_BACKEND"))
        if (CStringView::unsafeFromUTF8(backend) != backendName)
            return;

    g_type_ensure(WEBKIT_TYPE_SETTINGS_BACKEND);
    GRefPtr backend = g_settings_backend_get_default();
    RELEASE_ASSERT(WEBKIT_IS_SETTINGS_BACKEND(backend.get()));

    WebKitSettingsBackendPrivate *priv = WEBKIT_SETTINGS_BACKEND(backend.get())->priv;
    priv->auxiliaryProcess = &process;

// FIXME
//    process.addMessageReceiver(Messages::WebKitSettingsBackend::messageReceiverName(), priv->);
}

static GVariant* webkitSettingsBackendRead(GSettingsBackend* backend, const gchar* key, const GVariantType* expectedType, gboolean defaultValue)
{
// FIXME: to reduce sync IPC, implement a local cache and change notifications
WTFLogAlways("Trying to read a GSetting!");
    WebKitSettingsBackendPrivate *priv = WEBKIT_SETTINGS_BACKEND(backend)->priv;
    auto fixmeWhatsThis = priv->auxiliaryProcess->parentProcessConnection()->sendSync(Messages::GSettingsProvider::Read(String::fromUTF8(key), String::fromUTF8(g_variant_type_peek_string(expectedType)), defaultValue), 0);
WTFLogAlways("Received a response from UI process? I think?");
    return nullptr;
}

static gboolean webkitSettingsBackendGetWritable(GSettingsBackend* backend, const gchar* name)
{
// FIXME
  return TRUE;
}

static gboolean webkitSettingsBackendWrite(GSettingsBackend* backend, const gchar* key, GVariant* value, gpointer originTag)
{
  // FIXME
  return TRUE;
}

static gboolean webkitSettingsBackendWriteTree(GSettingsBackend* backend, GTree* tree, gpointer originTag)
{
  // FIXME
  //g_tree_foreach (tree, g_memory_settings_backend_write_one, backend);
  //g_settings_backend_changed_tree (backend, tree, origin_tag);

  return TRUE;
}

static void webkitSettingsBackendReset(GSettingsBackend* backend, const gchar* key, gpointer originTag)
{
  // FIXME
}

static GPermission* webkitSettingsBackendGetPermission(GSettingsBackend* backend, const gchar* path)
{
// FIXME
  return g_simple_permission_new(TRUE);
}

static void webkit_settings_backend_class_init(WebKitSettingsBackendClass* klass)
{
    GSettingsBackendClass* settingsBackendClass = G_SETTINGS_BACKEND_CLASS(klass);

    settingsBackendClass->read = webkitSettingsBackendRead;
    settingsBackendClass->get_writable = webkitSettingsBackendGetWritable;
    settingsBackendClass->write = webkitSettingsBackendWrite;
    settingsBackendClass->write_tree = webkitSettingsBackendWriteTree;
    settingsBackendClass->reset = webkitSettingsBackendReset;
    //FIXME settingsBackendClass->subscribe = webkitSettingsBackendSubscribe;
    //FIXME settingsBackendClass->unsubscribe = webkitSettingsBackendUnsubscribe;
    //FIXME settingsBackendClass->sync = webkitSettingsBackendSync;
    settingsBackendClass->get_permission = webkitSettingsBackendGetPermission;
    //FIXME settingsBackendClass->read_user_value = webkitSettingsBackendReadUserValue;
}
