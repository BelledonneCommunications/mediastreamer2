/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file defines an NDK API.
 * Do not remove methods.
 * Do not change method signatures.
 * Do not change the value of constants.
 * Do not change the size of any of the classes defined in here.
 * Do not reference types that are not part of the NDK.
 * Do not #include files that aren't part of the NDK.
 */

#ifndef _NDK_MEDIA_CRYPTO_H
#define _NDK_MEDIA_CRYPTO_H

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaCrypto;
typedef struct AMediaCrypto AMediaCrypto;

typedef uint8_t AMediaUUID[16];

bool AMediaCrypto_isCryptoSchemeSupported(const AMediaUUID uuid);

bool AMediaCrypto_requiresSecureDecoderComponent(const char *mime);

AMediaCrypto *AMediaCrypto_new(const AMediaUUID uuid, const void *initData, size_t initDataSize);

void AMediaCrypto_delete(AMediaCrypto *crypto);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _NDK_MEDIA_CRYPTO_H
