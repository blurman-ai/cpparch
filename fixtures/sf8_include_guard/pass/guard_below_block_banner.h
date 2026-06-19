/*
 * Copyright 2024 Example Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file   guard_below_block_banner.h
 *
 * @brief  Models nanovdb-style headers whose include guard sits below a long
 *         block-comment license + documentation banner (#128). The guard must
 *         be found even though it is far from the first physical line: comment
 *         lines are stripped before the guard scan, so the banner length does
 *         not matter.
 *
 *         Historically SF.8 scanned only the first N non-empty lines and
 *         false-flagged such headers as "missing #pragma once or include
 *         guard". This fixture documents that contract: 0 violations expected.
 */

#ifndef ARCHCHECK_FIXTURE_GUARD_BELOW_BLOCK_BANNER_H
#define ARCHCHECK_FIXTURE_GUARD_BELOW_BLOCK_BANNER_H

namespace fixture
{

struct Widget
{
  int value = 0;
};

} // namespace fixture

#endif // ARCHCHECK_FIXTURE_GUARD_BELOW_BLOCK_BANNER_H
