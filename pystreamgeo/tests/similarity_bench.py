import pystreamgeo.streamgeo as sg
import random
import time

def perf_test(u_n, v_n, radius, n_times):
    time_accum = 0.0
    for j in range(n_times):
        u = [i * random.random() for i in range(2*u_n)]
        v = [i * random.random() for i in range(2*v_n)]
        start = time.clock()
        sg.align(u, v, 1)
        end = time.clock()
        time_accum += (end - start)
    print "{0} iterations of {1} by {2} alignment took {3} millis, average {4}".format(n_times, u_n, v_n, 1000.0*time_accum, 1000.0*time_accum / n_times)

for i in range(100, 1000, 10):
    perf_test(i, i, 1, 30)
for i in range(1000, 3100, 100):
    perf_test(i, i, 1, 30)
