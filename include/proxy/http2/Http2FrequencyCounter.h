/** @file

  Http2FrequencyCounter

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#pragma once

#include <cstdint>
#include "iocore/eventsystem/EventSystem.h"

class Http2FrequencyCounter
{
public:
  void increment(uint16_t amount = 1);
  uint32_t get_count();
  virtual ~Http2FrequencyCounter() {}

protected:
  uint16_t _count[2]      = {0};
  ink_hrtime _last_update = 0;

private:
  virtual ink_hrtime _ink_get_hrtime();
};
