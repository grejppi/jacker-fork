
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
            "/DWIN32",
        ],
        CPPPATH = [
            "win32/include",
        ],
        LIBPATH = [
            "win32/lib",
        ],
        LIBS = [
            'libjack',
        ],
        )
else:
    env = Environment(
        ENV = os.environ,
        CXXFLAGS = [
            "-g",
        ],
        LIBS = [
            'stdc++',
        ],
        )
    #env.ParseConfig("pkg-config sdl --cflags --libs")
    env.ParseConfig("pkg-config jack --cflags --libs")
    env.ParseConfig("pkg-config gtkmm-2.4 --cflags --libs")
    env.ParseConfig("pkg-config sigc++-2.0 --cflags --libs")

env.Program('jacker',
    ['main.cpp',
    'jack.cpp'])
