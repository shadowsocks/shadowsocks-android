#include <sstream>
#include <vector>

#include "jni.h"
#include "re2/re2.h"

using namespace std;

struct AclMatcher {
    stringstream bypassDomainsBuilder, proxyDomainsBuilder;
    RE2 *bypassDomains, *proxyDomains;

    ~AclMatcher() {
        if (bypassDomains) delete bypassDomains;
        if (proxyDomains) delete proxyDomains;
    }
};

bool addDomain(JNIEnv *env, stringstream &domains, jstring regex) {
    const char *regexChars = env->GetStringUTFChars(regex, nullptr);
    if (regexChars == nullptr) return false;
    if (domains.rdbuf()->in_avail()) domains << '|';
    domains << regexChars;
    env->ReleaseStringUTFChars(regex, regexChars);
    return true;
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

JNIEXPORT jboolean JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_addBypassDomain(JNIEnv *env, jclass clazz, jlong handle,
                                                           jstring regex) {
    return static_cast<jboolean>(handle &&
                                 ::addDomain(env, reinterpret_cast<AclMatcher *>(handle)->bypassDomainsBuilder, regex));
}

JNIEXPORT jboolean JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_addProxyDomain(JNIEnv *env, jclass clazz, jlong handle,
                                                          jstring regex) {
    return static_cast<jboolean>(handle &&
                                 ::addDomain(env, reinterpret_cast<AclMatcher *>(handle)->proxyDomainsBuilder, regex));
}

JNIEXPORT jstring JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_build(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle) return env->NewStringUTF("AclMatcher closed");
    auto matcher = reinterpret_cast<AclMatcher *>(handle);
    if (matcher->bypassDomains || matcher->proxyDomains) return env->NewStringUTF("already built");
    matcher->bypassDomains = new RE2(matcher->bypassDomainsBuilder.str());
    if (!matcher->bypassDomains->ok()) return env->NewStringUTF(matcher->bypassDomains->error().c_str());
    matcher->proxyDomains = new RE2(matcher->proxyDomainsBuilder.str());
    if (!matcher->proxyDomains->ok()) return env->NewStringUTF(matcher->proxyDomains->error().c_str());
    return nullptr;
}

JNIEXPORT jint JNICALL
Java_com_github_shadowsocks_acl_AclMatcher_matchHost(JNIEnv *env, jclass clazz, jlong handle,
                                                     jstring host) {
    if (!handle) return -1;
    auto matcher = reinterpret_cast<const AclMatcher *>(handle);
    const char *hostChars = env->GetStringUTFChars(host, nullptr);
    jint result = 0;
    if (RE2::FullMatch(hostChars, *matcher->bypassDomains)) result = 1;
    else if (RE2::FullMatch(hostChars, *matcher->proxyDomains)) result = 2;
    env->ReleaseStringUTFChars(host, hostChars);
    return result;
}
}
