#
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

bin_PROGRAMS += traffic_crashlog/traffic_crashlog

traffic_crashlog_traffic_crashlog_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    $(iocore_include_dirs) \
    -I$(abs_top_srcdir)/include \
    -I$(abs_top_srcdir)/lib \
    -I$(abs_top_srcdir)/proxy \
    -I$(abs_top_srcdir)/proxy/http \
    -I$(abs_top_srcdir)/proxy/hdrs \
    -I$(abs_top_srcdir)/mgmt \
    @SWOC_INCLUDES@ \
    $(TS_INCLUDES)

traffic_crashlog_traffic_crashlog_LDFLAGS = \
    $(AM_LDFLAGS) \
    @OPENSSL_LDFLAGS@

traffic_crashlog_traffic_crashlog_SOURCES = \
	shared/overridable_txn_vars.cc \
	traffic_crashlog/procinfo.cc \
	traffic_crashlog/traffic_crashlog.cc \
	traffic_crashlog/traffic_crashlog.h

traffic_crashlog_traffic_crashlog_LIBTOOLFLAGS = --preserve-dup-deps

traffic_crashlog_traffic_crashlog_LDADD = \
    $(top_builddir)/src/records/librecords_p.a \
    $(top_builddir)/src/tscpp/util/libtscpputil.la \
    $(top_builddir)/src/api/libtsapi.la \
    $(top_builddir)/iocore/cache/libinkcache.a \
    $(top_builddir)/lib/fastlz/libfastlz.a \
    $(top_builddir)/proxy/http/libhttp.a \
    $(top_builddir)/iocore/utils/libinkutils.a \
    $(top_builddir)/proxy/logging/liblogging.a \
    $(top_builddir)/proxy/http/remap/libhttp_remap.a \
    $(top_builddir)/proxy/http2/libhttp2.a \
    $(top_builddir)/proxy/hdrs/libhdrs.a \
    $(top_builddir)/proxy/libproxy.a \
    $(top_builddir)/iocore/eventsystem/libinkevent.a \
    $(top_builddir)/iocore/hostdb/libinkhostdb.a \
    $(top_builddir)/iocore/dns/libinkdns.a \
    $(top_builddir)/src/records/librecords_p.a \
    $(top_builddir)/iocore/net/libinknet.a \
    $(top_builddir)/iocore/aio/libinkaio.a \
    $(top_builddir)/mgmt/rpc/libjsonrpc_protocol.la \
    $(top_builddir)/src/tscore/libtscore.a \
    $(top_builddir)/mgmt/config/libconfigmanager.a \
    @HWLOC_LIBS@ @SWOC_LIBS@ @YAMLCPP_LIBS@ @LIBPCRE@ @LIBCAP@ \
    @OPENSSL_LIBS@ \
    @LIBZ@ \
	@LIBLZMA@
