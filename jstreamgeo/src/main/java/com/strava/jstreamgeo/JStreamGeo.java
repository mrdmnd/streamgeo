package com.strava.jstreamgeo;

import com.sun.jna.*;

public class JStreamGeo {
    public interface JStreamGeoLib extends Library {
        JStreamGeoLib instance = (JStreamGeoLib) Native.loadLibrary("cstreamgeo", JStreamGeoLib.class);
        float stream_distance(int s_n, float[] s);
    }
}
