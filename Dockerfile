FROM phusion/baseimage:0.9.19
MAINTAINER Matthew Redmond mredmond@strava.com
CMD ["/sbin/my_init"]

RUN apt-get update && apt-get install -y build-essential git cmake
# Install RoaringBitmap library
RUN git clone https://github.com/RoaringBitmap/CRoaring.git && cd CRoaring && mkdir build && cd build && cmake .. && make -j && make install && cd ..
RUN ldconfig
ADD CMakeLists.txt /tmp/CMakeLists.txt
ADD cstreamgeo/ /tmp/cstreamgeo/
ADD jstreamgeo/ /tmp/jstreamgeo
ADD pystreamgeo/ /tmp/pystreamgeo
WORKDIR /tmp/
RUN mkdir build && cd build && cmake .. && make && make install
CMD ["./tmp/build/cstreamgeo/filter_test"]
CMD ["./tmp/build/cstreamgeo/similarity_test"]
CMD ["./tmp/build/cstreamgeo/stream_test"]

