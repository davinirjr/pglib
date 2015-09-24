#!/usr/bin/env python3

import sys, os, re
import subprocess, sysconfig
from os.path import join, abspath, dirname, exists

try:
    from setuptools import setup, Command, Extension
except:
    from distutils.core import setup, Command, Extension
from distutils.command.build_ext import build_ext

long_description = """\
A PostgreSQL interface for Python.

This provides an interface to the libpq library.  It is not an DB API
library, but is instead designed to match the interface PostgreSQL
offers.
"""

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
        match = re.search(r'^Version:\s+(\d+\.\d+\.\d+(?:b\d+)?)', open(filename).read())
        if match:
            return match.group(1)

    # If not a source release, we should be in a git repository.  Look for the latest tag.

    n, result = getoutput("git describe --tags --match '[0-9]*'")
    if n:
        print('WARNING: git describe failed with: %s %s' % (n, result))
        sys.exit('Unable to determine version.')

    match = re.match(r'(\d+).(\d+).(\d+) (?: -(\d+)-g[0-9a-z]+)?', result, re.VERBOSE)
    if not match:
        sys.exit('Unable to determine version.')

    parts      = [ int(n or 0) for n in match.groups() ]
    prerelease = ''

    if parts[-1] > 0:
        # We are building the next version, so increment the patch and add the
        # build number.
        parts[-2] += 1
        prerelease = 'b%d' % parts[-1]

    return '.'.join(str(part) for part in parts[:3]) + prerelease


def _get_osx_sdkpath():
    """
    Use xcodebuild to find the latest installed OS X SDK and return the path to it.
    """
    # The output is in blank line separated sections.  Find the section for the latest OS/X SDK
    # and get the Path entry.

    output = subprocess.check_output(['xcodebuild', '-version', '-sdk']).strip().decode('utf-8')

    highest = (0,0)
    path    = None

    # MacOSX10.8.sdk - OS X 10.8 (macosx10.8)
    resection = re.compile('^.*- OS X (\d+)\.(\d+)')
    repath = re.compile('^Path: ([^\n]+)', re.MULTILINE)

    for section in output.split('\n\n'):
        match = resection.match(section)
        if not match:
            continue

        version = tuple(int(g) for g in match.groups())
        if version > highest:
            highest = version
            path = repath.search(section).group(1)


    if highest == (0,0):
        sys.exit('No OS X SDKs installed?  xcodebuild returned {!r}'.format(output))

    return path


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
            # TODO: The build command already has debug. Pass it in.
            sys.argv.remove('--debug')
            settings['extra_compile_args'].extend('/Od /Ge /GS /GZ /RTC1 /Wp64 /Yd'.split())

        settings['libraries'] = ['libpq', 'Ws2_32']


    elif sys.platform == 'darwin':
        # Apple is not making it easy for non-Xcode builds.  We'll always build with the latest
        # SDK we can find but we'll set the version we are targeting to the same one that
        # Python was built with.

        sdkpath = _get_osx_sdkpath()

        settings['include_dirs'] = [
            join(sdkpath, 'usr', 'include'),
            abspath(dirname(sysconfig.get_config_h_filename().strip())),
            subprocess.check_output(['pg_config', '--includedir']).strip().decode('utf-8')
        ]
        settings['library_dirs'] = [ subprocess.check_output(['pg_config', '--libdir']).strip().decode('utf-8') ]
        settings['libraries']    = ['pq']

        settings['define_macros'].append( ('MAC_OS_X_VERSION_10_7',) )

        settings['extra_compile_args'] = ['-Wall']


    else:
        # Other posix-like: Linux, Solaris, etc.

        settings['include_dirs'] = [ subprocess.check_output(['pg_config', '--includedir']).strip().decode('utf-8') ]
        settings['library_dirs'] = [ subprocess.check_output(['pg_config', '--libdir']).strip().decode('utf-8') ]
        settings['libraries']    = ['pq']

        # Python functions take a lot of 'char *' that really should be const.  gcc complains about this *a lot*
        settings['extra_compile_args'] = ['-Wno-write-strings']

    return settings

setup(
    name='pglib',
    version = get_version(),
    description      = 'A PostgreSQL interface',
    long_description = long_description,
    maintainer       = 'Michael Kleehammer',
    maintainer_email = 'michael@kleehammer.com',

    packages = ['pglib'],
    ext_modules = [ Extension('_pglib', _get_files(), **_get_settings()) ],

    cmdclass = { 'version' : VersionCommand },

    keywords = 'postgresql postgres',

    url='https://github.com/mkleehammer/pglib',
    license='MIT',
    classifiers=[
        # https://pypi.python.org/pypi?%3Aaction=list_classifiers
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Database',
        'License :: OSI Approved :: MIT License',

        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: 3.4',
    ],
)
