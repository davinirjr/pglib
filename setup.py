#!/usr/bin/env python3

import sys, os, re
from os.path import join, abspath, dirname, exists
from distutils.core import setup, Command, Extension
from distutils.command.build_ext import build_ext

def get_version():
    """
    Returns the version of the product as (description, [major,minor,micro]).

      1. If in a git repository, use the latest tag (git describe).
      2. If in an unzipped source directory (from setup.py sdist),
         read the version from the PKG-INFO file.
      3. Complain ;)
    """

    # If this is a source release the version will have already been assigned and be in the PKG-INFO file.

    filename = join(dirname(abspath(__file__)), 'PKG-INFO')
    if exists(filename):
        match = re.search(r'^Version:\s+(\d+\.\d+\.\d+(?:-beta\d+)?)', open(filename).read())
        if match:
            return match.group(1)

    # If not a source release, we should be in a git repository.  Look for the latest tag.

    n, result = getoutput('git describe --tags --match rel-*')
    if n:
        print('WARNING: git describe failed with: %s %s' % (n, result))
        sys.exit('Unable to determine version.')

    match = re.match(r'rel-(\d+).(\d+).(\d+) (?: -(\d+)-g[0-9a-z]+)?', result, re.VERBOSE)
    if not match:
        sys.exit('Unable to determine version.')

    parts      = [ int(n or 0) for n in match.groups() ]
    prerelease = ''

    if parts[-1] > 0:
        # We are building the next version, so increment the patch and add the word 'beta'.
        parts[-2] += 1
        prerelease = '-rc%02d' % parts[-1]

    return '.'.join(str(part) for part in parts[:3]) + prerelease


def getoutput(cmd):
    pipe = os.popen(cmd, 'r')
    text   = pipe.read().rstrip('\n')
    status = pipe.close() or 0
    return status, text

class VersionCommand(Command):
    description = 'print the library version'
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        version = get_version()
        print(version)
        

def _get_files():
    return [ abspath(join('src', f)) for f in os.listdir('src') if f.endswith('.cpp') ]

def _get_settings():

    version = get_version()

    settings = { 'define_macros' : [ ('PGLIB_VERSION', version) ] }

    # This isn't the best or right way to do this, but I don't see how someone is supposed to sanely subclass the build
    # command.
    for option in ['assert', 'trace', 'leak-check']:
        try:
            sys.argv.remove('--%s' % option)
            settings['define_macros'].append(('PGLIB_%s' % option.replace('-', '_').upper(), 1))
        except ValueError:
            pass

    if os.name == 'nt':
        settings['extra_compile_args'] = ['/Wall',
                                          '/wd4668',
                                          '/wd4820',
                                          '/wd4711', # function selected for automatic inline expansion
                                          '/wd4100', # unreferenced formal parameter
                                          '/wd4127', # "conditional expression is constant" testing compilation constants
                                          '/wd4191', # casts to PYCFunction which doesn't have the keywords parameter
                                          ]

        if '--debug' in sys.argv:
            sys.argv.remove('--debug')
            settings['extra_compile_args'].extend('/Od /Ge /GS /GZ /RTC1 /Wp64 /Yd'.split())

    elif sys.platform == 'darwin':
        # Temporary for Mavericks...
        settings['include_dirs'] = [
            '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk/usr/include'
        ]

        settings['library_dirs'] = [ '/Library/PostgreSQL/9.1/lib/' ]
        settings['libraries']    = ['pq']

        # Apple has decided they won't maintain the iODBC system in OS/X and has added deprecation warnings in 10.8.
        # For now target 10.7 to eliminate the warnings.

        # Python functions take a lot of 'char *' that really should be const.  gcc complains about this *a lot*
        # settings['extra_compile_args'] = ['-Wno-write-strings', '-Wno-deprecated-declarations']

        settings['define_macros'].append( ('MAC_OS_X_VERSION_10_7',) )

    else:
        # Other posix-like: Linux, Solaris, etc.

        # Python functions take a lot of 'char *' that really should be const.  gcc complains about this *a lot*
        settings['extra_compile_args'] = ['-Wno-write-strings']

        # What is the proper way to detect iODBC, MyODBC, unixODBC, etc.?
        # settings['libraries'].append('odbc')

    return settings

setup(
    name='pglib',
    version = get_version(),
    description      = 'Simple DB API module for PostgreSQL',
    maintainer       = 'Michael Kleehammer',
    maintainer_email = 'michael@kleehammer.com',

    ext_modules = [ Extension('pglib', _get_files(), **_get_settings()) ],


    cmdclass = { 'version' : VersionCommand }
)
