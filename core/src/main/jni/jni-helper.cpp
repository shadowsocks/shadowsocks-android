#define LOG_TAG "JniHelper"

#include "jni.h"
#include <android/log.h>
#include <sys/system_properties.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ancillary.h>

#define LOGI(...) do { __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGW(...) do { __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGE(...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); } while(0)
#define THROW(env, clazz, msg) do { env->ThrowNew(env->FindClass(clazz), msg); } while (0)

static jclass ProcessImpl;
static jfieldID ProcessImpl_pid, ProcessImpl_exitValue, ProcessImpl_exitValueMutex;

static int sdk_version() {
    char version[PROP_VALUE_MAX + 1];
    __system_property_get("ro.build.version.sdk", version);
    return atoi(version);
}

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
}

/*
 * This is called by the VM when the shared library is first loaded.
 */

typedef union {
    JNIEnv* env;
    void* venv;
} UnionJNIEnvToVoid;

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

    if (sdk_version() < 24) {
        if (!(ProcessImpl = env->FindClass("java/lang/ProcessManager$ProcessImpl"))) {
            THROW(env, "java/lang/RuntimeException", "ProcessManager$ProcessImpl not found");
            goto bail;
        }
        ProcessImpl = (jclass) env->NewGlobalRef((jobject) ProcessImpl);
        if (!(ProcessImpl_pid = env->GetFieldID(ProcessImpl, "pid", "I"))) {
            THROW(env, "java/lang/RuntimeException", "ProcessManager$ProcessImpl.pid not found");
            goto bail;
        }
        if (!(ProcessImpl_exitValue = env->GetFieldID(ProcessImpl, "exitValue", "Ljava/lang/Integer;"))) {
            THROW(env, "java/lang/RuntimeException", "ProcessManager$ProcessImpl.exitValue not found");
            goto bail;
        }
        if (!(ProcessImpl_exitValueMutex = env->GetFieldID(ProcessImpl, "exitValueMutex", "Ljava/lang/Object;"))) {
            THROW(env, "java/lang/RuntimeException", "ProcessManager$ProcessImpl.exitValueMutex not found");
            goto bail;
        }
    }

    result = JNI_VERSION_1_6;

bail:
    return result;
}
