from .typemap import *
import ctypes

def stream_distance(stream):
    stream_len = len(stream)
    n = stream_len // 2
    input_n = int_type(n)
    input_stream = (float_type * stream_len)()
    for i in range(stream_len):
        input_stream[i] = stream[i]
    return libcstreamgeo.stream_distance(input_n, ctypes.cast(input_stream, floatp_type))

def stream_sparsity(stream):
    stream_len = len(stream)
    n = stream_len // 2
    input_n = int_type(n)
    input_stream = (float_type * stream_len)()
    for i in range(stream_len):
        input_stream[i] = stream[i]
    output_n = ctypes.pointer(int_type(n))
    output_stream = ctypes.cast((float_type * n)(), floatp_type)
    libcstreamgeo.stream_sparsity(input_n, input_stream, output_n, output_stream)
    return [output_stream[i] for i in range(n)]

def reduce_by_rdp(stream, epsilon):
    stream_len = len(stream)
    n = stream_len // 2
    input_n = int_type(n)
    input_stream = (float_type * stream_len)()
    for i in range(stream_len):
        input_stream[i] = stream[i]
    epsilon = float_type(epsilon)
    output_n = ctypes.pointer(int_type(n))
    output_stream = ctypes.cast((float_type * 2*n)(), floatp_type)
    libcstreamgeo.reduce_by_rdp(input_n, input_stream, epsilon, output_n, output_stream)
    return [output_stream[i] for i in range(2*output_n.contents.value)]

def align(stream1, stream2, radius):
    stream_len_1 = len(stream1)
    n_1 = stream_len_1 // 2
    input_n_1 = int_type(n_1)
    input_stream_1 = (float_type * stream_len_1)()
    for i in range(stream_len_1):
        input_stream_1[i] = stream1[i]

    stream_len_2 = len(stream2)
    n_2 = stream_len_2 // 2
    input_n_2 = int_type(n_2)
    input_stream_2 = (float_type * stream_len_2)()
    for i in range(stream_len_2):
        input_stream_2[i] = stream2[i]

    radius = int_type(radius)

    output_n = ctypes.pointer(int_type(n_1 + n_2 - 1))
    output_stream = ctypes.cast((int_type * 2*(n_1 + n_2 - 1))(), intp_type)
    cost = libcstreamgeo.align(input_n_1, input_stream_1, input_n_2, input_stream_2, radius, output_n, output_stream)
    return (cost, [output_stream[i] for i in range(2*output_n.contents.value)])

def similarity(stream1, stream2, radius):
    stream_len_1 = len(stream1)
    n_1 = stream_len_1 // 2
    input_n_1 = int_type(n_1)
    input_stream_1 = (float_type * stream_len_1)()
    for i in range(stream_len_1):
        input_stream_1[i] = stream1[i]

    stream_len_2 = len(stream2)
    n_2 = stream_len_2 // 2
    input_n_2 = int_type(n_2)
    input_stream_2 = (float_type * stream_len_2)()
    for i in range(stream_len_2):
        input_stream_2[i] = stream2[i]

    radius = int_type(radius)

    return libcstreamgeo.similarity(input_n_1, input_stream_1, input_n_2, input_stream_2, radius)

