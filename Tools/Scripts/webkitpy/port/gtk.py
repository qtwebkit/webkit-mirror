# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013 Samsung Electronics.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import subprocess
import uuid

from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.base import Port
from webkitpy.port.pulseaudio_sanitizer import PulseAudioSanitizer
from webkitpy.port.xvfbdriver import XvfbDriver
from webkitpy.port.westondriver import WestonDriver


class GtkPort(Port):
    port_name = "gtk"

    def __init__(self, *args, **kwargs):
        super(GtkPort, self).__init__(*args, **kwargs)
        self._pulseaudio_sanitizer = PulseAudioSanitizer()

        if self.get_option("leaks"):
            if not self.get_option("wrapper"):
                raise ValueError('use --wrapper=\"valgrind\" for memory leak detection on GTK')

    def warn_if_bug_missing_in_test_expectations(self):
        return not self.get_option('webkit_test_runner')

    def _port_flag_for_scripts(self):
        return "--gtk"

    @memoized
    def _driver_class(self):
        if os.environ.get("WAYLAND_DISPLAY"):
            return WestonDriver
        return XvfbDriver

    def default_timeout_ms(self):
        # Starting an application under Valgrind takes a lot longer than normal
        # so increase the timeout (empirically 10x is enough to avoid timeouts).
        multiplier = 10 if self.get_option("leaks") else 1
        if self.get_option('configuration') == 'Debug':
            return multiplier * 12 * 1000
        return multiplier * 6 * 1000

    def driver_stop_timeout(self):
        if self.get_option("leaks"):
            # Wait the default timeout time before killing the process in driver.stop().
            return self.default_timeout_ms()
        return super(GtkPort, self).driver_stop_timeout()

    def setup_test_run(self):
        super(GtkPort, self).setup_test_run()
        self._pulseaudio_sanitizer.unload_pulseaudio_module()

    def clean_up_test_run(self):
        super(GtkPort, self).clean_up_test_run()
        self._pulseaudio_sanitizer.restore_pulseaudio_module()

    def setup_environ_for_server(self, server_name=None):
        environment = super(GtkPort, self).setup_environ_for_server(server_name)
        environment['GSETTINGS_BACKEND'] = 'memory'
        environment['LIBOVERLAY_SCROLLBAR'] = '0'
        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('Libraries', 'libTestRunnerInjectedBundle.la')
        environment['TEST_RUNNER_TEST_PLUGIN_PATH'] = self._build_path('TestNetscapePlugin', '.libs')
        environment['AUDIO_RESOURCES_PATH'] = self.path_from_webkit_base('Source', 'WebCore', 'platform', 'audio', 'resources')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_OUTPUTDIR')
        if self.get_option("leaks"):
            #  Turn off GLib memory optimisations https://wiki.gnome.org/Valgrind.
            environment['G_SLICE'] = 'always-malloc'
            environment['G_DEBUG'] = 'gc-friendly'
            xmlfilename = "".join(("drt-%p-", uuid.uuid1().hex, "-leaks.xml"))
            xmlfile = os.path.join(self.results_directory(), xmlfilename)
            suppressionsfile = self.path_from_webkit_base('Tools', 'Scripts', 'valgrind', 'suppressions.txt')
            environment['VALGRIND_OPTS'] = \
                "--tool=memcheck " \
                "--num-callers=40 " \
                "--demangle=no " \
                "--trace-children=no " \
                "--smc-check=all-non-file " \
                "--leak-check=yes " \
                "--leak-resolution=high " \
                "--show-possibly-lost=no " \
                "--show-reachable=no " \
                "--leak-check=full " \
                "--undef-value-errors=no " \
                "--gen-suppressions=all " \
                "--xml=yes " \
                "--xml-file=\"%s\" " \
                "--suppressions=%s" % (xmlfile, suppressionsfile)
        return environment

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            configurations.append(TestConfiguration(version=self._version, architecture='x86', build_type=build_type))
        return configurations

    def _path_to_driver(self):
        return self._build_path('Programs', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('Programs', 'ImageDiff')

    def _path_to_webcore_library(self):
        gtk_library_names = [
            "libwebkitgtk-1.0.so",
            "libwebkitgtk-3.0.so",
            "libwebkit2gtk-1.0.so",
        ]

        for library in gtk_library_names:
            full_library = self._build_path(".libs", library)
            if self._filesystem.isfile(full_library):
                return full_library
        return None

    def _search_paths(self):
        search_paths = []
        if self.get_option('webkit_test_runner'):
            search_paths.extend([self.port_name + '-wk2', 'wk2'])
        else:
            search_paths.append(self.port_name + '-wk1')
        search_paths.append(self.port_name)
        search_paths.extend(self.get_option("additional_platform_directory", []))
        return search_paths

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, self._search_paths())

    def _port_specific_expectations_files(self):
        return [self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in reversed(self._search_paths())]

    # FIXME: We should find a way to share this implmentation with Gtk,
    # or teach run-launcher how to call run-safari and move this down to Port.
    def show_results_html_file(self, results_filename):
        run_launcher_args = ["file://%s" % results_filename]
        if self.get_option('webkit_test_runner'):
            run_launcher_args.append('-2')
        # FIXME: old-run-webkit-tests also added ["-graphicssystem", "raster", "-style", "windows"]
        # FIXME: old-run-webkit-tests converted results_filename path for cygwin.
        self._run_script("run-launcher", run_launcher_args)

    def check_sys_deps(self, needs_http):
        return super(GtkPort, self).check_sys_deps(needs_http) and self._driver_class().check_driver(self)

    def _get_gdb_output(self, coredump_path):
        cmd = ['gdb', '-ex', 'thread apply all bt 1024', '--batch', str(self._path_to_driver()), coredump_path]
        proc = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        errors = [l.strip().decode('utf8', 'ignore') for l in stderr.splitlines()]
        return (stdout.decode('utf8', 'ignore'), errors)

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        pid_representation = str(pid or '<unknown>')
        log_directory = os.environ.get("WEBKIT_CORE_DUMPS_DIRECTORY")
        errors = []
        crash_log = ''
        expected_crash_dump_filename = "core-pid_%s-_-process_%s" % (pid_representation, name)

        def match_filename(filesystem, directory, filename):
            if pid:
                return filename == expected_crash_dump_filename
            return filename.find(name) > -1

        if log_directory:
            dumps = self._filesystem.files_under(log_directory, file_filter=match_filename)
            if dumps:
                # Get the most recent coredump matching the pid and/or process name.
                coredump_path = list(reversed(sorted(dumps)))[0]
                if not newer_than or self._filesystem.mtime(coredump_path) > newer_than:
                    crash_log, errors = self._get_gdb_output(coredump_path)

        stderr_lines = errors + (stderr or '<empty>').decode('utf8', 'ignore').splitlines()
        errors_str = '\n'.join(('STDERR: ' + l) for l in stderr_lines)
        if not crash_log:
            if not log_directory:
                log_directory = "/path/to/coredumps"
            core_pattern = os.path.join(log_directory, "core-pid_%p-_-process_%e")
            crash_log = """\
Coredump %(expected_crash_dump_filename)s not found. To enable crash logs:

- run this command as super-user: echo "%(core_pattern)s" > /proc/sys/kernel/core_pattern
- enable core dumps: ulimit -c unlimited
- set the WEBKIT_CORE_DUMPS_DIRECTORY environment variable: export WEBKIT_CORE_DUMPS_DIRECTORY=%(log_directory)s

""" % locals()

        return (stderr, """\
Crash log for %(name)s (pid %(pid_representation)s):

%(crash_log)s
%(errors_str)s""" % locals())
