/*
 * Copyright (C) 2024 The LineageOS Project
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

#define LOG_TAG "bb_token"
#define LOG_NDEBUG 0

#include <log/log.h>

extern "C" int token_service_get_token_payload(const char *name, void *buf) {
	ALOGI("%s called for %s", __func__, name);
	return 1;
}

extern "C" int token_service_is_token_present(const char *name) {
	ALOGI("%s called for %s", __func__, name);
	return 1;
}
