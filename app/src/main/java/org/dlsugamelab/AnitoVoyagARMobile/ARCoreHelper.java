package org.dlsugamelab.AnitoVoyagARMobile;

import android.content.Context;
import android.graphics.Bitmap;
import android.media.Image;
import android.opengl.Matrix;
import android.util.Log;

import com.google.ar.core.ArCoreApk;
import com.google.ar.core.Camera;
import com.google.ar.core.Config;
import com.google.ar.core.DepthPoint;
import com.google.ar.core.Frame;
import com.google.ar.core.Pose;
import com.google.ar.core.Session;
import com.google.ar.core.TrackingState;
import com.google.ar.core.exceptions.CameraNotAvailableException;
import com.google.ar.core.exceptions.NotYetAvailableException;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;

/**
 * Helper class to manage ARCore functionality for the AR application.
 * Acts as a bridge between the native C++ code and the ARCore Java API.
 */
public class ARCoreHelper {
    private static final String TAG = "ARCoreHelper";

    // ARCore objects
    private Session session;
    private Frame lastFrame;
    private boolean sessionPaused = true;

    // Camera image dimensions
    private int cameraWidth = 0;
    private int cameraHeight = 0;

    // Depth image dimensions
    private int depthWidth = 0;
    private int depthHeight = 0;

    // Camera transform
    private final float[] cameraPoseMatrix = new float[16];
    private final float[] viewMatrix = new float[16];

    // Cached data for native access
    private byte[] latestCameraImage;
    private byte[] latestDepthImage;
    private boolean isTracking = false;

    /**
     * Initialize ARCore session with the given context
     */
    public boolean initialize(Context context) {
        try {
            // Check ARCore availability on this device using ArCoreApk instead of deprecated Session.isSupported()
            ArCoreApk.Availability availability = ArCoreApk.getInstance().checkAvailability(context);
            if (availability.isSupported()) {
                // Create the ARCore session
                session = new Session(context);

                // Configure the session for AR
                Config config = session.getConfig();
                config.setDepthMode(Config.DepthMode.AUTOMATIC);
                config.setUpdateMode(Config.UpdateMode.LATEST_CAMERA_IMAGE);
                config.setFocusMode(Config.FocusMode.AUTO);
                session.configure(config);

                return true;
            } else {
                Log.e(TAG, "ARCore is not supported on this device");
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize ARCore", e);
            return false;
        }
    }

    /**
     * Resume the ARCore session
     */
    public void onResume() {
        if (session == null) {
            Log.e(TAG, "Cannot resume, session is null");
            return;
        }

        try {
            session.resume();
            sessionPaused = false;
        } catch (CameraNotAvailableException e) {
            Log.e(TAG, "Camera not available during onResume", e);
            sessionPaused = true;
        }
    }

    /**
     * Pause the ARCore session
     */
    public void onPause() {
        if (session == null) {
            Log.e(TAG, "Cannot pause, session is null");
            return;
        }

        session.pause();
        sessionPaused = true;
    }

    /**
     * Update ARCore and process the latest frame
     */
    public void update() {
        if (session == null || sessionPaused) {
            return;
        }

        try {
            // Update the ARCore session
            lastFrame = session.update();

            // Get camera and update tracking state
            Camera camera = lastFrame.getCamera();
            isTracking = camera.getTrackingState() == TrackingState.TRACKING;

            if (isTracking) {
                // Get the camera pose
                Pose cameraPose = camera.getPose();
                cameraPose.toMatrix(cameraPoseMatrix, 0);

                // Compute view matrix (inverse of camera pose)
                Matrix.invertM(viewMatrix, 0, cameraPoseMatrix, 0);

                // Update camera image
                updateCameraImage();

                // Update depth image if available
                updateDepthImage();
            }
        } catch (Exception e) {
            Log.e(TAG, "Exception during ARCore update", e);
        }
    }

    /**
     * Update the camera image from the current frame
     */
    private void updateCameraImage() {
        try {
            if (lastFrame == null) return;

            // Get the camera image
            try (Image cameraImage = lastFrame.acquireCameraImage()) {
                if (cameraImage == null) return;

                // Get image dimensions
                cameraWidth = cameraImage.getWidth();
                cameraHeight = cameraImage.getHeight();

                // Convert YUV to RGB
                byte[] imageBytes = convertYUVToRGBA(cameraImage);
                if (imageBytes != null) {
                    latestCameraImage = imageBytes;
                }
            } catch (NotYetAvailableException e) {
                Log.d(TAG, "Camera image not yet available");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error capturing camera image", e);
        }
    }

    /**
     * Update the depth image from the current frame
     */
    private void updateDepthImage() {
        try {
            if (lastFrame == null) return;

            // Get the depth image
            try (Image depthImage = lastFrame.acquireDepthImage16Bits()) {
                if (depthImage == null) return;

                depthWidth = depthImage.getWidth();
                depthHeight = depthImage.getHeight();

                // Get the depth data
                latestDepthImage = convertDepthMapToBytes(depthImage);
            } catch (NotYetAvailableException e) {
                // Depth may not be available every frame, this is normal
                Log.d(TAG, "Depth not yet available");
            }
        } catch (Exception e) {
            Log.e(TAG, "Error capturing depth image", e);
        }
    }

    /**
     * Convert YUV image to RGBA byte array
     */
    private byte[] convertYUVToRGBA(Image image) {
        if (image.getFormat() != android.graphics.ImageFormat.YUV_420_888) {
            Log.e(TAG, "Unexpected camera image format: " + image.getFormat());
            return null;
        }

        Image.Plane[] planes = image.getPlanes();
        ByteBuffer yBuffer = planes[0].getBuffer();
        ByteBuffer uBuffer = planes[1].getBuffer();
        ByteBuffer vBuffer = planes[2].getBuffer();

        int ySize = yBuffer.remaining();
        int uSize = uBuffer.remaining();
        int vSize = vBuffer.remaining();

        byte[] data = new byte[image.getWidth() * image.getHeight() * 4]; // RGBA

        int yRowStride = planes[0].getRowStride();
        int yPixelStride = planes[0].getPixelStride();
        int uvRowStride = planes[1].getRowStride();
        int uvPixelStride = planes[1].getPixelStride();

        int rgbaIndex = 0;

        for (int y = 0; y < image.getHeight(); y++) {
            for (int x = 0; x < image.getWidth(); x++) {
                int yIndex = y * yRowStride + x * yPixelStride;
                // UV values are subsampled, i.e., each UV entry corresponds to 2x2 Y values
                int uvIndex = (y / 2) * uvRowStride + (x / 2) * uvPixelStride;

                // YUV to RGB conversion
                int yValue = yBuffer.get(yIndex) & 0xFF;
                int uValue = uBuffer.get(uvIndex) & 0xFF;
                int vValue = vBuffer.get(uvIndex) & 0xFF;

                yValue = yValue - 16;
                uValue = uValue - 128;
                vValue = vValue - 128;

                // YUV to RGB formula
                int r = (int)(1.164 * yValue + 1.596 * vValue);
                int g = (int)(1.164 * yValue - 0.813 * vValue - 0.391 * uValue);
                int b = (int)(1.164 * yValue + 2.018 * uValue);

                // Clamp RGB values to 0-255
                r = Math.min(Math.max(r, 0), 255);
                g = Math.min(Math.max(g, 0), 255);
                b = Math.min(Math.max(b, 0), 255);

                // Set RGBA values
                data[rgbaIndex++] = (byte) r;
                data[rgbaIndex++] = (byte) g;
                data[rgbaIndex++] = (byte) b;
                data[rgbaIndex++] = (byte) 255; // Alpha
            }
        }

        return data;
    }

    /**
     * Convert depth image to byte array
     */
    private byte[] convertDepthMapToBytes(Image depthImage) {
        Image.Plane plane = depthImage.getPlanes()[0];
        ByteBuffer buffer = plane.getBuffer();

        // Each depth pixel is stored as a 16-bit unsigned short
        int bytesPerPixel = 2;
        int totalBytes = depthWidth * depthHeight * bytesPerPixel;
        byte[] bytes = new byte[totalBytes];

        // Copy the depth data
        buffer.rewind();
        buffer.get(bytes);

        return bytes;
    }

    /**
     * Get the current camera image as a byte array in RGBA format
     * This method is called from native code
     */
    public byte[] getCameraFrame() {
        return latestCameraImage;
    }

    /**
     * Get the current camera pose matrix as a float array
     * This method is called from native code
     */
    public float[] getCameraPose() {
        return cameraPoseMatrix;
    }

    /**
     * Get the current depth image as a byte array
     * This method is called from native code
     */
    public byte[] getDepthImage() {
        return latestDepthImage;
    }

    /**
     * Get the current tracking state
     * This method is called from native code
     */
    public boolean getTrackingState() {
        return isTracking;
    }

    /**
     * Get the camera width
     */
    public int getCameraWidth() {
        return cameraWidth;
    }

    /**
     * Get the camera height
     */
    public int getCameraHeight() {
        return cameraHeight;
    }

    /**
     * Get the depth image width
     */
    public int getDepthWidth() {
        return depthWidth;
    }

    /**
     * Get the depth image height
     */
    public int getDepthHeight() {
        return depthHeight;
    }

    /**
     * Cleanup resources
     */
    public void close() {
        if (session != null) {
            session.close();
            session = null;
        }
    }
}
