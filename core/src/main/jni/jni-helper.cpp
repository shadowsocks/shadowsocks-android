#include "jni.h"

#include <algorithm>
#include <cerrno>

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <ancillary.h>

using namespace std;

// return -1 for socket error
// return -2 for connect error
// return -3 for ancil_send_fd error
#pragma clang diagnostic ignored "-Wunused-parameter"
extern "C" {
JNIEXPORT jintArray JNICALL
Java_com_github_shadowsocks_JniHelper_sendFdInternal(JNIEnv *env, jobject thiz, jint tun_fd, jstring path) {
    int fd;
    struct sockaddr_un addr;
    const char *sock_str = env->GetStringUTFChars(path, 0);
    jint info[2] = {0};

    jintArray ret = env->NewIntArray(2);
    if (ret == nullptr) {
        goto quit3;
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        info[0] = -1;
        info[1] = errno;
        goto quit2;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_str, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        info[0] = -2;
        info[1] = errno;
        goto quit;
    }

    if (ancil_send_fd(fd, tun_fd)) {
        info[0] = -3;
        info[1] = errno;
    }

quit:
    close(fd);
quit2:
    env->ReleaseStringUTFChars(path, sock_str);
    env->SetIntArrayRegion(ret, 0, 2, info);
quit3:
    return ret;
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
