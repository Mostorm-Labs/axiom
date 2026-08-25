plugins {
    id("com.android.application")
}

val verificationRoot = file("${rootProject.projectDir}/../../../..")
val repositoryRoot = file("${rootProject.projectDir}/../../../../..")

android {
    namespace = "dev.mostorm.axiom.verification.android"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    // Keep native libraries page-aligned in the APK. This is required in
    // addition to ELF PT_LOAD alignment for Android devices with 16 KB pages.
    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }

    defaultConfig {
        applicationId = "dev.mostorm.axiom.verification.android"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1"
        testInstrumentationRunner =
            "dev.mostorm.axiom.verification.android.HarnessInstrumentation"
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DAXIOM_BUILD_VERIFICATION=ON",
                    "-DBUILD_TESTING=OFF",
                    "-DCANVAS_BUILD_POC01=OFF",
                    "-DCANVAS_BUILD_POC02=OFF",
                    "-DCANVAS_BUILD_POC03=OFF",
                    "-DCANVAS_BUILD_POC05=OFF",
                    "-DCANVAS_BUILD_POC06=OFF",
                    "-DCANVAS_BUILD_RF01=OFF",
                )
                cppFlags += listOf("-std=c++20")
                targets += listOf("axiom_verification_android_jni")
            }
        }
        ndk {
            abiFilters += listOf(
                providers.gradleProperty("axiomAndroidAbi").orElse("arm64-v8a").get()
            )
        }
    }

    externalNativeBuild {
        cmake {
            path = file("${repositoryRoot}/CMakeLists.txt")
            version = "3.30.5"
        }
    }

    sourceSets["main"].assets.srcDir(file("${verificationRoot}/platform/v1"))
}
