/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)
#include "WebGLDebugShaders.h"

#include "WebGLShader.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebGLDebugShaders);

WebGLDebugShaders::WebGLDebugShaders(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::WebGLDebugShaders)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_translated_shader_source"_s);
}

WebGLDebugShaders::~WebGLDebugShaders() = default;

bool WebGLDebugShaders::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_translated_shader_source"_s);
}

String WebGLDebugShaders::getTranslatedShaderSource(WebGLShader& shader)
{
    if (isContextLost())
        return String();
    Ref context = this->context();
    if (!context->validateWebGLObject("getTranslatedShaderSource"_s, shader))
        return emptyString();
    return context->protectedGraphicsContextGL()->getTranslatedShaderSourceANGLE(shader.object());
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
