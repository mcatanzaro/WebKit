/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#include "ImageBufferUtilitiesCG.h"

#if USE(CG)

#include "GraphicsContextCG.h"
#include "MIMETypeRegistry.h"
#include "PixelBuffer.h"
#include "UTIUtilities.h"
#include <ImageIO/ImageIO.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ScopedLambda.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using PutBytesCallback = size_t(std::span<const uint8_t>);

uint8_t verifyImageBufferIsBigEnough(std::span<const uint8_t> buffer)
{
    RELEASE_ASSERT(!buffer.empty());

    uintptr_t lastByte;
    bool isSafe = WTF::safeAdd((uintptr_t)buffer.data(), buffer.size() - 1, lastByte);
    RELEASE_ASSERT(isSafe);

    return *(uint8_t*)lastByte;
}

CFStringRef jpegUTI()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if PLATFORM(IOS_FAMILY)
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
#endif
    return kUTTypeJPEG;
ALLOW_DEPRECATED_DECLARATIONS_END
}

RetainPtr<CFStringRef> utiFromImageBufferMIMEType(const String& mimeType)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: Why doesn't iOS use the CoreServices version?
#if PLATFORM(MAC)
    return UTIFromMIMEType(mimeType).createCFString();
#else
    // FIXME: Add Windows support for all the supported UTIs when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG, and GIF. See <rdar://problem/6095286>.
    static CFStringRef kUTTypePNG;
    static CFStringRef kUTTypeGIF;

    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        kUTTypePNG = CFSTR("public.png");
        kUTTypeGIF = CFSTR("com.compuserve.gif");
    });

    if (equalLettersIgnoringASCIICase(mimeType, "image/png"_s))
        return kUTTypePNG;
    if (equalLettersIgnoringASCIICase(mimeType, "image/jpeg"_s) || equalLettersIgnoringASCIICase(mimeType, "image/jpg"_s))
        return jpegUTI();
    if (equalLettersIgnoringASCIICase(mimeType, "image/gif"_s))
        return kUTTypeGIF;

    ASSERT_NOT_REACHED();
    return kUTTypePNG;
#endif
ALLOW_DEPRECATED_DECLARATIONS_END
}

static RetainPtr<CFDictionaryRef> imagePropertiesForDestinationUTIAndQuality(CFStringRef destinationUTI, std::optional<double> quality)
{
    if (CFEqual(destinationUTI, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the JPEG image destination.
        auto compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &*quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        return adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }
    return nullptr;

    // FIXME: Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.
}

static bool encode(CGImageRef image, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    if (!image)
        return false;

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    if (!destinationUTI)
        return false;

    CGDataConsumerCallbacks callbacks {
        [](void* context, const void* buffer, size_t count) -> size_t {
            auto functor = *static_cast<const ScopedLambda<PutBytesCallback>*>(context);
            return functor(unsafeMakeSpan(static_cast<const uint8_t*>(buffer), count));
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(const_cast<ScopedLambda<PutBytesCallback>*>(&function), &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI.get(), 1, nullptr));
    
    auto imageProperties = imagePropertiesForDestinationUTIAndQuality(destinationUTI.get(), quality);
    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());

    return CGImageDestinationFinalize(destination.get());
}

static bool encode(const PixelBuffer& source, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;
    
    auto data = source.bytes();
    auto dataSize = data.size();

    Vector<uint8_t> premultipliedData;

    if (CFEqual(destinationUTI.get(), jpegUTI())) {
        // FIXME: Use PixelBufferConversion for this once it supports RGBX.

        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        if (!premultipliedData.tryReserveCapacity(dataSize))
            return false;

        premultipliedData.grow(dataSize);
        for (size_t i = 0; i < dataSize; i += 4) {
            unsigned alpha = data[i + 3];
            if (alpha != 255) {
                premultipliedData[i + 0] = data[i + 0] * alpha / 255;
                premultipliedData[i + 1] = data[i + 1] * alpha / 255;
                premultipliedData[i + 2] = data[i + 2] * alpha / 255;
            } else {
                premultipliedData[i + 0] = data[i + 0];
                premultipliedData[i + 1] = data[i + 1];
                premultipliedData[i + 2] = data[i + 2];
            }
        }

        dataAlphaInfo = kCGImageAlphaNoneSkipLast; // Ignore the alpha channel.
        data = premultipliedData.mutableSpan();
    }

    verifyImageBufferIsBigEnough(data);

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, data.data(), dataSize, nullptr));
    if (!dataProvider)
        return false;

    auto imageSize = source.size();
    auto image = adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), 8, 32, 4 * imageSize.width(), source.format().colorSpace.platformColorSpace(), static_cast<uint32_t>(kCGBitmapByteOrderDefault) | static_cast<uint32_t>(dataAlphaInfo), dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    return encode(image.get(), mimeType, quality, function);
}

static bool encode(std::span<const uint8_t> data, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    if (data.empty())
        return false;

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    if (!destinationUTI)
        return false;

    RetainPtr cfData = toCFDataNoCopy(data, kCFAllocatorNull);
    if (!cfData)
        return false;

    auto source = adoptCF(CGImageSourceCreateWithData(cfData.get(), nullptr));
    if (!source)
        return false;

    CGDataConsumerCallbacks callbacks {
        [](void* context, const void* buffer, size_t count) -> size_t {
            auto functor = *static_cast<const ScopedLambda<PutBytesCallback>*>(context);
            return functor(unsafeMakeSpan(static_cast<const uint8_t*>(buffer), count));
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(const_cast<ScopedLambda<PutBytesCallback>*>(&function), &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI.get(), 1, nullptr));

    auto imageProperties = imagePropertiesForDestinationUTIAndQuality(destinationUTI.get(), quality);
    CGImageDestinationAddImageFromSource(destination.get(), source.get(), 0, nullptr);

    return CGImageDestinationFinalize(destination.get());
}

template<typename Source> static Vector<uint8_t> encodeToVector(Source&& source, const String& mimeType, std::optional<double> quality)
{
    Vector<uint8_t> result;

    bool success = encode(std::forward<Source>(source), mimeType, quality, scopedLambdaRef<PutBytesCallback>([&] (std::span<const uint8_t> data) {
        result.append(data);
        return data.size();
    }));
    if (!success)
        return { };

    return result;
}

template<typename Source> static String encodeToDataURL(Source&& source, const String& mimeType, std::optional<double> quality)
{
    // FIXME: This could be done more efficiently with a streaming base64 encoder.

    auto encodedData = encodeToVector(std::forward<Source>(source), mimeType, quality);
    if (encodedData.isEmpty())
        return "data:,"_s;

    return makeString("data:"_s, mimeType, ";base64,"_s, base64Encoded(encodedData));
}

Vector<uint8_t> encodeData(CGImageRef image, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(image, mimeType, quality);
}

Vector<uint8_t> encodeData(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(pixelBuffer, mimeType, quality);
}

Vector<uint8_t> encodeData(std::span<const uint8_t> data, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(data, mimeType, quality);
}

String dataURL(CGImageRef image, const String& mimeType, std::optional<double> quality)
{
    return encodeToDataURL(image, mimeType, quality);
}

String dataURL(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    return encodeToDataURL(pixelBuffer, mimeType, quality);
}

} // namespace WebCore

#endif // USE(CG)
