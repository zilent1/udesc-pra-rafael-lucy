# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from distutils.core import setup, Extension
from distutils.command.build import build as _build
from distutils.command.clean import clean as _clean
from distutils.cmd import Command as _Command
from distutils.dep_util import newer_group
import distutils.ccompiler
import os
import glob
import platform
import shutil
import subprocess
import sysconfig
import sys
import unittest

def ext_build_dir(base):
    """Returns the build directory for compiled extensions"""
    pattern = "lib.{platform}-{version[0]}.{version[1]}"
    dirname = pattern.format(platform=sysconfig.get_platform(),
                             version=sys.version_info)
    return os.path.join(base, 'build', dirname)

# Get a compiler object and and strings representing the compiler type and
# CFLAGS.  Add the Python headers include dir to CFLAGS.
compiler = distutils.ccompiler.new_compiler()
cflags = sysconfig.get_config_var('CFLAGS')
cflags = cflags + " -I" + sysconfig.get_path('include')
compiler_type = distutils.ccompiler.get_default_compiler()

# There's no public way to get a string representing the compiler executable
# out of distutils, but the member variable has been in the same place for a
# long time, so violating encapsulation may be ok.
compiler_name = " ".join(compiler.compiler)
make_command = "make" # TODO portability

BASE_DIR        = os.path.abspath(os.path.join(os.pardir, os.pardir))
PARENT_DIR      = os.path.abspath(os.pardir)
CORE_SOURCE_DIR = os.path.join(PARENT_DIR, 'core')
CFEXT_DIR       = 'cfext'
COMMON_SOURCE_DIR    = os.path.join(PARENT_DIR, 'common')
CHARMONIZER_C        = os.path.join(COMMON_SOURCE_DIR, 'charmonizer.c')
CHARMONIZER_EXE_NAME = compiler.executable_filename('charmonizer')
CHARMONIZER_EXE_PATH = os.path.join(os.curdir, CHARMONIZER_EXE_NAME)
CHARMONY_H_PATH      = 'charmony.h'
LIBCLOWNFISH_NAME    = 'libclownfish.a' # TODO portability
LIBCLOWNFISH_PATH    = os.path.abspath(os.path.join(os.curdir, LIBCLOWNFISH_NAME))
AUTOGEN_INCLUDE      = os.path.join('autogen', 'include')
CFC_DIR              = os.path.join(BASE_DIR, 'compiler', 'python')
CFC_BUILD_DIR        = ext_build_dir(os.path.join(CFC_DIR))
PY_BINDING_DIR       = os.path.abspath(os.curdir)
BINDING_FILE         = '_clownfish.c'

c_filepaths = [BINDING_FILE]
paths_to_clean = [
    CHARMONIZER_EXE_PATH,
    CHARMONY_H_PATH,
    '_charm*',
    BINDING_FILE,
]

def _quotify(text):
    text = text.replace('\\', '\\\\')
    text = text.replace('"', '\\"')
    return '"' + text + '"'

class charmony(_Command):
    description = "Build and run charmonizer"
    user_options = []
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass
    def run(self):
        # Compile charmonizer.
        if newer_group([CHARMONIZER_C], CHARMONIZER_EXE_PATH):
            command = [compiler_name]
            if compiler_type == 'msvc':
                command.append('/Fe' + CHARMONIZER_EXE_PATH)
            else:
                command.extend(['-o', CHARMONIZER_EXE_PATH])
            command.append(CHARMONIZER_C)
            print(" ".join(command))
            subprocess.check_call(command)

        # Run charmonizer.
        if newer_group([CHARMONIZER_EXE_PATH], CHARMONY_H_PATH):
            command = [
                CHARMONIZER_EXE_PATH,
                '--cc=' + _quotify(compiler_name),
                '--enable-c',
                '--enable-python',
                '--host=python',
                '--enable-makefile',
                '--',
                cflags
            ]
            if 'CHARM_VALGRIND' in os.environ:
                command[0:0] = "valgrind", "--leak-check=yes";
            print(" ".join(command))
            subprocess.check_call(command)

class libclownfish(_Command):
    description = "Build the Clownfish runtime core as a static archive."
    user_options = []
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass

    def run(self):
        self.run_command('charmony')
        self.run_cfc()
        subprocess.check_call([make_command, '-j', 'static'])

    def run_cfc(self):
        sys.path.append(CFC_DIR)
        sys.path.append(CFC_BUILD_DIR)
        import cfc
        hierarchy = cfc.model.Hierarchy(dest="autogen")
        hierarchy.add_source_dir(CORE_SOURCE_DIR)
        hierarchy.build()
        header = "Autogenerated by setup.py"
        core_binding = cfc.binding.BindCore(hierarchy=hierarchy, header=header)
        modified = core_binding.write_all_modified()
        if modified:
            py_binding = cfc.binding.Python(hierarchy=hierarchy)
            py_binding.set_header(header)
            py_binding.write_bindings(parcel="Clownfish", dest=PY_BINDING_DIR)
            hierarchy.write_log()

class my_clean(_clean):
    def run(self):
        _clean.run(self)
        if os.path.isfile("Makefile"):
            subprocess.check_call([make_command, 'distclean'])
        for elem in paths_to_clean:
            for path in glob.glob(elem):
                print("removing " + path)
                if os.path.isdir(path):
                    shutil.rmtree(path)
                else:
                    os.unlink(path)

class my_build(_build):
    def run(self):
        self.run_command('charmony')
        self.run_command('libclownfish')
        _build.run(self)

class test(_Command):
    description = "Run unit tests."
    user_options = []
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass
    def ext_build_dir(self):
        """Returns the build directory for compiled extensions"""
        pattern = "lib.{platform}-{version[0]}.{version[1]}"
        dirname = pattern.format(platform=sysconfig.get_platform(),
                                 version=sys.version_info)
        return os.path.join('build', dirname)

    def run(self):
        self.run_command('build')
        orig_sys_path = sys.path[:]
        sys.path.append(self.ext_build_dir())

        loader = unittest.TestLoader()
        tests = loader.discover("test")
        test_runner = unittest.runner.TextTestRunner()
        test_runner.run(tests)

        # restore sys.path
        sys.path = orig_sys_path

clownfish_extension = Extension('clownfish._clownfish',
                                 include_dirs = [
                                    CORE_SOURCE_DIR,
                                    AUTOGEN_INCLUDE,
                                    CFEXT_DIR,
                                    os.curdir,
                                 ],
                                 extra_link_args = [LIBCLOWNFISH_PATH],
                                 sources = c_filepaths)

setup(name = 'clownfish',
      version = '0.5.1',
      description = 'Clownfish runtime',
      author = 'Apache Lucy Project',
      author_email = 'dev at lucy dot apache dot org',
      url = 'http://lucy.apache.org',
      packages = ['clownfish',
                 ],
      cmdclass = {
          'build': my_build,
          'clean': my_clean,
          'charmony': charmony,
          'libclownfish': libclownfish,
          'test': test,
      },
      package_dir={'': 'src'},
      ext_modules = [clownfish_extension])

