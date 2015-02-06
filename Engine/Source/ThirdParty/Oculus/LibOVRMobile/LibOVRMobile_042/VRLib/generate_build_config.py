import sys

class_definition = """\
package com.oculusvr.vrlib;

public class BuildConfig {
    public final static boolean DEBUG = %(is_debug)s;
}
"""

config = {
  'is_debug' : 'true',
}

sys.stdout.write(class_definition % config)

