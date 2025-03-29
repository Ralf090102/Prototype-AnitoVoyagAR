package org.dlsugamelab.AnitoVoyagARMobile;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

public class VoyagARMainActivity extends AppCompatActivity {
    private static final String TAG = "VoyagARMainActivity";
    private static final int CAMERA_PERMISSION_CODE = 0;

    private ARCoreHelper arCoreHelper;
    private boolean arCoreInitialized = false;

    static {
        // Load native libraries
        System.loadLibrary("Anito-VoyagAR");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initialize ARCoreHelper
        arCoreHelper = new ARCoreHelper();

        // Request camera permission if not granted
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA}, CAMERA_PERMISSION_CODE);
        } else {
            onCameraPermissionGranted();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == CAMERA_PERMISSION_CODE) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) {
                onCameraPermissionGranted();
            } else {
                Toast.makeText(this, "Camera permission is required for AR", Toast.LENGTH_LONG).show();
                finish(); // Exit if camera permission denied
            }
        }
    }

    private void onCameraPermissionGranted() {
        // Notify native code
        nativeOnCameraPermissionGranted();

        // Initialize ARCore
        arCoreInitialized = arCoreHelper.initialize(this);
        if (!arCoreInitialized) {
            Toast.makeText(this, "Failed to initialize ARCore", Toast.LENGTH_LONG).show();
            finish();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (arCoreInitialized) {
            arCoreHelper.onResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (arCoreInitialized) {
            arCoreHelper.onPause();
        }
    }

    @Override
    protected void onDestroy() {
        if (arCoreInitialized) {
            arCoreHelper.close();
        }
        super.onDestroy();
    }

    // This method is called from the native side whenever the rendering thread updates
    public void updateARCore() {
        if (arCoreInitialized) {
            arCoreHelper.update();
        }
    }

    // Native methods
    public native void nativeOnCameraPermissionGranted();
}
