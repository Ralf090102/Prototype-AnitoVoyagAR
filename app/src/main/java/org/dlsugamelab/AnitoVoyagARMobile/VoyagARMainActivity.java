package org.dlsugamelab.AnitoVoyagARMobile;

import android.Manifest;
import android.app.NativeActivity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.NonNull;

public class VoyagARMainActivity extends NativeActivity {
    static {
        System.loadLibrary("Anito-VoyagAR");
    }

    private static final int REQUEST_CAMERA_PERMISSION = 100;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkAndRequestCameraPermission();
    }

    private void checkAndRequestCameraPermission() {
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.CAMERA}, REQUEST_CAMERA_PERMISSION);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CAMERA_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.i("MyNativeActivity", "Camera permission granted");
                nativeOnCameraPermissionGranted();
            } else {
                Log.e("MyNativeActivity", "Camera permission denied");
            }
        }
    }

    // Native method to notify C++ code that permission was granted
    public native void nativeOnCameraPermissionGranted();
}