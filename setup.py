#coding:utf8
from setuptools import setup, Extension
import subprocess
import os
import glob
import site
import shutil
from distutils.command.install import install
from distutils.sysconfig import get_python_lib
from setuptools.command.build_ext import build_ext
from setuptools import find_packages
from setuptools.command.install_lib import install_lib
from pkg_resources import resource_filename

build_type = 'debug'
package_name = 'rdc'


class BuildCExt(install):

    def run(self):
        self.build_make()
        package_dir = os.path.join(get_python_lib(), package_name)
        lib_dir = os.path.join(os.getcwd(), 'build', build_type, 'lib')
        dlls = glob.glob(os.path.join(lib_dir, '*.so'))
        subprocess.check_call(['cp', '-a', *dlls, package_dir])
        install.run(self)
        super().run()

    def build_make(self):
        subprocess.check_call([
            'scons', '-j',
            str(os.cpu_count()),
            'BUILD_TYPE=%s' % (build_type)
        ])


setup(
    name=package_name,
    version='1.0',
    author='ankun zheng',
    packages=find_packages(exclude=('tests',)),
    cmdclass={'install': BuildCExt},
)
