#include "jni.h"

#include <algorithm>
#include <cerrno>

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <ancillary.h>

using namespace std;

// Based on: https://android.googlesource.com/platform/libcore/+/564c7e8/luni/src/main/native/libcore_io_Linux.cpp#256
static void throwException(JNIEnv* env, jclass exceptionClass, jmethodID ctor2, const char* functionName, int error) {
    jstring detailMessage = env->NewStringUTF(functionName);
    if (detailMessage == NULL) {
        // Not really much we can do here. We're probably dead in the water,
        // but let's try to stumble on...
        env->ExceptionClear();
    }
    env->Throw(reinterpret_cast<jthrowable>(env->NewObject(exceptionClass, ctor2, detailMessage, error)));
    env->DeleteLocalRef(detailMessage);
}

static void throwErrnoException(JNIEnv* env, const char* functionName) {
    int error = errno;
    static jclass ErrnoException = static_cast<jclass>(env->NewGlobalRef(
            env->FindClass("android/system/ErrnoException")));
    static jmethodID ctor2 = env->GetMethodID(ErrnoException, "<init>", "(Ljava/lang/String;I)V");
    throwException(env, ErrnoException, ctor2, functionName, error);
}

#pragma clang diagnostic ignored "-Wunused-parameter"
extern "C" {
JNIEXPORT void JNICALL
        Java_com_github_shadowsocks_JniHelper_sendFd(JNIEnv *env, jobject thiz, jint tun_fd, jstring path) {
    int fd;
    struct sockaddr_un addr;
    const char *sock_str = env->GetStringUTFChars(path, 0);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        throwErrnoException(env, "socket");
        goto quit2;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_str, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        throwErrnoException(env, "connect");
        goto quit;
    }

    if (ancil_send_fd(fd, tun_fd)) throwErrnoException(env, "ancil_send_fd");

quit:
    close(fd);
quit2:
    env->ReleaseStringUTFChars(path, sock_str);
    return;
}

JNIEXPORT jbyteArray JNICALL
Java_com_github_shadowsocks_JniHelper_parseNumericAddress(JNIEnv *env, jobject thiz, jstring str) {
    const char *src = env->GetStringUTFChars(str, 0);
    jbyte dst[max(sizeof(in_addr), sizeof(in6_addr))];
    jbyteArray arr = nullptr;
    if (inet_pton(AF_INET, src, dst) == 1) {
        arr = env->NewByteArray(sizeof(in_addr));
        env->SetByteArrayRegion(arr, 0, sizeof(in_addr), dst);
    } else if (inet_pton(AF_INET6, src, dst) == 1) {
        arr = env->NewByteArray(sizeof(in6_addr));
        env->SetByteArrayRegion(arr, 0, sizeof(in6_addr), dst);
    }
    env->ReleaseStringUTFChars(str, src);
    return arr;
}
}

/*
 * This is called by the VM when the shared library is first loaded.
 */
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}
