#define LOG_TAG "Shadowsocks"

#include "jni.h"
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cpu-features.h>

#define LOGI(...) do { __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGW(...) do { __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__); } while(0)
#define LOGE(...) do { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); } while(0)

jstring Java_com_github_shadowsocks_system_getabi(JNIEnv *env, jobject thiz) {
  AndroidCpuFamily family = android_getCpuFamily();
  uint64_t features = android_getCpuFeatures();
  const char *abi;

  if (family == ANDROID_CPU_FAMILY_X86) {
    abi = "x86";
  } else if (family == ANDROID_CPU_FAMILY_MIPS) {
    abi = "mips";
  } else if (family == ANDROID_CPU_FAMILY_ARM) {
    // if (features & ANDROID_CPU_ARM_FEATURE_ARMv7) {
    abi = "armeabi-v7a";
    // } else {
    //   abi = "armeabi";
    // }
  }
  return env->NewStringUTF(abi);
}

void Java_com_github_shadowsocks_system_exec(JNIEnv *env, jobject thiz, jstring cmd) {
    const char *str  = env->GetStringUTFChars(cmd, 0);
    system(str);
    env->ReleaseStringUTFChars(cmd, str);
}

static const char *classPathName = "com/github/shadowsocks/System";

static JNINativeMethod method_table[] = {
    { "exec", "(Ljava/lang/String;)V",
        (void*) Java_com_github_shadowsocks_system_exec },
    { "getABI", "()Ljava/lang/String;",
        (void*) Java_com_github_shadowsocks_system_getabi }
};



/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        LOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        LOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 *
 * returns JNI_TRUE on success.
 */
static int registerNatives(JNIEnv* env)
{
  if (!registerNativeMethods(env, classPathName, method_table,
                 sizeof(method_table) / sizeof(method_table[0]))) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
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

    LOGI("JNI_OnLoad");

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("ERROR: GetEnv failed");
        goto bail;
    }
    env = uenv.env;

    if (registerNatives(env) != JNI_TRUE) {
        LOGE("ERROR: registerNatives failed");
        goto bail;
    }

    result = JNI_VERSION_1_4;

bail:
    return result;
}
