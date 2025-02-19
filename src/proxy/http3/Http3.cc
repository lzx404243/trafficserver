/** @file
 *
 *  A brief file description
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "proxy/http3/Http3.h"

// Default values of settings defined by specs (draft-17)
const uint32_t HTTP3_DEFAULT_HEADER_TABLE_SIZE      = 0;
const uint32_t HTTP3_DEFAULT_MAX_FIELD_SECTION_SIZE = UINT32_MAX;
const uint32_t HTTP3_DEFAULT_QPACK_BLOCKED_STREAMS  = 0;
const uint32_t HTTP3_DEFAULT_NUM_PLACEHOLDERS       = 0;

Http3StatsBlock http3_rsb;

void
Http3::init()
{
  // Example (remove comments here when addding)
  //
  // Setup statistics
  // http3_rsb.current_client_session_count = Metrics::Gauge::createPtr("proxy.process.http3.current_client_connections");
}
