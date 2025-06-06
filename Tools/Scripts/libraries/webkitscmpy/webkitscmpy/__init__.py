# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

import sys

if sys.version_info < (3, 9):  # noqa: UP036
    raise ImportError("webkitscmpy requires Python 3.9 or above")

import logging
import os

log = logging.getLogger('webkitscmpy')


def _maybe_add_webkitcorepy_path():
    # Hopefully we're beside webkitcorepy, otherwise webkitcorepy will need to be installed.
    libraries_path = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(__file__))))
    webkitcorepy_path = os.path.join(libraries_path, 'webkitcorepy')
    if os.path.isdir(webkitcorepy_path) and os.path.isdir(os.path.join(webkitcorepy_path, 'webkitcorepy')) and webkitcorepy_path not in sys.path:
        sys.path.insert(0, webkitcorepy_path)


_maybe_add_webkitcorepy_path()

try:
    from webkitcorepy import AutoInstall, Package, Version
except ImportError:
    raise ImportError(
        "'webkitcorepy' could not be found on your Python path.\n" +
        "You are not running from a complete WebKit checkout.\n" +
        "See https://github.com/WebKit/WebKit/tree/main/Tools/Scripts/libraries/webkitcorepy"
    )

version = Version(7, 0, 2)

AutoInstall.register(Package('fasteners', Version(0, 15, 0)))
AutoInstall.register(Package('markupsafe', Version(3, 0, 2), pypi_name='MarkupSafe', wheel=True))
AutoInstall.register(Package('jinja2', Version(3, 1, 4), implicit_deps=['markupsafe']))
AutoInstall.register(Package('monotonic', Version(1, 5)))
AutoInstall.register(Package('xmltodict', Version(0, 11, 0)))
AutoInstall.register(Package('webkitbugspy', Version(0, 15, 1)), local=True)

AutoInstall.register(Package('rapidfuzz', Version(3, 4, 0)))

from webkitscmpy.contributor import Contributor
from webkitscmpy.commit import Commit
from webkitscmpy.commit_classifier import CommitClassifier
from webkitscmpy.pull_request import PullRequest
from webkitscmpy.scm_base import ScmBase

from webkitscmpy import local
from webkitscmpy import mocks

name = 'webkitscmpy'
