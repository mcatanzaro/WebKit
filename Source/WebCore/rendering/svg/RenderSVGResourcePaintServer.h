/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "Color.h"
#include "RenderSVGResourceContainer.h"

namespace WebCore {

class GraphicsContext;
class RenderSVGShape;

class RenderSVGResourcePaintServer : public RenderSVGResourceContainer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGResourcePaintServer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGResourcePaintServer);
public:
    virtual ~RenderSVGResourcePaintServer();

    virtual bool prepareFillOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) { return false; }
    virtual bool prepareStrokeOperation(GraphicsContext&, const RenderLayerModelObject&, const RenderStyle&) { return false; }

protected:
    RenderSVGResourcePaintServer(Type, SVGElement&, RenderStyle&&);
};

using SVGPaintServerOrColor = Variant<std::monostate, RenderSVGResourcePaintServer*, Color>;

}

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGResourcePaintServer, isRenderSVGResourcePaintServer())
