#######################
#
#  Licensed to the Apache Software Foundation (ASF) under one or more contributor license
#  agreements.  See the NOTICE file distributed with this work for additional information regarding
#  copyright ownership.  The ASF licenses this file to you under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with the License.  You may obtain
#  a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software distributed under the License
#  is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
#  or implied. See the License for the specific language governing permissions and limitations under
#  the License.
#
#######################

include(add_atsplugin)

add_subdirectory(authproxy)
add_subdirectory(background_fetch)
add_subdirectory(cache_promote)
add_subdirectory(cache_range_requests)
add_subdirectory(cachekey)
add_subdirectory(certifier)
add_subdirectory(compress)
add_subdirectory(conf_remap)
add_subdirectory(escalate)
add_subdirectory(esi)
add_subdirectory(generator)
add_subdirectory(header_rewrite)
add_subdirectory(libloader)
add_subdirectory(multiplexer)
add_subdirectory(prefetch)
add_subdirectory(regex_remap)
add_subdirectory(regex_revalidate)
add_subdirectory(remap_purge)
add_subdirectory(remap_stats)
add_subdirectory(s3_auth)
add_subdirectory(server_push_preload)
add_subdirectory(slice)
add_subdirectory(statichit)
add_subdirectory(stats_over_http)
add_subdirectory(tcpinfo)
add_subdirectory(traffic_dump)
add_subdirectory(xdebug)

if(NOT OPENSSL_IS_BORINGSSL)
  add_subdirectory(ja3_fingerprint)
endif()

if(USE_LUAJIT)
  add_subdirectory(lua)
endif()

if(BUILD_EXPERIMENTAL_PLUGINS)
  add_subdirectory(experimental)
endif()

if(HOST_OS STREQUAL "linux")
  add_subdirectory(healthchecks)
endif()
