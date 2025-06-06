/* Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LogClient.h"

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

namespace WebKit {

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
LogClient::LogClient(IPC::StreamClientConnection& connection, const LogStreamIdentifier &identifier)
    : m_logStreamConnection(connection)
    , m_logStreamIdentifier(identifier)
{
}
#else
LogClient::LogClient(IPC::Connection& connection, const LogStreamIdentifier& identifier)
    : m_logConnection(connection)
    , m_logStreamIdentifier(identifier)
{
}
#endif

void LogClient::log(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, os_log_type_t type)
{
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    Locker locker { m_logStreamLock };
    m_logStreamConnection->send(Messages::LogStream::LogOnBehalfOfWebContent(logChannel, logCategory, logString, type), m_logStreamIdentifier);
#else
    if (RefPtr connection = m_logConnection.get()) {
        connection->send(Messages::LogStream::LogOnBehalfOfWebContent(logChannel, logCategory, logString, type), m_logStreamIdentifier);
        return;
    }
#endif
}

}

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
