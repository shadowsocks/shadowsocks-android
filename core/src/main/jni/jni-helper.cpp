#define LOG_TAG "JniHelper"

#include "jni.h"
#include <android/log.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ancillary.h>

using namespace std;

#define LOGI(...) do { __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGW(...) do { __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGE(...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); } while(0)
#define THROW(env, clazz, msg) do { env->ThrowNew(env->FindClass(clazz), msg); } while (0)

static int sdk_version;
static jclass ProcessImpl;
static jfieldID ProcessImpl_pid, ProcessImpl_exitValue, ProcessImpl_exitValueMutex;

#pragma clang diagnostic ignored "-Wunused-parameter"
extern "C" {
JNIEXPORT jint JNICALL Java_com_github_shadowsocks_JniHelper_sigkill(JNIEnv *env, jobject thiz, jint pid) {
    // Suppress "No such process" errors. We just want the process killed. It's fine if it's already killed.
    return kill(pid, SIGKILL) == -1 && errno != ESRCH ? errno : 0;
}

JNIEXPORT jint JNICALL Java_com_github_shadowsocks_JniHelper_sigterm(JNIEnv *env, jobject thiz, jobject process) {
    if (!env->IsInstanceOf(process, ProcessImpl)) {
        THROW(env, "java/lang/ClassCastException",
                   "Unsupported process object. Only java.lang.ProcessManager$ProcessImpl is accepted.");
        return -1;
    }
    jint pid = env->GetIntField(process, ProcessImpl_pid);
    // Suppress "No such process" errors. We just want the process killed. It's fine if it's already killed.
    return kill(pid, SIGTERM) == -1 && errno != ESRCH ? errno : 0;
}

JNIEXPORT jobject JNICALL
        Java_com_github_shadowsocks_JniHelper_getExitValue(JNIEnv *env, jobject thiz, jobject process) {
    if (!env->IsInstanceOf(process, ProcessImpl)) {
        THROW(env, "java/lang/ClassCastException",
                   "Unsupported process object. Only java.lang.ProcessManager$ProcessImpl is accepted.");
        return NULL;
    }
    return env->GetObjectField(process, ProcessImpl_exitValue);
}

JNIEXPORT jobject JNICALL
        Java_com_github_shadowsocks_JniHelper_getExitValueMutex(JNIEnv *env, jobject thiz, jobject process) {
    if (!env->IsInstanceOf(process, ProcessImpl)) {
        THROW(env, "java/lang/ClassCastException",
                   "Unsupported process object. Only java.lang.ProcessManager$ProcessImpl is accepted.");
        return NULL;
    }
    return env->GetObjectField(process, ProcessImpl_exitValueMutex);
}

JNIEXPORT void JNICALL Java_com_github_shadowsocks_JniHelper_close(JNIEnv *env, jobject thiz, jint fd) {
    close(fd);
}

JNIEXPORT jint JNICALL
        Java_com_github_shadowsocks_JniHelper_sendFd(JNIEnv *env, jobject thiz, jint tun_fd, jstring path) {
    int fd;
    struct sockaddr_un addr;
    const char *sock_str  = env->GetStringUTFChars(path, 0);

    if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        LOGE("socket() failed: %s (socket fd = %d)\n", strerror(errno), fd);
        return (jint)-1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_str, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOGE("connect() failed: %s (fd = %d)\n", strerror(errno), fd);
        close(fd);
        return (jint)-1;
    }

    if (ancil_send_fd(fd, tun_fd)) {
        LOGE("ancil_send_fd: %s", strerror(errno));
        close(fd);
        return (jint)-1;
    }

    close(fd);
    env->ReleaseStringUTFChars(path, sock_str);
    return 0;
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

typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;
    jint result = -1;
    JNIEnv* env = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK) {
        THROW(env, "java/lang/RuntimeException", "GetEnv failed");
        goto bail;
    }
    env = uenv.env;

    char version[PROP_VALUE_MAX + 1];
    __system_property_get("ro.build.version.sdk", version);
    sdk_version = atoi(version);

#define FIND_CLASS(out, name)                                                           \
    if (!(out = env->FindClass(name))) {                                                \
        THROW(env, "java/lang/RuntimeException", name " not found");                    \
        goto bail;                                                                      \
    }                                                                                   \
    out = reinterpret_cast<jclass>(env->NewGlobalRef(reinterpret_cast<jobject>(out)))
#define GET_FIELD(out, clazz, name, sig)                                                                    \
    if (!(out = env->GetFieldID(clazz, name, sig))) {                                                       \
        THROW(env, "java/lang/RuntimeException", "Field " #clazz "." name " with type " sig " not found");  \
        goto bail;                                                                                          \
    }

    if (sdk_version < 24) {
        FIND_CLASS(ProcessImpl, "java/lang/ProcessManager$ProcessImpl");
        GET_FIELD(ProcessImpl_pid, ProcessImpl, "pid", "I")
        GET_FIELD(ProcessImpl_exitValue, ProcessImpl, "exitValue", "Ljava/lang/Integer;")
        GET_FIELD(ProcessImpl_exitValueMutex, ProcessImpl, "exitValueMutex", "Ljava/lang/Object;")
    }

    result = JNI_VERSION_1_6;

bail:
    return result;
}
