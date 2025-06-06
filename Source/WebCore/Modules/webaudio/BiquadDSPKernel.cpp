/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "BiquadDSPKernel.h"

#include "AudioArray.h"
#include "AudioUtilities.h"
#include "Biquad.h"
#include "BiquadProcessor.h"
#include "FloatConversion.h"
#include <limits.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if CPU(X86_SSE2)
#include <immintrin.h>
#endif

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BiquadDSPKernel);

static bool hasConstantValue(std::span<float> values)
{
    // Load the initial value
    const float value = values[0];
    // This initialization ensures that we correctly handle the first frame and
    // start the processing from the second frame onwards, effectively excluding
    // the first frame from the subsequent comparisons in the non-SIMD paths
    // it guarantees that we don't redundantly compare the first frame again
    // during the loop execution.
    size_t processedFrames = 1;

#if CPU(X86_SSE2)
    // Process 4 floats at a time using SIMD.
    __m128 valueVec = _mm_set1_ps(value);
    // Start at 0 for byte alignment
    for (processedFrames = 0; processedFrames < values.size() - 3; processedFrames += 4) {
        // Load 4 floats from memory.
        __m128 inputVec = _mm_loadu_ps(&values[processedFrames]);
        // Compare the 4 floats with the value.
        __m128 cmpVec = _mm_cmpneq_ps(inputVec, valueVec);
        // Check if any of the floats are not equal to the value.
        if (_mm_movemask_ps(cmpVec))
            return false;
    }
#elif HAVE(ARM_NEON_INTRINSICS)
    // Process 4 floats at a time using SIMD.
    float32x4_t valueVec = vdupq_n_f32(value);
    // Start at 0 for byte alignment.
    for (processedFrames = 0; processedFrames < values.size() - 3; processedFrames += 4) {
        // Load 4 floats from memory.
        float32x4_t inputVec = vld1q_f32(&values[processedFrames]);
        // Compare the 4 floats with the value.
        uint32x4_t cmpVec = vceqq_f32(inputVec, valueVec);
        // Accumulate the elements of the cmpVec vector using bitwise AND.
        uint32x2_t cmpReduced32 = vand_u32(vget_low_u32(cmpVec), vget_high_u32(cmpVec));
        // Check if any of the floats are not equal to the value.
        if (!vget_lane_u32(vpmin_u32(cmpReduced32, cmpReduced32), 0))
            return false;
    }
#endif
    // Fallback implementation without SIMD optimization.
    while (processedFrames < values.size()) {
        if (values[processedFrames] != value)
            return false;
        ++processedFrames;
    }
    return true;
}

void BiquadDSPKernel::updateCoefficientsIfNecessary(size_t framesToProcess)
{
    if (biquadProcessor()->filterCoefficientsDirty()) {
        if (biquadProcessor()->hasSampleAccurateValues() && biquadProcessor()->shouldUseARate()) {
            // Use float arrays instead of AudioFloatArray to avoid heap allocations on the audio thread.
            std::array<float, AudioUtilities::renderQuantumSize> cutoffFrequency;
            std::array<float, AudioUtilities::renderQuantumSize> q;
            std::array<float, AudioUtilities::renderQuantumSize> gain;
            std::array<float, AudioUtilities::renderQuantumSize> detune; // in Cents

            RELEASE_ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

            auto cutoffFrequencySpan = std::span { cutoffFrequency }.first(framesToProcess);
            auto qSpan = std::span { q }.first(framesToProcess);
            auto gainSpan = std::span { gain }.first(framesToProcess);
            auto detuneSpan = std::span { detune }.first(framesToProcess);

            biquadProcessor()->parameter1().calculateSampleAccurateValues(cutoffFrequencySpan);
            biquadProcessor()->parameter2().calculateSampleAccurateValues(qSpan);
            biquadProcessor()->parameter3().calculateSampleAccurateValues(gainSpan);
            biquadProcessor()->parameter4().calculateSampleAccurateValues(detuneSpan);

            // If all the values are actually constant for this render (or the
            // automation rate is "k-rate" for all of the AudioParams), we don't need
            // to compute filter coefficients for each frame since they would be the
            // same as the first.
            bool isConstant = hasConstantValue(cutoffFrequencySpan)
                && hasConstantValue(qSpan)
                && hasConstantValue(gainSpan)
                && hasConstantValue(detuneSpan);

            updateCoefficients(isConstant ? 1 : framesToProcess, cutoffFrequency, q, gain, detune);
        } else {
            float cutoffFrequency = biquadProcessor()->parameter1().finalValue();
            float q = biquadProcessor()->parameter2().finalValue();
            float gain = biquadProcessor()->parameter3().finalValue();
            float detune = biquadProcessor()->parameter4().finalValue();
            updateCoefficients(1, singleElementSpan(cutoffFrequency), singleElementSpan(q), singleElementSpan(gain), singleElementSpan(detune));
        }
    }
}

void BiquadDSPKernel::updateCoefficients(size_t numberOfFrames, std::span<const float> cutoffFrequency, std::span<const float> q, std::span<const float> gain, std::span<const float> detune)
{
    // Convert from Hertz to normalized frequency 0 -> 1.
    double nyquist = this->nyquist();

    m_biquad.setHasSampleAccurateValues(numberOfFrames > 1);

    for (size_t k = 0; k < numberOfFrames; ++k) {
        double normalizedFrequency = cutoffFrequency[k] / nyquist;

        // Offset frequency by detune.
        if (detune[k]) {
            // Detune multiplies the frequency by 2^(detune[k] / 1200).
            normalizedFrequency *= std::exp2(detune[k] / 1200);
        }

        // Configure the biquad with the new filter parameters for the appropriate
        // type of filter.
        switch (biquadProcessor()->type()) {
        case BiquadFilterType::Lowpass:
            m_biquad.setLowpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Highpass:
            m_biquad.setHighpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Bandpass:
            m_biquad.setBandpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Lowshelf:
            m_biquad.setLowShelfParams(k, normalizedFrequency, gain[k]);
            break;

        case BiquadFilterType::Highshelf:
            m_biquad.setHighShelfParams(k, normalizedFrequency, gain[k]);
            break;

        case BiquadFilterType::Peaking:
            m_biquad.setPeakingParams(k, normalizedFrequency, q[k], gain[k]);
            break;

        case BiquadFilterType::Notch:
            m_biquad.setNotchParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Allpass:
            m_biquad.setAllpassParams(k, normalizedFrequency, q[k]);
            break;
        }
    }

    ASSERT(numberOfFrames);
    updateTailTime(numberOfFrames - 1);
}

void BiquadDSPKernel::process(std::span<const float> source, std::span<float> destination)
{
    ASSERT(source.data() && destination.data() && biquadProcessor());
    
    // Recompute filter coefficients if any of the parameters have changed.
    // FIXME: as an optimization, implement a way that a Biquad object can simply copy its internal filter coefficients from another Biquad object.
    // Then re-factor this code to only run for the first BiquadDSPKernel of each BiquadProcessor.

    updateCoefficientsIfNecessary(source.size());

    m_biquad.process(source, destination);
}

void BiquadDSPKernel::getFrequencyResponse(unsigned nFrequencies, std::span<const float> frequencyHz, std::span<float> magResponse, std::span<float> phaseResponse)
{
    bool isGood = nFrequencies > 0 && frequencyHz.data() && magResponse.data() && phaseResponse.data();
    ASSERT(isGood);
    if (!isGood)
        return;

    Vector<float> frequency(nFrequencies);

    double nyquist = this->nyquist();

    // Convert from frequency in Hz to normalized frequency (0 -> 1),
    // with 1 equal to the Nyquist frequency.
    for (unsigned k = 0; k < nFrequencies; ++k)
        frequency[k] = frequencyHz[k] / nyquist;

    m_biquad.getFrequencyResponse(nFrequencies, frequency.span(), magResponse, phaseResponse);
}

double BiquadDSPKernel::tailTime() const
{
    return m_tailTime;
}

double BiquadDSPKernel::latencyTime() const
{
    return 0;
}

void BiquadDSPKernel::updateTailTime(size_t coefIndex)
{
    // A reasonable upper limit for the tail time. While it's easy to
    // create biquad filters whose tail time can be much larger than
    // this, limit the maximum to this value so that we don't keep such
    // nodes alive "forever".
    // TODO: What is a reasonable upper limit?
    constexpr double maxTailTime = 30;

    double sampleRate = this->sampleRate();
    double tail = m_biquad.tailFrame(coefIndex, maxTailTime * sampleRate) / sampleRate;

    m_tailTime = std::clamp(tail, 0.0, maxTailTime);
}

bool BiquadDSPKernel::requiresTailProcessing() const
{
    // Always return true even if the tail time and latency might both
    // be zero. This is for simplicity and because TailTime() is 0
    // basically only when the filter response H(z) = 0 or H(z) = 1. And
    // it's ok to return true. It just means the node lives a little
    // longer than strictly necessary.
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
