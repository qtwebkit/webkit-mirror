# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os

from webkitcorepy import mocks
from webkitscmpy import local


class Git(mocks.Subprocess):
    def __init__(self, path='/.invalid-git', branch=None, remote=None, detached=None):
        self.path = path
        self.branch = branch or 'master'
        self.remote = remote or 'git@webkit.org:/mock/{}'.format(os.path.basename(path))
        self.detached = detached or False

        super(Git, self).__init__(
            mocks.Subprocess.Route(
                local.Git.executable, 'status',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='''HEAD detached at b8a315ed93c
nothing to commit, working tree clean
''' if self.detached else ''''On branch {branch}
Your branch is up to date with 'origin/{branch}'.

nothing to commit, working tree clean
'''.format(branch=self.branch),
                    ),
            ),
            mocks.Subprocess.Route(
                local.Git.executable, 'rev-parse', '--show-toplevel',
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.path),
                ),
            ), mocks.Subprocess.Route(
                local.Git.executable, 'rev-parse', '--abbrev-ref', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format('HEAD' if self.detached else self.branch),
                ),
            ), mocks.Subprocess.Route(
                local.Git.executable, 'remote', 'get-url', '.*',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.remote),
                ) if args[3] == 'origin' else mocks.ProcessCompletion(
                    returncode=128,
                    stderr="fatal: No such remote '{}'\n".format(args[3]),
                ),
            ), mocks.Subprocess.Route(
                local.Git.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr='usage: git [--version] [--help]...\n',
                ),
            ), mocks.Subprocess.Route(
                local.Git.executable,
                completion=mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: not a git repository (or any parent up to mount point)\nStopping at filesystem boundary (GIT_DISCOVERY_ACROSS_FILESYSTEM not set).\n',
                ),
            ),
        )
