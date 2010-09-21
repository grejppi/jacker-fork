import os, sys
import distutils.sysconfig

root_dir = Dir('#').abspath

######################################
# install paths
######################################

try:
	umask = os.umask(022)
	#print 'setting umask to 022 (was 0%o)' % umask
except OSError:     # ignore on systems that don't support umask
	pass

import SCons
from SCons.Script.SConscript import SConsEnvironment
SConsEnvironment.Chmod = SCons.Action.ActionFactory(os.chmod,
		lambda dest, mode: 'Chmod: "%s" with 0%o' % (dest, mode))

# 755 = rwxr-xr-x
def InstallPerm(env, dir, source, perm):
	obj = env.Install(dir, source)
	for i in obj:
		env.AddPostAction(i, env.Chmod(str(i), perm))
	return dir

SConsEnvironment.InstallPerm = InstallPerm

def win32():
    return sys.platform == 'win32'
    
def make_symlink(target, source, env):
    os.symlink(os.path.abspath(str(source[0])), str(target[0]))
    
class LocalEnvironment(Environment):
    def __init__(self, **kargs):
        Environment.__init__(self,
            ENV=os.environ,
            variables=opts,
            **kargs)
        self['BUILDERS'].update(dict(
            Symlink = Builder(action = make_symlink)
        ))
        self.Append(
            LINKFLAGS = "-z defs" # warn for missing symbols
        )
        if not self['VERBOSE']:
            self.Append(
                SHCXXCOMSTR = "$SOURCE",
                SHLINKCOMSTR = "Linking $TARGET",
                CXXCOMSTR = "$SOURCE",
                LINKCOMSTR = "Linking $TARGET",
            )
        if self['DEBUG']:
            self.debug()
        else:
            self.release()

        self.Alias(target='install', source="${DESTDIR}${PREFIX}")
        self['PYTHON_SITE_PACKAGES'] = distutils.sysconfig.get_python_lib(prefix="${DESTDIR}${PREFIX}")
        
    def debug(self):
        if win32():
            self.Append(
                CXXFLAGS=[
                    "/EHsc",
                    "/arch:SSE",
                    "/MTd",
                    "/D",
                ],
            )
        else:
            self.Append(
                CXXFLAGS=[
                    "-g",
                    "-DDEBUG",
                    "-fno-strict-aliasing",
                    "-fwrapv",
                    "-Wall",
                    "-Wno-deprecated",
                    '-march=core2', #x86_64: we need to take this out
                    '-mfpmath=sse',
                    '-msse',
                    '-ffast-math',
                ],
            )
    
    def release(self):
        if win32():
            self.Append(
                CXXFLAGS=[
                    "/EHsc",
                    "/arch:SSE",
                    "/Ox",
                    "/Oy",
                    "/Oi",
                    "/Ob2",
                    "/fp:fast",
                    "/MT",
                ],
            )
        else:
            self.Append(
                CXXFLAGS=[
                    "-fno-strict-aliasing",
                    "-fwrapv",
                    "-Wall",
                    "-Wno-deprecated",
                    '-march=core2', #x86_64: we need to take this out
                    '-mfpmath=sse',
                    '-msse',
                    '-O3',
                    '-funroll-loops',
                    '-fomit-frame-pointer',
                    '-ffast-math',
                    '-DNDEBUG',
                ],
            )

    def install(self, target, source, perm=None):
        if self['IDDQD']:
            target_dir = self.Dir(target)
            if not hasattr(source, '__iter__'):
                source = [source]
            for s in source:
                target = os.path.join(str(target_dir), os.path.basename(str(s)))
                i = self.Symlink(target=target, source=source)
        elif not perm:
            self.Install(dir=self.Dir(target), source=source)
        else:
            self.InstallPerm(dir=self.Dir(target), source=source, perm=perm)

class PyExtEnvironment(LocalEnvironment):
    def __init__(self, **kargs):
        LocalEnvironment.__init__(self, **kargs)
        if win32():
            python_inc = distutils.sysconfig.get_python_inc(False)
            # not beautiful but rare
            python_lib = os.path.join(distutils.sysconfig.get_python_lib(False, True),'..','libs')
            self.Append(
                CPPPATH=[
                    python_inc,
                    '../../../include',
                    '#/include',
                ],
                CXXFLAGS=[
                    "/DBOOST_PYTHON_STATIC_LIB",
                    "/DWIN32",
                ],
                LIBPATH=[
                    python_lib,
                    '../../../lib',
                ],
                LIBS=[
                ]
            )
            self['SHLIBPREFIX'] = "" #gets rid of lib prefix
            self['SHLIBSUFFIX'] = ".pyd"
        else:
            python_inc = distutils.sysconfig.get_python_inc(False)
            python_lib = distutils.sysconfig.get_python_lib(False, True)

            self.Append(
                CPPPATH=[
                    python_inc,
                    '#/include',
                ],
                LIBPATH=[
                    python_lib,
                ],
                LIBS=[
                    'stdc++', # possible fix for __cxa_allocate_exception segfault
                    'boost_python-mt-py26',
                ],
            )
            self['SHLIBPREFIX'] = "" #gets rid of lib prefix
            self.ParseConfig("python-config --libs")
    
######################################

#VariantDir('build', 'src', duplicate=0)

def bool_converter(value):
	value = value.lower()
	if value in ('true','enabled','on','yes','1'):
		return True
	elif value in ('false','disabled','off','no','0'):
		return False
	return bool(value)

######################################

opts = Variables( root_dir + '/options.conf', ARGUMENTS )
opts.Add("PREFIX", 'Set the install "prefix" ( /path/to/PREFIX )', "/usr/local")
opts.Add("DESTDIR", 'Set the root directory to install into ( /path/to/DESTDIR )', "")
opts.Add("DEBUG", "Compile in debug mode if true", False, None, bool_converter)
opts.Add("VERBOSE", "Print command lines while building", False, None, bool_converter)
opts.Add("IDDQD", "Enables god mode (installs symlinks)", False, None, bool_converter)

env = LocalEnvironment()
env.SConsignFile()

assert not env['DESTDIR'] or os.path.isabs(env['DESTDIR']), "%s must be an absolute path." % env['DESTDIR']
assert not env['PREFIX'] or os.path.isabs(env['PREFIX']), "%s must be an absolute path." % env['PREFIX']

opts.Save(root_dir + '/options.conf', env)
Help( opts.GenerateHelpText( env ) )

######################################

Export('LocalEnvironment', 
       'PyExtEnvironment', 
       'win32')
SConscript([
    'SConscript',
])
