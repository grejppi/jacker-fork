################################################################################
# build script which distinguishes between the platform we are building on,
# (a combination of OS and toolkit) and the platform we are building for,
# a combination of OS and architecture.
#
# it should be possible to build multiple objects for multiple targets at
# the same time.
#
# here's a list of platforms we'd like to support:
#
#               | linux-gcc | linux-wine-msvc | win32-msvc | win32-mingw | darwin-xcode | darwin-gcc |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# linux-x86     |     X     |                 |            |             |              |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# linux-x86_64  |     X     |                 |            |             |              |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# win32-x86     |     X     |        X        |     X      |      X      |              |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# win32-x86_64  |     X     |        X        |     X      |      X      |              |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# darwin-x86    |           |                 |            |             |      X       |      X     |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# darwin-x86_64 |           |                 |            |             |      X       |      X     |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# android-armv7 |     X     |                 |            |             |              |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# ios-armv6     |           |                 |            |             |      X       |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# ios-armv7     |           |                 |            |             |      X       |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
# ios-universal |           |                 |            |             |      X       |            |
# --------------+-----------+-----------------+------------+-------------+--------------+------------+
#
# here is a list of objects we'd like to be able to build:
# - executable
# - static library
# - shared library
# - visual studio solution & projects
# - xcode projects
# - eclipse projects
# - android projects
# - wrapper makefiles
#
################################################################################

import os
import sys
import platform
import inspect
import multiprocessing

import SCons.Script
from sconstk import *

EmptyEnvironment = SCons.Script.Environment

_global_variable_path = 'build.cfg'

_toolchains = {
    #~ 'linux-gcc',
    #~ 'linux-wine-msvc',
    #~ 'win32-msvc',
    #~ 'win32-mingw',
    #~ 'darwin-xcode',
    #~ 'darwin-gcc',
}

_platforms = {
    #~ 'linux-x86',
    #~ 'linux-x86_64',
    #~ 'win32-x86',
    #~ 'win32-x86_64',
    #~ 'darwin-x86',
    #~ 'darwin-x86_64',
    #~ 'android-armv7',
    #~ 'ios-armv6',
    #~ 'ios-armv7',
    #~ 'ios-universal',
}

def add_toolchain(cls):
    assert cls.name
    _toolchains[cls.name] = cls
    return cls
    
def add_platform(cls):
    assert cls.name
    _platforms[cls.name] = cls
    return cls

def platform_stats():
    print "Machine: %s" % platform.machine()
    print "Processor: %s" % platform.processor()
    print "System: %s" % platform.system()
    print "Release: %s" % platform.release()
    print "Version: %s" % platform.version()

def which(binpath):
    for path in os.environ['PATH'].split(os.pathsep):
        filepath = os.path.join(path, binpath)
        if os.path.isfile(filepath):
            return filepath
    return None

################################################################################
# Platforms
################################################################################

class Configurable(object):
    @classmethod
    def plus_features(cls, **kargs):
        features = cls.features.copy()
        features.update(kargs)
        return features

class Platform(Configurable):
    name = None
    features = dict(
        has_pkgconfig = False,
        has_aptget = False,
    )    
    system = None
    machine = None
    machine_libdir = '/lib'

class PlatformLinux(Platform):
    base = Platform
    
    name = 'linux'
    features = base.plus_features(
        has_pkgconfig = True,
        has_aptget = False,
    )
    system = "Linux"
    
    def __init__(self):
        self.features = self.plus_features(
            has_aptget = which('apt-get') or False
        )

@add_platform
class PlatformLinux_x86(PlatformLinux):
    name = PlatformLinux.name + '-x86'
    machine = "i686"

@add_platform
class PlatformLinux_x86_64(PlatformLinux):
    name = PlatformLinux.name + '-x86_64'
    machine = "x86_64"
    #machine_libdir = '/lib64'

class PlatformWin32(Platform):
    name = 'win32'
    system = "Windows"

@add_platform
class PlatformWin32_x86(PlatformWin32):
    name = PlatformWin32.name + '-x86'
    machine = "x86"

@add_platform
class PlatformWin32_x86_64(PlatformWin32):
    name = PlatformWin32.name + '-x86_64'

class PlatformDarwin(Platform):
    name = 'darwin'

@add_platform
class PlatformDarwin_x86(PlatformWin32):
    name = PlatformDarwin.name + '-x86'

@add_platform
class PlatformDarwin_x86_64(PlatformWin32):
    name = PlatformDarwin.name + '-x86_64'

@add_platform
class PlatformAdobeAlchemy(Platform):
    name = 'alchemy'

@add_platform
class PlatformAndroidARMv7(Platform):
    name = 'android-armv7'

def PlatformAuto():
    machine = platform.machine()
    system = platform.system()
    for name,pf in _platforms.items():
        if not inspect.isclass(pf):
            continue
        if machine != pf.machine:
            continue
        if system != pf.system:
            continue
        return pf()
    platform_stats()
    Error("The current platform can not be recognized, or is not supported yet.")
    
PlatformAuto.name = 'auto'
add_platform(PlatformAuto)

################################################################################
# Toolchains
################################################################################

class Toolchain(Configurable):
    name = None
    features = dict(
        has_pkgconfig = False,
    )
    platforms = {
    }
    libs = []
    defines = []
    flags = ''
    debug_flags = ''
    release_flags = ''
    system = None
    
    def get_platform_config(self, pf, default=None):
        config = self.platforms.get(pf.__class__, default)
        if config is None:
            keys = sorted([p.name for p in self.platforms])
            Error("%s toolchain only accepts platforms %r." % (
                self.name, keys)
            )
        return config
    
    def configure(self, env, pf):
        config = self.get_platform_config(pf)

        env['platform'] = pf.name
        env['toolchain'] = self.name

        features = pf.features.copy()
        for feature,enabled in self.features.items():
            if features.has_key(feature):
                features[feature] = features[feature] and enabled
            else:
                features[feature] = enabled
        
        for feature,enabled in features.items():
            env[feature] = enabled
        
        env['machine_libdir'] = pf.machine_libdir
        env.Append(
            LIBS = self.libs,
            CPPDEFINES = self.defines,
        )

        env.MergeFlags(self.flags)

        variant_dir = '/' + pf.name
        if env['debug']:
            variant_dir += '/debug'
            env.MergeFlags(self.debug_flags)
        else:
            variant_dir += '/release'
            env.MergeFlags(self.release_flags)
        env['variant_dir'] = variant_dir
        
        platform_flags = config.get('flags', '')
        if platform_flags:
            env.MergeFlags(platform_flags)

@add_toolchain
class ToolchainGCC(Toolchain):
    base = Toolchain
    
    name = 'gcc'
    features = base.plus_features(
        has_pkgconfig = True
    )
    debug_flags = '-g'
    release_flags = '-O2'
    platforms = {
        PlatformLinux_x86 : dict(
            flags = '-m32',
        ),
        PlatformLinux_x86_64 : dict(
            flags = '-m64 -fPIC',
        ),
    }

@add_toolchain
class ToolchainLinuxGCC(ToolchainGCC):
    base =  ToolchainGCC
    
    name = 'linux-' + base.name
    features = base.plus_features(
    )
    system = 'Linux'

@add_toolchain
class ToolchainWindowsMSVC(Toolchain):
    base = Toolchain
    
    name = 'win32-msvc'
    system = 'Windows'
    libs = ['Ws2_32']
    features = base.plus_features(
    )
    
    #debug_flags = '-g'
    #release_flags = '-O2'
    platforms = {
        PlatformWin32_x86 : dict(
        ),
    }
    
@add_toolchain
class ToolchainLinuxAdobeAlchemy(Toolchain):
    base = Toolchain
    
    name = 'linux-alchemy'
    features = base.plus_features(
        has_pkgconfig = True
    )
    defines = [
        'ADOBE_ALCHEMY',
    ]
    #debug_flags = '-g'
    #release_flags = '-O2'
    platforms = {
        PlatformAdobeAlchemy : dict(
        ),
    }
    
    def configure(self, env, pf):
        self.base.configure(self, env, pf)
        ALCHEMY_HOME = os.environ.get('ALCHEMY_HOME',None)
        if not ALCHEMY_HOME:
            Error("ALCHEMY_HOME not set - have you executed "
                "'source /path/to/alchemy-setup' before running scons?")
        if not which('gcc').startswith(ALCHEMY_HOME):
            Error("Path to gcc does not point to ALCHEMY_HOME - "
                "have you executed 'alc-on' before running scons?")
    
def ToolchainAuto():
    machine = platform.machine()
    system = platform.system()
    for name,tc in _toolchains.items():
        if not inspect.isclass(tc):
            continue
        if system != tc.system:
            continue
        return tc()
    platform_stats()
    Error("The current toolchain can not be recognized, or is not supported yet.")

ToolchainAuto.name = 'auto'
add_toolchain(ToolchainAuto)

################################################################################
################################################################################

def PrintCommandLine(s, target, src, env):
    """s is the original command line, target and src are lists of target
        and source nodes respectively, and env is the environment."""
    if s == '<ignore>':
        return
    sys.stdout.write(s + u'\n')

def Environment(*custom_args, **custom_kargs):
    # setup variables
    vars = Variables(_global_variable_path)
    vars.AddVariables(
        BoolVariable('debug', 
            'Build with debug symbols (if no, optimize).',
            False),
        EnumVariable('toolchain',
            'Tools to use for building.',
            'auto',
            allowed_values=sorted(_toolchains.keys())),
        EnumVariable('platform',
            'Target platform to build for.',
            'auto',
            allowed_values=sorted(_platforms.keys())),
        EnumVariable('jobs',
            'Number of jobs to use for building.',
            min(max(multiprocessing.cpu_count(),1),8),
            allowed_values=[str(s) for s in range(1,9)]),
        PathVariable('destdir',
            'Root path of installation.',
            '',
            PathVariable.PathAccept),
        PathVariable('prefix',
            'Prefix for all installed files, relative to "destdir".',
            '/usr/local',
            PathVariable.PathAccept),
        PathVariable('bindir',
            'Prefix for binary files, relative to "prefix".',
            '/bin',
            PathVariable.PathAccept),
        PathVariable('libdir',
            'Prefix for library files, relative to "prefix".',
            '$machine_libdir',
            PathVariable.PathAccept),
        PathVariable('includedir',
            'Prefix for include files, relative to "prefix".',
            '/include',
            PathVariable.PathAccept),
        PathVariable('etcdir',
            'Prefix for config files, relative to "destdir".',
            '/usr/local/etc',
            PathVariable.PathAccept),
        PathVariable('sharedir',
            'Prefix for shared resources, relative to "prefix".',
            '/share',
            PathVariable.PathAccept),
        PathVariable('docdir',
            'Prefix for documentation files, relative to "prefix".',
            '/doc',
            PathVariable.PathAccept),
        BoolVariable('tidy', 
            'Tidy build output.',
            True),
    )
    
    # setup args/kargs
    args = custom_args
    kargs= dict(
        ENV = os.environ,
        PRINT_CMD_LINE_FUNC = PrintCommandLine,
    )
    kargs.update(kargs)
    
    # create environment
    env = EmptyEnvironment(*args, **kargs)
    
    # all tools
    Tool("xplib")(env)
    Tool("modular")(env)
    
    vars.Update(env)
    
    # check for unknown variables
    unknown = vars.UnknownVariables()
    if unknown:
        Error("Unknown variables: %r" % unknown.keys())
        
    Help(vars.GenerateHelpText(env))
    
    # save options back to file
    vars.Save(_global_variable_path, env)
    
    # try to parallelize builds as much as possible
    SetOption('num_jobs', int(env['jobs']))

    # monikers to absolute paths
    env['install_dir'] = '$destdir$prefix'
    env['install_bin_dir'] = '$install_dir$bindir'
    env['install_lib_dir'] = '$install_dir$libdir'
    env['install_include_dir'] = '$install_dir$includedir'
    env['install_etc_dir'] = '$destdir$etcdir'
    env['install_share_dir'] = '$install_dir$sharedir'
    env['install_doc_dir'] = '$install_dir$docdir'
    
    # add installation target
    env.Alias('install', [
        '$destdir$prefix'
    ])

    toolchain = _toolchains[env['toolchain']]()
    platform = _platforms[env['platform']]()
    
    toolchain.configure(env, platform)
    
    # configure Tidy build output
    if env['tidy']:
        if 0: # check for msvc compiler here
            # MSVC compiler likes to print its own output and does not want
            # to shut up - so we need to quiet down SCons completely.
            env.Append(
                SHCCCOMSTR = "<ignore>",
                SHCXXCOMSTR = "<ignore>",
                CCCOMSTR = "<ignore>",
                CXXCOMSTR = "<ignore>",
            )
        else:
            env.Append(
                SHCCCOMSTR = "$SOURCE",
                SHCXXCOMSTR = "$SOURCE",
                CCCOMSTR = "$SOURCE",
                CXXCOMSTR = "$SOURCE",
            )
        env.Append(
            ARCOMSTR = "Archiving $TARGET",
            RANLIBCOMSTR = "Indexing $TARGET",
            SHLINKCOMSTR = "Linking $TARGET",
            LINKCOMSTR = "Linking $TARGET",
            INSTALLSTR = "$SOURCE => $TARGET",
        )
    
    
    if 'dump' in COMMAND_LINE_TARGETS:
        print env.Dump()
        print
        platform_stats()
        print
        print "Toolchain: " + toolchain.name
        print "Platform: " + platform.name
        
        Exit(0)
    
    return env
