/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "FileSystemDirectoryReader.h"

#include "DOMException.h"
#include "DOMFileSystem.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ErrorCallback.h"
#include "ExceptionOr.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemEntriesCallback.h"
#include "ScriptExecutionContext.h"
#include "WindowEventLoop.h"
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FileSystemDirectoryReader);

Ref<FileSystemDirectoryReader> FileSystemDirectoryReader::create(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory)
{
    auto reader = adoptRef(*new FileSystemDirectoryReader(context, directory));
    reader->suspendIfNeeded();
    return reader;
}

FileSystemDirectoryReader::FileSystemDirectoryReader(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory)
    : ActiveDOMObject(&context)
    , m_directory(directory)
{
}

FileSystemDirectoryReader::~FileSystemDirectoryReader() = default;

Document* FileSystemDirectoryReader::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

// https://wicg.github.io/entries-api/#dom-filesystemdirectoryentry-readentries
void FileSystemDirectoryReader::readEntries(ScriptExecutionContext& context, Ref<FileSystemEntriesCallback>&& successCallback, RefPtr<ErrorCallback>&& errorCallback)
{
    if (m_isReading) {
        if (errorCallback)
            errorCallback->scheduleCallback(context, DOMException::create(Exception { ExceptionCode::InvalidStateError, "Directory reader is already reading"_s }));
        return;
    }

    if (m_error) {
        if (errorCallback)
            errorCallback->scheduleCallback(context, DOMException::create(*m_error));
        return;
    }

    if (m_isDone) {
        successCallback->scheduleCallback(context, { });
        return;
    }

    m_isReading = true;
    callOnMainThread([context = Ref { context }, successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), pendingActivity = makePendingActivity(*this)]() mutable {
        pendingActivity->object().m_isReading = false;
        pendingActivity->object().m_directory->filesystem().listDirectory(context, pendingActivity->object().m_directory, [successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), pendingActivity = WTFMove(pendingActivity)](ExceptionOr<Vector<Ref<FileSystemEntry>>>&& result) mutable {
            RefPtr document = pendingActivity->object().document();
            if (result.hasException()) {
                pendingActivity->object().m_error = result.releaseException();
                if (errorCallback && document) {
                    document->checkedEventLoop()->queueTask(TaskSource::Networking, [errorCallback = WTFMove(errorCallback), pendingActivity = WTFMove(pendingActivity)]() mutable {
                        errorCallback->invoke(DOMException::create(*pendingActivity->object().m_error));
                    });
                }
                return;
            }
            pendingActivity->object().m_isDone = true;
            if (document) {
                document->checkedEventLoop()->queueTask(TaskSource::Networking, [successCallback = WTFMove(successCallback), pendingActivity = WTFMove(pendingActivity), result = result.releaseReturnValue()]() mutable {
                    successCallback->invoke(WTFMove(result));
                });
            }
        });
    });
}

} // namespace WebCore
