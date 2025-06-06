/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "TestsController.h"

#if ENABLE(WPE_PLATFORM)
#include <wpe/headless/wpe-headless.h>
#endif

#if ENABLE(WPE_PLATFORM)
static bool useWPEPlatformAPI(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!g_strcmp0(argv[i], "--wpe-legacy-api"))
            return false;
    }
    return true;
}
#endif

int main(int argc, char** argv)
{
#if ENABLE(WPE_PLATFORM)
    WPEDisplay* display = nullptr;
    if (useWPEPlatformAPI(argc, argv))
        display = wpe_display_headless_new();
#endif

    bool passed = TestWebKitAPI::TestsController::singleton().run(argc, argv);

#if ENABLE(WPE_PLATFORM)
    g_clear_object(&display);
#endif

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
