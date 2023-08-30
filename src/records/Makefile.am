# Makefile.am for the src/records
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

include $(top_srcdir)/mk/tidy.mk

check_PROGRAMS = test_librecords test_librecords_on_eventsystem

AM_CPPFLAGS += \
	-I$(abs_top_srcdir)/iocore/eventsystem \
	-I$(abs_top_srcdir)/iocore/utils \
	-I$(abs_top_srcdir)/include \
	-I$(abs_top_srcdir)/lib \
	-I$(abs_top_srcdir)/ \
	@YAMLCPP_INCLUDES@\
	@SWOC_INCLUDES@\
	$(TS_INCLUDES)

noinst_LIBRARIES = librecords_p.a

librecords_COMMON = \
	I_RecCore.h \
	I_RecDefs.h \
	I_RecHttp.h \
	I_RecMutex.h \
	P_RecCore.cc \
	P_RecCore.h \
	P_RecDefs.h \
	P_RecFile.h \
	P_RecMessage.h \
	P_RecUtils.h \
	RecConfigParse.cc \
	RecYAMLDecoder.cc \
	RecCore.cc \
	RecDebug.cc \
	RecFile.cc \
	RecHttp.cc \
	RecMessage.cc \
	RecMutex.cc \
	RecRawStats.cc \
	RecUtils.cc \
	RecordsConfig.cc \
	RecordsConfigUtils.cc

librecords_p_a_SOURCES = $(librecords_COMMON)

TESTS = $(check_PROGRAMS)

TESTS_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(abs_top_srcdir)/lib/catch2 \
    -I$(abs_top_srcdir)/proxy \
    -I$(abs_top_srcdir)/proxy/hdrs \
    -I$(abs_top_srcdir)/proxy/http

test_librecords_CPPFLAGS = $(TESTS_CPPFLAGS)

test_librecords_SOURCES = \
    unit_tests/unit_test_main.cc \
    unit_tests/test_RecHttp.cc \
	$(top_builddir)/src/shared/overridable_txn_vars.cc

test_librecords_LIBTOOLFLAGS = --preserve-dup-deps

test_librecords_LDADD = \
	$(top_builddir)/src/records/librecords_p.a \
	$(top_builddir)/src/api/libtsapi.la \
	$(top_builddir)/iocore/cache/libinkcache.a \
    $(top_builddir)/lib/fastlz/libfastlz.a \
    $(top_builddir)/proxy/http/libhttp.a \
    $(top_builddir)/iocore/utils/libinkutils.a \
    $(top_builddir)/proxy/logging/liblogging.a \
    $(top_builddir)/proxy/http/remap/libhttp_remap.a \
    $(top_builddir)/proxy/http2/libhttp2.a \
	$(top_builddir)/src/records/librecords_p.a \
    $(top_builddir)/iocore/hostdb/libinkhostdb.a \
    $(top_builddir)/iocore/dns/libinkdns.a \
	$(top_builddir)/proxy/hdrs/libhdrs.a \
    $(top_builddir)/proxy/libproxy.a \
    $(top_builddir)/iocore/net/libinknet.a \
    $(top_builddir)/iocore/eventsystem/libinkevent.a \
	$(top_builddir)/iocore/aio/libinkaio.a \
	$(top_builddir)/src/tscpp/util/libtscpputil.la \
    $(top_builddir)/mgmt/rpc/libjsonrpc_protocol.la \
    $(top_builddir)/mgmt/config/libconfigmanager.a \
	$(top_builddir)/src/tscore/libtscore.a \
	@SWOC_LIBS@ @HWLOC_LIBS@ @LIBCAP@ @YAMLCPP_LIBS@ @LIBPCRE@ @OPENSSL_LIBS@ \
    @LIBZ@ \
	@LIBLZMA@

test_librecords_on_eventsystem_CPPFLAGS = $(TESTS_CPPFLAGS)

test_librecords_on_eventsystem_SOURCES = \
    unit_tests/unit_test_main_on_eventsystem.cc \
	unit_tests/test_DynamicStats.cc \
	$(top_builddir)/src/shared/overridable_txn_vars.cc

test_librecords_on_eventsystem_LIBTOOLFLAGS = --preserve-dup-deps

test_librecords_on_eventsystem_LDADD = \
	$(top_builddir)/src/records/librecords_p.a \
	$(top_builddir)/src/api/libtsapi.la \
	$(top_builddir)/iocore/cache/libinkcache.a \
    $(top_builddir)/lib/fastlz/libfastlz.a \
    $(top_builddir)/proxy/http/libhttp.a \
    $(top_builddir)/iocore/utils/libinkutils.a \
    $(top_builddir)/proxy/logging/liblogging.a \
    $(top_builddir)/proxy/http/remap/libhttp_remap.a \
    $(top_builddir)/proxy/http2/libhttp2.a \
	$(top_builddir)/src/records/librecords_p.a \
	$(top_builddir)/proxy/hdrs/libhdrs.a \
	$(top_builddir)/src/tscpp/util/libtscpputil.la \
    $(top_builddir)/proxy/libproxy.a \
    $(top_builddir)/iocore/hostdb/libinkhostdb.a \
    $(top_builddir)/iocore/dns/libinkdns.a \
    $(top_builddir)/iocore/net/libinknet.a \
	$(top_builddir)/iocore/aio/libinkaio.a \
    $(top_builddir)/iocore/eventsystem/libinkevent.a \
    $(top_builddir)/mgmt/rpc/libjsonrpc_protocol.la \
    $(top_builddir)/mgmt/config/libconfigmanager.a \
	$(top_builddir)/src/tscore/libtscore.a \
	@HWLOC_LIBS@ @LIBCAP@ @YAMLCPP_LIBS@ @SWOC_LIBS@ @LIBPCRE@ @OPENSSL_LIBS@ \
    @LIBZ@ \
	@LIBLZMA@

clang-tidy-local: $(sort $(DIST_SOURCES))
	$(CXX_Clang_Tidy)
