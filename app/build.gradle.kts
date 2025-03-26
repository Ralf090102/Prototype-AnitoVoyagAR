plugins {
    id("com.android.application")
}

android {
    compileSdk = 35
    ndkVersion = "28.0.13004108"
    namespace = "org.dlsugamelab.AnitoVoyagARMobile"

    defaultConfig {
        minSdk = 30
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        applicationId = "org.dlsugamelab.AnitoVoyagARMobile"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                arguments.add("-DANDROID_STL=c++_shared")
                arguments.add("-DANDROID_USE_LEGACY_TOOLCHAIN_FILE=OFF")
            }
            ndk {
                abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            }
        }
    }
    lint {
        disable.add("ExpiredTargetSdkVersion")
    }
    buildTypes {
        release {
            isDebuggable = false
            isJniDebuggable = false
        }
        debug {
            isDebuggable = true
            isJniDebuggable = true
        }
    }
    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }
    sourceSets {
        getByName("main") {
            res.srcDirs("src/main/res")
            java.srcDirs("src/main/java")
            assets.srcDirs("src/main/assets")
        }
        getByName("debug") {
            jniLibs {
                srcDir("libs/debug")
            }
        }
        getByName("release") {
            jniLibs.srcDir("libs/release")
        }
    }
    packaging {
        jniLibs {
            keepDebugSymbols.add("**.so")
        }
    }
    buildFeatures {
        viewBinding = true
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.constraintlayout)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}