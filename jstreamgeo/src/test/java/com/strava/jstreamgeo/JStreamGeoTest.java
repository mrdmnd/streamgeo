package com.strava.jstreamgeo;

import junit.framework.Assert;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * Unit test for simple App.
 */
public class JStreamGeoTest extends TestCase
{
    /**
     * Create the test case
     *
     * @param testName name of the test case
     */
    public JStreamGeoTest( String testName ) {
        super( testName );
    }

    /**
     * @return the suite of tests being tested
     */
    public static Test suite() {
        return new TestSuite( JStreamGeoTest.class );
    }

    /**
     * Rigourous Test :-)
     */
    public void testStreamDistance() {
        int n_points = 8;
        float[] points = {1.0f, 1.0f,
                          1.0f, 2.0f,
                          2.0f, 3.0f,
                          3.0f, 3.0f,
                          4.0f, 3.0f,
                          5.0f, 2.0f,
                          6.0f, 4.0f,
                          4.0f, 4.0f};
        float length = JStreamGeo.JStreamGeoLib.instance.stream_distance(n_points, points);
        System.out.println(length);
        Assert.assertEquals(length, 10.064495, 0.000001);

    }
}
