from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys, os, platform
import setuptools

__version__ = '0.0.1'

class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)

if sys.platform.startswith('win'):
    libsolv_prefix = os.path.join(sys.prefix, 'Library/')
else:
    libsolv_prefix = sys.prefix

print("Looking for libsolv in: ", libsolv_prefix)

extra_link_args = []
if sys.platform == 'darwin':
    extra_link_args = ['-Wl,-rpath', '-Wl,%s' % os.path.abspath(libsolv_prefix)]

ext_modules = [
    Extension(
        'mamba.mamba_api',
        ['include/py_interface.cpp', 'include/parsing.cpp', 'include/solver.cpp', 'include/thirdparty/simdjson/simdjson.cpp'],
        include_dirs=[
            get_pybind_include(),
            get_pybind_include(user=True),
            os.path.join(libsolv_prefix, 'include')
        ],
        libraries=['solv'],
        language='c++'
    ),
]

# As of Python 3.6, CCompiler has a `has_flag` method.
# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.cpp') as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except setuptools.distutils.errors.CompileError:
            return False
    return True

class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc', '/std:c++latest', '/arch:AVX2', '/Ox'],
        'unix': ['-std=c++17', '-march=core-avx2', '-O3'],
    }

    def build_extensions(self):
        ct = self.compiler.compiler_type

        if sys.platform == 'darwin':
            self.c_opts['unix'] += ['-stdlib=libc++', '-mmacosx-version-min=10.7']
            if not has_flag(self.compiler, '-std=c++17'):
                self.c_opts['unix'].remove('-std=c++17')
                self.c_opts['unix'].append('-std=c++1z')

        opts = self.c_opts.get(ct, [])
        if ct == 'unix':
            opts.append('-DVERSION_INFO="%s"' % self.distribution.get_version())
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')
        elif ct == 'msvc':
            opts.append('/DVERSION_INFO=\\"%s\\"' % self.distribution.get_version())

        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)

setup(
    name='mamba',
    version=__version__,
    author='Wolf Vollprecht',
    author_email='w.vollprecht@gmail.com',
    url='https://github.com/wolfv/mamba',
    description='Faster than a conda?!',
    packages=['mamba'],
    entry_points={
        'console_scripts': [
            'mamba = mamba.mamba:main'
        ]
    },
    long_description='A (hopefully faster) reimplementation of the slow bits of conda.',
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.2'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)

