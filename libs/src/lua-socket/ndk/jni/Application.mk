
APP_PLATFORM := android-16
APP_ABI := arm64-v8a armeabi-v7a x86
APP_OPTIM := release
APP_STL := c++_static
APP_THIN_ARCHIVE := true
APP_CPPFLAGS := -fpic -frtti -fexceptions -Wl,--retain-symbols-file=../../../../src/symbol/libsocket.io.export -Wl,--version-script=../../../../src/symbol/libsocket.io.script
