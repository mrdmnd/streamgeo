import ctypes
import platform
import sys

libfilename = "libcstreamgeo.so"
if platform.uname()[0] == "Darwin":
    libfilename = "libcstreamgeo.dylib"
try:
    libcstreamgeo = ctypes.cdll.LoadLibrary(libfilename)
except OSError as e:
    sys.stderr.write('ERROR: cannot open shared library %s\n' % libfilename)
    sys.stderr.write('       Please make sure that the library can be found.\n')
    sys.stderr.write('       For instance, on GNU/Linux, it could be in /usr/lib and /usr/include directories,\n')
    sys.stderr.write('       or in some other directory with the environment variable LD_LIBRARY_PATH correctly set.\n')
    raise e

int_type = ctypes.c_int
float_type = ctypes.c_float
intp_type = ctypes.POINTER(int_type)
floatp_type = ctypes.POINTER(float_type)

libcstreamgeo.align.argtypes = [int_type, floatp_type, int_type, floatp_type, int_type, intp_type, intp_type]
libcstreamgeo.align.restype = float_type

libcstreamgeo.similarity.argtypes = [int_type, floatp_type, int_type, floatp_type, int_type]
libcstreamgeo.similarity.restype = float_type

libcstreamgeo.reduce_by_rdp.argtypes = [int_type, floatp_type, float_type, intp_type, floatp_type]
libcstreamgeo.reduce_by_rdp.restype = None

libcstreamgeo.stream_distance.argtypes = [int_type, floatp_type]
libcstreamgeo.stream_distance.restype = float_type

libcstreamgeo.stream_sparsity.argtypes = [int_type, floatp_type, intp_type, floatp_type]
libcstreamgeo.stream_sparsity.restype = None


