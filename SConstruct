
import os
import sys

win32 = sys.platform == 'win32'

if win32:
    env = Environment(
        ENV = os.environ,
        CXXFLAGS = [
            "/EHsc",
            "/arch:SSE",
            "/Ox",
            "/Oy",
            "/Oi",
            "/Ob2",
            "/fp:fast",
            "/MT",
            "/DBT_NO_PROFILE=1",
            "/DWIN32",
        ],
        CPPPATH = [
            "win32/include",
        ],
        LIBPATH = [
            "win32/lib",
        ],
        LIBS = [
            'SDL',
            'opengl32',
            'glu32',
            'glut32',
            'libjack',
        ],
        )
else:
    env = Environment(
        ENV = os.environ,
        CXXFLAGS = [
            "-g",
            "-DBT_NO_PROFILE=1",
        ],
        LIBS = [
            'stdc++',
            'GL',
            'GLU',
            'glut',
        ],
        )
    env.ParseConfig("pkg-config sdl --cflags --libs")
    env.ParseConfig("pkg-config jack --cflags --libs")

env.Program('jacker',
    ['main.cpp',
    'jack.cpp'])
