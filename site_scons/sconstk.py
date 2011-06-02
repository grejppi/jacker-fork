
import sys
import SCons.Script

def Error(msg):
    print >> sys.stderr, 'ERROR: ' + msg
    SCons.Script.Exit(1)
