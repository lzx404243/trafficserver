'''
Verify ATS handles down origin servers with domain cached correctly.
'''
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
from ports import get_port
import os

Test.Summary = '''
Verify ATS handles down origin servers with cached domain correctly.
'''


class DownCachedOriginServerTest:
    replay_file = "replay/server_down.replay.yaml"

    def __init__(self):
        """Initialize the Test processes for the test runs."""
        # self._server = Test.MakeVerifierServerProcess("server", DownCachedOriginServerTest.replay_file)

        # The microdns response TTL has been hardcoded to 20 seconds, instead of the default 300s.
        self._dns = Test.MakeDNServer("dns", default='127.0.0.1')
        self._configure_trafficserver()
        self.client_counter = 0

    def _configure_trafficserver(self):
        """Configure Traffic Server."""
        self._ts = Test.MakeATSProcess("ts", enable_cache=False)

        # Reserve a port for the server. We will start the server binded to this port.
        self.server_port = get_port(self._ts, 'server_port')
        self._ts.Disk.remap_config.AddLine(
            f"map / http://resolve.this.com:{self.server_port}/"
        )

        self._ts.Disk.records_config.update({
            'proxy.config.diags.debug.enabled': 1,
            'proxy.config.diags.debug.tags': 'hostdb|dns|http|socket',
            'proxy.config.http.connect_attempts_max_retries': 0,
            'proxy.config.http.connect_attempts_rr_retries': 0,
            'proxy.config.dns.resolv_conf': 'NULL',
            'proxy.config.dns.nameservers': f'127.0.0.1:{self._dns.Variables.Port}',
            'proxy.config.http.connect_attempts_max_retries_down_server': 1,
            # 'proxy.config.hostdb.fail.timeout': 10,
            'proxy.config.hostdb.ttl_mode': 0,
            'proxy.config.hostdb.timeout': 5,
            'proxy.config.hostdb.serve_stale_for': 10,
            # 'proxy.config.dns.dedicated_thread': 1,
            'proxy.config.hostdb.lookup_timeout': 2,
            'proxy.config.http.down_server.cache_time': 5,

            # 'proxy.config.http.transaction_no_activity_timeout_in': 2,
            # 'proxy.config.http.connect_attempts_timeout': 2,
            # 'proxy.config.hostdb.host_file.interval': 1,
            # 'proxy.config.hostdb.host_file.path': os.path.join(Test.TestDirectory, "hosts_file"),
        })
    # Even when the origin server is down, SM will return a hit-fresh domain from HostDB.
    # After request has failed, SM should mark the IP as down

    def _test_host_mark_down(self):
        tr = Test.AddTestRun()

        # tr.Processes.Default.StartBefore(self._server)
        tr.Processes.Default.StartBefore(self._dns)
        tr.Processes.Default.StartBefore(self._ts)

        tr.AddVerifierClientProcess(
            f"client-{self.client_counter}",
            DownCachedOriginServerTest.replay_file,
            http_ports=[self._ts.Variables.port],
            other_args='--keys 1')
        self.client_counter += 1

    # After host has been marked down from previous test, HostDB should not return
    # the host as available and DNS lookup should fail.
    def _test_host_unreachable(self):
        tr = Test.AddTestRun()

        tr.AddVerifierClientProcess(
            f"client-{self.client_counter}",
            DownCachedOriginServerTest.replay_file,
            http_ports=[self._ts.Variables.port],
            other_args='--keys 2')
        self.client_counter += 1

    def _test_host_up(self):
        tr = Test.AddTestRun()

        self.server = tr.AddVerifierServerProcess(
            f"verifier-server-{self.client_counter}",
            http_ports=[
                self.server_port],
            replay_path=DownCachedOriginServerTest.replay_file)
        tr.Processes.Default.StartBefore(self.server)

        tr.AddVerifierClientProcess(
            f"client-{self.client_counter}",
            DownCachedOriginServerTest.replay_file,
            http_ports=[self._ts.Variables.port],
            other_args=f'--keys {self.client_counter + 1}')

        self.client_counter += 1
    # Verify error log marking host down exists

    def _test_error_log(self):
        tr = Test.AddTestRun()
        tr.Processes.Default.Command = (
            os.path.join(Test.Variables.AtsTestToolsDir, 'condwait') + ' 60 1 -f ' +
            os.path.join(self._ts.Variables.LOGDIR, 'error.log')
        )

        # self._ts.Disk.error_log.Content = Testers.ContainsExpression("marking down", "host should be marked down")
        self._ts.Disk.error_log.Content += Testers.ContainsExpression(
            "DNS CACHE origin marked down: resolve.this.com", "DNS lookup should fail")

    def run(self):
        self._test_host_mark_down()
        self._test_host_unreachable()
        self._test_error_log()
        self._test_host_up()
        self._test_host_up()
        # self._test_host_up()


DownCachedOriginServerTest().run()
