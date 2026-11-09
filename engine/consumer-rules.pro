# Keep the JNI bridge: native code looks these methods up by name.
-keepclasseswithmembernames class io.github.hypocketapp.engine.** {
    native <methods>;
}
