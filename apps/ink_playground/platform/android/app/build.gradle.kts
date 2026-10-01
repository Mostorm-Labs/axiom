plugins { id("com.android.application") }

android {
    namespace = "dev.mostorm.axiom.inkplayground"
    compileSdk = 35
    ndkVersion = "27.2.12479018"
    defaultConfig {
        applicationId = "dev.mostorm.axiom.inkplayground"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.3"
        externalNativeBuild {
            cmake {
                targets += listOf("axiom_ink_playground_android")
                arguments += listOf(
                    "-DCANVAS_BUILD_POC01=OFF",
                    "-DCANVAS_BUILD_POC02=OFF",
                    "-DCANVAS_BUILD_POC03=OFF",
                    "-DCANVAS_BUILD_RF01=ON",
                    "-DCANVAS_RF01_BUILD_TESTS=OFF",
                    "-DCANVAS_BUILD_RENDER=ON",
                    "-DCANVAS_BUILD_ARC=ON",
                    "-DARC_BUILD_TESTS=OFF",
                    "-DARC_BUILD_EXTERNAL_CONSUMER_TEST=OFF",
                    "-DCANVAS_RENDER_BUILD_TESTS=OFF",
                    "-DCANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH=ON",
                    "-DCANVAS_SKIA_SDK_ROOT=/Users/qing/Desktop/sources/git/deps/axiom/darwin-arm64/skia-sdk/android-arm64-v8a-gles3",
                    "-DCANVAS_POC02_BUILD_PLATFORM_SHELLS=OFF",
                    "-DBUILD_TESTING=OFF",
                    "-DCMAKE_CXX_FLAGS=-Wno-error=missing-field-initializers -Wno-missing-field-initializers"
                )
                cppFlags += listOf("-std=c++20")
            }
        }
        ndk { abiFilters += listOf("arm64-v8a") }
    }
    externalNativeBuild {
        cmake {
            path = file("../../../../../CMakeLists.txt")
            version = "3.30.5"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
