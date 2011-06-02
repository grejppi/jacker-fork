
import os
from sconstk import *

def pkg_config(env, target, source, MIN_VERSION=None, DEB_NAME=None):
    source = source[0]
    error_msg = None
    missing = False
    if env['has_pkgconfig']:
        ret = os.system("pkg-config --exists %s" % source)
        if ret:
            missing = True
            error_msg = "Dependency on %s not satisfied." % source
        if missing and DEB_NAME:
            if env.AptGet(DEB_NAME):
                # check again
                if os.system("pkg-config --exists %s" % source) == 0:
                    # reset state
                    missing = False
                    error_msg = None
        if MIN_VERSION and not missing:
            ret = os.system("pkg-config --exists \'%s >= %s\'" % (source, 
                MIN_VERSION))
            if ret:
                error_msg = "Dependency on %s (>= %s) not satisfied." % (
                    source, MIN_VERSION)
        if error_msg:
            Error(error_msg)
        env.ParseConfig("pkg-config %s --cflags --libs" % source)

def apt_get(env, target, source):
    if not env['has_aptget']:
        return
    deps = ' '.join(source)
    print "Attempting to install dependencies '%s'..." % deps  
    ret = os.system('sudo apt-get install %s' % deps)
    return True if (ret == 0) else False

XPLIB_BUILDERS = dict(
    PkgConfig = pkg_config,
    AptGet = apt_get,
)

def generate(env):
    env.Append(
        BUILDERS = XPLIB_BUILDERS
    )
    
def exists(*args):
    print 'exists',args