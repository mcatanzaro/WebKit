/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_PAGE_HEADER_PLACEMENT_MODE_H
#define PAS_PAGE_HEADER_PLACEMENT_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_page_header_placement_mode {
    pas_page_header_at_head_of_page,
    pas_page_header_in_table
};

typedef enum pas_page_header_placement_mode pas_page_header_placement_mode;

static inline const char*
pas_page_header_placement_mode_get_string(
    pas_page_header_placement_mode mode)
{
    switch (mode) {
    case pas_page_header_at_head_of_page:
        return "at_head_of_page";
    case pas_page_header_in_table:
        return "in_table";
    }
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_HEADER_PLACEMENT_MODE_H */

