#include <jni.h>
#include <string.h>
#include <unistd.h>

typedef unsigned short char16_t;

class String8 {
public:
    String8() {
        mString = 0;
    }

    ~String8() {
        if (mString) {
            free(mString);
        }
    }

    void set(const char16_t* o, size_t numChars) {
        if (mString) {
            free(mString);
        }
        mString = (char*) malloc(numChars + 1);
        if (!mString) {
            return;
        }
        for (size_t i = 0; i < numChars; i++) {
            mString[i] = (char) o[i];
        }
        mString[numChars] = '\0';
    }

    const char* string() {
        return mString;
    }
private:
    char* mString;
};

extern "C" {

int main(int argc, char* args[]);

#if defined(PDNSD_JNI)
jint Java_com_github_shadowsocks_Core_pdnsd(JNIEnv *env, jobject thiz, jobjectArray argv) {
#elif defined(SSLOCAL_JNI)
jint Java_com_github_shadowsocks_Core_sslocal(JNIEnv *env, jobject thiz, jobjectArray argv) {
#elif defined(SSTUNNEL_JNI)
jint Java_com_github_shadowsocks_Core_sstunnel(JNIEnv *env, jobject thiz, jobjectArray argv) {
#elif defined(REDSOCKS_JNI)
jint Java_com_github_shadowsocks_Core_redsocks(JNIEnv *env, jobject thiz, jobjectArray argv) {
#elif defined(TUN2SOCKS_JNI)
jint Java_com_github_shadowsocks_Core_tun2socks(JNIEnv *env, jobject thiz, jobjectArray argv) {
#else
#error "invalid header"
#endif

    int argc = argv ? env->GetArrayLength(argv) : 0;
    char **args = NULL;
    String8 tmp_8;

    if (argc > 0) {
        args = (char **)malloc((argc+1)*sizeof(char *));
        for (int i = 0; i < argc; ++i) {
            jstring arg = reinterpret_cast<jstring>(env->GetObjectArrayElement(argv, i));
            const jchar *str = env->GetStringCritical(arg, 0);
            tmp_8.set(str, env->GetStringLength(arg));
            env->ReleaseStringCritical(arg, str);
            args[i] = strdup(tmp_8.string());
        }
        args[argc] = NULL;

        pid_t pid;
        pid = fork();
        if (pid == 0) {
            int ret = main(argc, args);
            return ret;
        }

        if (args != NULL) free(args);
    }

    return 0;
}

}
