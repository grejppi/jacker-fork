
import os
import sys

win32 = sys.platform == 'win32'

if win32:
    LINKFLAGS = [
        #"/SUBSYSTEM:WINDOWS",
        "/NOLOGO",
        "/SUBSYSTEM:CONSOLE",
    ]
    
    if 1:
        # debug
        GTKMM_LIBS = "glademm-vc90-d-2_4.lib xml++-vc90-d-2_6.lib gtkmm-vc90-d-2_4.lib glade-2.0.lib gdkmm-vc90-d-2_4.lib atkmm-vc90-d-1_6.lib pangomm-vc90-d-1_4.lib giomm-vc90-d-2_4.lib glibmm-vc90-d-2_4.lib cairomm-vc90-d-1_0.lib sigc-vc90-d-2_0.lib gtk-win32-2.0.lib libxml2.lib gdk-win32-2.0.lib atk-1.0.lib gdk_pixbuf-2.0.lib pangowin32-1.0.lib pangocairo-1.0.lib pango-1.0.lib cairo.lib gio-2.0.lib gobject-2.0.lib gmodule-2.0.lib glib-2.0.lib intl.lib iconv.lib".split(' ')
        CXXFLAGS = [
            "/EHsc",
            "/arch:SSE",
            "/MTd",
            "/DWIN32",
            "/DDEBUG",
            "/D_DEBUG",
            "/D",        
            "/vd2",
            "/fp:fast",
            "/Zi",
        ]
        LINKFLAGS += [
            "/DEBUG",
        ]
    else:
        # release
        GTKMM_LIBS = "glademm-vc90-2_4.lib xml++-vc90-2_6.lib gtkmm-vc90-2_4.lib glade-2.0.lib gdkmm-vc90-2_4.lib atkmm-vc90-1_6.lib pangomm-vc90-1_4.lib giomm-vc90-2_4.lib glibmm-vc90-2_4.lib cairomm-vc90-1_0.lib sigc-vc90-2_0.lib gtk-win32-2.0.lib libxml2.lib gdk-win32-2.0.lib atk-1.0.lib gdk_pixbuf-2.0.lib pangowin32-1.0.lib pangocairo-1.0.lib pango-1.0.lib cairo.lib gio-2.0.lib gobject-2.0.lib gmodule-2.0.lib glib-2.0.lib intl.lib iconv.lib".split(' ')
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
            "/vd2",
        ]

    env = Environment(
        ENV = os.environ,
        CXXFLAGS = CXXFLAGS,
        LINKFLAGS = LINKFLAGS,
        CPPPATH = [
            "win32/include",
            ".",
        ],
        LIBPATH = [
            "win32/lib",
        ],
        LIBS = [
            'libjack',
        ],
        )
    
    gtk_env = env.Clone()
    gtk_env.Append(
        CPPPATH = [
            r'${GTKMM_BASEPATH}\include\libglademm-2.4',
            r'${GTKMM_BASEPATH}\lib\libglademm-2.4\include',
            r'${GTKMM_BASEPATH}\lib\gtkmm-2.4\include',
            r'${GTKMM_BASEPATH}\include\gtkmm-2.4',
            r'${GTKMM_BASEPATH}\lib\gdkmm-2.4\include',
            r'${GTKMM_BASEPATH}\include\gdkmm-2.4',
            r'${GTKMM_BASEPATH}\include\pangomm-1.4',
            r'${GTKMM_BASEPATH}\include\atkmm-1.6',
            r'${GTKMM_BASEPATH}\lib\libxml++-2.6\include',
            r'${GTKMM_BASEPATH}\include\libxml++-2.6',
            r'${GTKMM_BASEPATH}\lib\giomm-2.4\include',
            r'${GTKMM_BASEPATH}\include\giomm-2.4',
            r'${GTKMM_BASEPATH}\lib\glibmm-2.4\include',
            r'${GTKMM_BASEPATH}\include\glibmm-2.4',
            r'${GTKMM_BASEPATH}\include\cairomm-1.0',
            r'${GTKMM_BASEPATH}\lib\sigc++-2.0\include',
            r'${GTKMM_BASEPATH}\include\sigc++-2.0',
            r'${GTKMM_BASEPATH}\include\libglade-2.0',
            r'${GTKMM_BASEPATH}\lib\gtk-2.0\include',
            r'${GTKMM_BASEPATH}\include\gtk-2.0',
            r'${GTKMM_BASEPATH}\include\pango-1.0',
            r'${GTKMM_BASEPATH}\include\atk-1.0',
            r'${GTKMM_BASEPATH}\lib\glib-2.0\include',
            r'${GTKMM_BASEPATH}\include\glib-2.0',
            r'${GTKMM_BASEPATH}\include\libxml2',
            r'${GTKMM_BASEPATH}\include\cairo',
            r'${GTKMM_BASEPATH}\include',
        ],
        LIBPATH = [
            r'${GTKMM_BASEPATH}\lib',
        ],
        LIBS = GTKMM_LIBS,
    )
        
    gtk_env['GTKMM_BASEPATH'] = os.environ['GTKMM_BASEPATH']
else:
    env = Environment(
        ENV = os.environ,
        CXXFLAGS = [
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
        CPPPATH = [
            '.',
        ],
        LIBS = [
            'stdc++',
        ],
        )
    #env.ParseConfig("pkg-config sdl --cflags --libs")
    env.ParseConfig("pkg-config jack --cflags --libs")
    
    gtk_env = env.Clone()
    gtk_env.ParseConfig("pkg-config gtkmm-2.4 --cflags --libs")
    gtk_env.ParseConfig("pkg-config sigc++-2.0 --cflags --libs")

json_files = [
    'json/json_reader.cpp',
    'json/json_value.cpp',
    'json/json_writer.cpp',
]

objects = env.Object(['jack.cpp',
     'player.cpp',
     'jsong.cpp',
     'model.cpp',
     'drag.cpp',
     ] + json_files)
gtk_objects = gtk_env.Object(['main.cpp',
     'songview.cpp',
     'patternview.cpp',
     'measure.cpp'])
gtk_env.Program('jacker', objects + gtk_objects)
