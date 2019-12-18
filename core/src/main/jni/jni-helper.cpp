#include <utility>
#include <vector>

#include "jni.h"
#include "re2/re2.h"

using namespace std;

struct AclMatcher {
    vector<RE2 *> bypassDomains, proxyDomains;
    ~AclMatcher() {
        for (RE2 *re2 : bypassDomains) delete re2;
        for (RE2 *re2 : proxyDomains) delete re2;
    }
};

jstring addDomain(JNIEnv *env, vector<RE2 *> &domains, jstring regex) {
    const char *regexChars = env->GetStringUTFChars(regex, nullptr);
    RE2 *re2 = new RE2(regexChars);
    env->ReleaseStringUTFChars(regex, regexChars);
    if (re2->ok()) {
        domains.push_back(re2);
        return nullptr;
    } else {
        auto result = env->NewStringUTF(re2->error().c_str());
        delete re2;
        return result;
    }
}

bool matches(const vector<RE2 *> &domains, const char *host) {
    return any_of(domains.cbegin(), domains.cend(), [host](const RE2 *re){
        return RE2::FullMatch(host, *re);
    });
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
extern "C" {
JNIEXPORT jlong JNICALL Java_com_github_shadowsocks_acl_AclMatcher_init(JNIEnv *env, jclass clazz) {
    return reinterpret_cast<jlong>(new AclMatcher());
}

JNIEXPORT void JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_close(JNIEnv *env, jclass clazz, jlong handle) {
    delete reinterpret_cast<AclMatcher *>(handle);
}

JNIEXPORT jstring JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_addBypassDomain(JNIEnv *env, jclass clazz, jlong handle,
                                                           jstring regex) {
    if (!handle) return env->NewStringUTF("AclMatcher closed");
    return ::addDomain(env, reinterpret_cast<AclMatcher *>(handle)->bypassDomains, regex);
}

JNIEXPORT jstring JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_addProxyDomain(JNIEnv *env, jclass clazz, jlong handle,
                                                          jstring regex) {
    if (!handle) return env->NewStringUTF("AclMatcher closed");
    return ::addDomain(env, reinterpret_cast<AclMatcher *>(handle)->proxyDomains, regex);
}

JNIEXPORT jint JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_matchHost(JNIEnv *env, jclass clazz, jlong handle,
                                                     jstring host) {
    if (!handle) return -1;
    auto matcher = reinterpret_cast<const AclMatcher *>(handle);
    const char *hostChars = env->GetStringUTFChars(host, nullptr);
    jint result = 0;
    if (::matches(matcher->bypassDomains, hostChars)) result = 1;
    else if (::matches(matcher->proxyDomains, hostChars)) result = 2;
    env->ReleaseStringUTFChars(host, hostChars);
    return result;
}
}
