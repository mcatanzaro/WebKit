/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_LIST_DIRECTION_H
#define PAS_LIST_DIRECTION_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_list_direction {
    pas_list_direction_prev,
    pas_list_direction_next
};

typedef enum pas_list_direction pas_list_direction;

static inline const char* pas_list_direction_get_string(pas_list_direction direction)
{
    switch (direction) {
    case pas_list_direction_prev:
        return "prev";
    case pas_list_direction_next:
        return "next";
    }
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline pas_list_direction pas_list_direction_invert(pas_list_direction direction)
{
    switch (direction) {
    case pas_list_direction_prev:
        return pas_list_direction_next;
    case pas_list_direction_next:
        return pas_list_direction_prev;
    }
    PAS_ASSERT_NOT_REACHED();
    return pas_list_direction_prev;
}

PAS_END_EXTERN_C;

#endif /* PAS_LIST_DIRECTION_H */

