#include <jni.h>

void Java_com_github_shadowsocks_Node_exec(JNIEnv *env, jobject thiz, jstring cmd) {
    const char *str  = (*env)->GetStringUTFChars(env, cmd, 0);
    setenv("LD_LIBRARY_PATH", "/vendor/lib:/system/lib", 1);
    setegid(getgid());
    seteuid(getuid());
    system(str);
    (*env)->ReleaseStringUTFChars(env, cmd, str);
}
