import os
import sys
import ctypes
import numpy as np
import subprocess

def _find_lib_path(dll_name):
    """Find the rdc dynamic library files.

    Returns
    -------
    lib_path: list(string)
       List of all found library path to rdc
    """
    curr_path = os.path.dirname(os.path.abspath(os.path.expanduser(__file__)))
    # make pythonpack hack: copy this directory one level upper for setup.py
    dll_path = [
        curr_path,
        os.path.join(curr_path, '../lib/'),
        os.path.join(curr_path, './lib/')
    ]
    if os.name == 'nt':
        dll_path = [os.path.join(p, dll_name) for p in dll_path]
    else:
        dll_path.append('/usr/local/lib')
        dll_path = [os.path.join(p, dll_name) for p in dll_path]
    lib_path = [p for p in dll_path if os.path.exists(p) and os.path.isfile(p)]
    if len(lib_path) == 0:
        raise RuntimeError(
            'Cannot find Rdc Libarary in the candicate path, ' +
            'did you install compilers and run build.sh in root path?\n'
            'List of candidates:\n' + ('\n'.join(dll_path)))
    return lib_path


def _load_lib():
    py_ext_suffix = subprocess.check_output(
        ['python-config', '--extension-suffix'])
    py_ext_suffix = py_ext_suffix.decode('utf-8').strip()
    dll_name = 'pyrdc' + py_ext_suffix
    sys.path.append(os.path.dirname(_find_lib_path(dll_name)[0]))


def _unload_lib():
    """Unload rdc library."""
    pass


#library instance
_load_lib()
import atexit
atexit.register(_unload_lib)
# type definitions
rdc_uint = ctypes.c_uint
rdc_float = ctypes.c_float
rdc_float_p = ctypes.POINTER(rdc_float)
rdc_real_t = np.float32
