/************************************************************************************
 *
 * D++, A Lightweight C++ library for Discord
 *
 * Copyright 2021 Craig Edwards and D++ contributors
 * (https://github.com/brainboxdotcc/DPP/graphs/contributors)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#pragma once

/* Compile-time assertion check for C++17 */
// Investigate: MSVC doesnt like this
//static_assert(__cplusplus >= 201703L, "D++ Requires a C++17 compatible compiler. Please ensure that you have enabled C++17 in your compiler flags.");


#define DPP_EXPORT


#ifndef _WIN32
	#define SOCKET int
#else
	#include <WinSock2.h>
#endif