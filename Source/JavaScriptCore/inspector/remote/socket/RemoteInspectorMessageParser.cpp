/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "RemoteInspectorMessageParser.h"

#include <wtf/ByteOrder.h>

#if ENABLE(REMOTE_INSPECTOR)

namespace Inspector {

/*
| <--- one message for send / didReceiveData ---> |
+--------------+----------------------------------+--------------
|    size      |               data               | (next message)
|  4byte (NBO) |          variable length         |
+--------------+----------------------------------+--------------
               | <------------ size ------------> |
*/

MessageParser::MessageParser(Function<void(Vector<uint8_t>&&)>&& listener)
    : m_listener(WTFMove(listener))
{
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
Vector<uint8_t> MessageParser::createMessage(std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > std::numeric_limits<uint32_t>::max())
        return Vector<uint8_t>();

    auto messageBuffer = Vector<uint8_t>(data.size() + sizeof(uint32_t));
    uint32_t nboSize = htonl(static_cast<uint32_t>(data.size()));
    memcpy(&messageBuffer[0], &nboSize, sizeof(uint32_t));
    memcpy(&messageBuffer[sizeof(uint32_t)], data.data(), data.size());
    return messageBuffer;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void MessageParser::pushReceivedData(std::span<const uint8_t> data)
{
    if (data.empty() || !m_listener)
        return;

    m_buffer.append(data);

    if (!parse())
        clearReceivedData();
}

void MessageParser::clearReceivedData()
{
    m_buffer.clear();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
bool MessageParser::parse()
{
    while (m_buffer.size() >= sizeof(uint32_t)) {
        uint32_t dataSize;
        memcpy(&dataSize, &m_buffer[0], sizeof(uint32_t));
        dataSize = ntohl(dataSize);
        if (!dataSize) {
            LOG_ERROR("Message Parser received an invalid message size");
            return false;
        }

        size_t messageSize = (sizeof(uint32_t) + dataSize);
        if (m_buffer.size() < messageSize) {
            // Wait for more data.
            return true;
        }

        // FIXME: This should avoid re-creating a new data Vector.
        auto dataBuffer = Vector<uint8_t>(dataSize);
        memcpy(&dataBuffer[0], &m_buffer[sizeof(uint32_t)], dataSize);

        m_listener(WTFMove(dataBuffer));

        m_buffer.removeAt(0, messageSize);
    }

    return true;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
