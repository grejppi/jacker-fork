
from sconstk import *

def glob_sconscript_dirs(env, targets, sources):
    targets = []
    dirtype = type(env.Dir('.'))
    for source in (sources or [env.Dir('.')]):
        # include all subdirectory scripts
        for filename in env.Glob(str(source) + '/*'):
            if not isinstance(filename, dirtype):
                continue
            if str(filename).startswith('.'):
                continue
            script_path = env.Glob(str(filename) + '/SConscript')
            if len(script_path):
                targets += [filename]
    
    return targets

MODULAR_BUILDERS = dict(
    GlobSConscriptDirs = glob_sconscript_dirs,
)

def generate(env):
    env.Append(
        BUILDERS = MODULAR_BUILDERS
    )
    
def exists(*args):
    pass