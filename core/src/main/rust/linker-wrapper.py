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
    shutil.copyfile(sys.argv[sys.argv.index('-o') + 1], os.environ['RUST_ANDROID_GRADLE_TARGET'])
sys.exit(code)
