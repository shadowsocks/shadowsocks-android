from __future__ import absolute_import, print_function, unicode_literals

import os
import pipes
import shutil
import subprocess
import sys

args = [os.environ['RUST_ANDROID_GRADLE_CC'], os.environ['RUST_ANDROID_GRADLE_CC_LINK_ARG']] + sys.argv[1:]

# This only appears when the subprocess call fails, but it's helpful then.
printable_cmd = ' '.join(pipes.quote(arg) for arg in args)
print(printable_cmd)

code = subprocess.call(args)
if code == 0:
    sys_argv = sys.argv
    if sys.platform == 'msys' or sys.platform == 'cygwin' or sys.platform == 'win32':
        linkargs = list(filter(lambda s: s.startswith('@') and s.find('linker-arguments') != -1, sys.argv[1:]))
        if linkargs != []:
            with open(linkargs[0][1:]) as f:
                sys_argv = f.read().splitlines()
    shutil.copyfile(sys_argv[sys_argv.index('-o') + 1], os.environ['RUST_ANDROID_GRADLE_TARGET'])
sys.exit(code)
