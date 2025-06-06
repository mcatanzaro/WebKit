/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebFakeXRInputController.h"

#if ENABLE(WEBXR)
#include "ExceptionOr.h"
#include "WebFakeXRDevice.h"
#include "XRHandJoint.h"
#include <ranges>

namespace WebCore {

using InputSource = PlatformXR::FrameData::InputSource;
using InputSourceButton = PlatformXR::FrameData::InputSourceButton;
using InputSourcePose = PlatformXR::FrameData::InputSourcePose;
using ButtonType = FakeXRButtonStateInit::Type;

#if ENABLE(WEBXR_HANDS)
using HandJointsVector = PlatformXR::FrameData::HandJointsVector;
using InputSourceHandJoint = PlatformXR::FrameData::InputSourceHandJoint;
#endif

// https://immersive-web.github.io/webxr-gamepads-module/#xr-standard-gamepad-mapping
constexpr std::array<ButtonType, 5> XR_STANDARD_BUTTONS = { ButtonType::Grip, ButtonType::Touchpad, ButtonType::Thumbstick, ButtonType::OptionalButton, ButtonType::OptionalThumbstick };

Ref<WebFakeXRInputController> WebFakeXRInputController::create(PlatformXR::InputSourceHandle handle, const FakeXRInputSourceInit& init)
{
    return adoptRef(*new WebFakeXRInputController(handle, init));
}

WebFakeXRInputController::WebFakeXRInputController(PlatformXR::InputSourceHandle handle, const FakeXRInputSourceInit& init)
    : m_handle(handle)
    , m_handedness(init.handedness)
    , m_targetRayMode(init.targetRayMode)
    , m_profiles(init.profiles)
    , m_primarySelected(init.selectionStarted)
    , m_simulateSelect(init.selectionClicked)
{
    setPointerOrigin(init.pointerOrigin, false);
    setGripOrigin(init.gripOrigin, false);
    setSupportedButtons(init.supportedButtons);
#if ENABLE(WEBXR_HANDS)
    updateHandJoints(init.handJoints);
#endif
}

void WebFakeXRInputController::setGripOrigin(FakeXRRigidTransformInit gripOrigin, bool emulatedPosition)
{
    auto transform = WebFakeXRDevice::parseRigidTransform(gripOrigin);
    if (transform.hasException())
        return;
    m_gripOrigin = InputSourcePose { transform.releaseReturnValue(), emulatedPosition };
}   

void WebFakeXRInputController::setPointerOrigin(FakeXRRigidTransformInit pointerOrigin, bool emulatedPosition)
{
    auto transform = WebFakeXRDevice::parseRigidTransform(pointerOrigin);
    if (transform.hasException())
        return;
    m_pointerOrigin = { transform.releaseReturnValue(), emulatedPosition };
}

void WebFakeXRInputController::disconnect()
{
    m_connected = false;
}

void WebFakeXRInputController::reconnect()
{
    m_connected = true;
}

void WebFakeXRInputController::setSupportedButtons(const Vector<FakeXRButtonStateInit>& buttons)
{
    m_buttons.clear();
    for (auto& button : buttons)
        m_buttons.add(button.buttonType, button);
}

void WebFakeXRInputController::updateButtonState(const FakeXRButtonStateInit& init)
{
    auto it = m_buttons.find(init.buttonType);
    if (it != m_buttons.end())
        it->value = init;
}

InputSource WebFakeXRInputController::getFrameData()
{
    InputSource state;
    state.handle = m_handle;
    state.handedness = m_handedness;
    state.targetRayMode = m_targetRayMode;
    state.profiles = m_profiles;
    state.pointerOrigin = m_pointerOrigin;
    state.gripOrigin = m_gripOrigin;
#if ENABLE(WEBXR_HANDS)
    state.handJoints = m_handJoints;
#endif

    if (m_simulateSelect)
        m_primarySelected = true;

    // https://immersive-web.github.io/webxr-gamepads-module/#xr-standard-gamepad-mapping
    // Mimic xr-standard gamepad layout

    // Primary trigger is required and must be at index 0
    state.buttons.append({
        .touched = m_primarySelected,
        .pressed = m_primarySelected,
        .pressedValue = m_primarySelected ? 1.0f : 0.0f
    });

    // Next buttons in xr-standard order
    for (auto buttonType : XR_STANDARD_BUTTONS) {
        auto data = getButtonOrPlaceholder(buttonType);
        if (data.button)
            state.buttons.append(*data.button);
        if (data.axes)
            state.axes.appendVector(*data.axes);

    }

    if (m_simulateSelect) {
        m_primarySelected = false;
        m_simulateSelect = false;
    }

    return state;
}

WebFakeXRInputController::ButtonOrPlaceholder WebFakeXRInputController::getButtonOrPlaceholder(FakeXRButtonStateInit::Type buttonType) const
{
    ButtonOrPlaceholder result;

    auto it = m_buttons.find(buttonType);
    if (it != m_buttons.end()) {
        result.button = InputSourceButton {
            .touched = it->value.touched,
            .pressed = it->value.pressed,
            .pressedValue = it->value.pressedValue
        };

        if (buttonType == ButtonType::Touchpad || buttonType == ButtonType::Thumbstick)
            result.axes = Vector<float> { it->value.xValue, it->value.yValue };

    } else {
        // Add a placeholder if needed
        // Devices that lack one of the optional inputs listed in the tables above MUST preserve their place in the
        // buttons or axes array, reporting a placeholder button or placeholder axis, respectively.
        if (buttonType != ButtonType::OptionalButton && buttonType != ButtonType::OptionalThumbstick) {
            size_t priority = std::ranges::find(XR_STANDARD_BUTTONS, buttonType) - XR_STANDARD_BUTTONS.begin();
            ASSERT(priority != XR_STANDARD_BUTTONS.size());

            for (size_t i = priority + 1; i < XR_STANDARD_BUTTONS.size(); ++i) {
                if (m_buttons.contains(XR_STANDARD_BUTTONS[i])) {
                    result.button = InputSourceButton();
                    break;
                }
            }
        }

        if (buttonType == ButtonType::Touchpad && m_buttons.contains(ButtonType::Thumbstick))
            result.axes = Vector<float> { 0.0, 0.0 };
    }

    return result;
}

#if ENABLE(WEBXR_HANDS)
void WebFakeXRInputController::updateHandJoints(const Vector<FakeXRJointStateInit>& handJoints)
{
    if (handJoints.isEmpty() || handJoints.size() != static_cast<size_t>(XRHandJoint::Count)) {
        m_handJoints = std::nullopt;
        return;
    }

    HandJointsVector updatedJoints;
    for (auto handJoint : handJoints) {
        auto transform = WebFakeXRDevice::parseRigidTransform(handJoint.pose);
        if (transform.hasException()) {
            updatedJoints.append(std::nullopt);
            continue;
        }
        
        updatedJoints.append(InputSourceHandJoint { InputSourcePose { transform.releaseReturnValue(), false }, handJoint.radius });
    }
    m_handJoints = WTFMove(updatedJoints);
}
#endif // ENABLE(WEBXR_HANDS)

} // namespace WebCore

#endif // ENABLE(WEBXR)
