@ECHO OFF
SETLOCAL

IF NOT DEFINED ANDROID_NDK_HOME (
	SET ANDROID_NDK_HOME=%ANDROID_HOME%\ndk-bundle
)

SET DIR=%~dp0
SET MIN_API=%1%
SET TARGET=%DIR%\bin
SET DEPS=%DIR%\.deps

SET ANDROID_ARM_TOOLCHAIN=%DEPS%\android-toolchain-%MIN_API%-arm
SET ANDROID_ARM64_TOOLCHAIN=%DEPS%\android-toolchain-%MIN_API%-arm64
SET ANDROID_X86_TOOLCHAIN=%DEPS%\android-toolchain-%MIN_API%-x86

SET ANDROID_ARM_CC=%ANDROID_ARM_TOOLCHAIN%\bin\arm-linux-androideabi-clang
SET ANDROID_ARM_STRIP=%ANDROID_ARM_TOOLCHAIN%\bin\arm-linux-androideabi-strip

SET ANDROID_ARM64_CC=%ANDROID_ARM64_TOOLCHAIN%\bin\aarch64-linux-android-clang
SET ANDROID_ARM64_STRIP=%ANDROID_ARM64_TOOLCHAIN%\bin\aarch64-linux-android-strip

SET ANDROID_X86_CC=%ANDROID_X86_TOOLCHAIN%\bin\i686-linux-android-clang
SET ANDROID_X86_STRIP=%ANDROID_X86_TOOLCHAIN%\bin\i686-linux-android-strip

MKDIR %DEPS%>nul 2>nul 
MKDIR %TARGET%\armeabi-v7a>nul 2>nul 
MKDIR %TARGET%\x86>nul 2>nul 
MKDIR %TARGET%\arm64-v8a>nul 2>nul 

IF NOT EXIST %ANDROID_ARM_CC% (
	ECHO "Make standalone toolchain for ARM arch"
    %ANDROID_NDK_HOME%\build\tools\make_standalone_toolchain.py --arch arm ^
        --api %MIN_API% --install-dir %ANDROID_ARM_TOOLCHAIN%
)

IF NOT EXIST %ANDROID_ARM64_CC% (
    ECHO "Make standalone toolchain for ARM64 arch"
    %ANDROID_NDK_HOME%\build\tools\make_standalone_toolchain.py --arch arm64 ^
        --api %MIN_API% --install-dir %ANDROID_ARM64_TOOLCHAIN%
)

IF NOT EXIST %ANDROID_X86_CC% (
    ECHO "Make standalone toolchain for X86 arch"
    %ANDROID_NDK_HOME%\build\tools\make_standalone_toolchain.py --arch x86 ^
        --api %MIN_API% --install-dir %ANDROID_X86_TOOLCHAIN%
)

IF NOT EXIST %DIR%\go\bin\go.exe (
    ECHO "Build the custom go"

    PUSHD %DIR%\go\src
    CALL make.bat
    POPD
)

SET GOROOT=%DIR%\go
SET GOPATH=%DIR%
SET PATH=%GOROOT%\bin;%GOPATH%\bin;%PATH%

SET BUILD=1
IF EXIST "%TARGET%\armeabi-v7a\liboverture.so" (
	IF EXIST "%TARGET%\arm64-v8a\liboverture.so" (
		IF EXIST "%TARGET%\x86\liboverture.so" (
			SET BUILD=0
		)
	)
)

IF %BUILD% == 1 (
	ECHO "Get dependences for overture"
	go.exe get -u github.com\tools\godep

	PUSHD %GOPATH%\src\github.com\shadowsocks\overture\main
	godep.exe restore

	ECHO "Cross compile overture for arm"
	IF NOT EXIST "%TARGET%\armeabi-v7a\liboverture.so" (
		SETLOCAL
	    SET CGO_ENABLED=1
	    SET CC=%ANDROID_ARM_CC%
	    SET GOOS=android
	    SET GOARCH=arm
	    SET GOARM=7
	    go.exe build -ldflags="-s -w"
	    %ANDROID_ARM_STRIP% main
	    MOVE main %TARGET%\armeabi-v7a\liboverture.so>nul 2>nul 
	    ENDLOCAL
	)

	ECHO "Cross compile overture for arm64"
	IF NOT EXIST "%TARGET%\arm64-v8a\liboverture.so" (
		SETLOCAL
	    SET CGO_ENABLED=1
	    SET CC=%ANDROID_ARM64_CC%
	    SET GOOS=android
	    SET GOARCH=arm64
	    go.exe build -ldflags="-s -w"
	    %ANDROID_ARM64_STRIP% main
	    MOVE main %TARGET%\arm64-v8a\liboverture.so>nul 2>nul 
	    ENDLOCAL
	)

	ECHO "Cross compile overture for x86"
	IF NOT EXIST "%TARGET%\x86\liboverture.so" (
		SETLOCAL
	    SET CGO_ENABLED=1
	    SET CC=%ANDROID_X86_CC%
	    SET GOOS=android
	    SET GOARCH=386
	    go.exe build -ldflags="-s -w"
	    %ANDROID_X86_STRIP% main
	    MOVE main %TARGET%\x86\liboverture.so>nul 2>nul 
	    ENDLOCAL
	)

	POPD
)

ECHO "Successfully build overture"
ENDLOCAL