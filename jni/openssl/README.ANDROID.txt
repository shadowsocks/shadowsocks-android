Version Information
---

The code in this directory is based on 0.9.8h with patches from
http://openssl.org/news/secadv_20090107.txt, and some backported OpenSSL code
in crypto/0.9.9-dev.


Porting New Versions of OpenSSL
---

The following steps are recommended for porting new OpenSSL versions.


a) Run "./Configure linux-generic32 no-idea no-bf no-cast no-seed no-md2      \
       -DL_ENDIAN"
   in the openssl distribution directory.

   (Ignore when the Configure scripts asks you to run "make depend".)


b) Create an updated android-config.mk file by looking at CFLAG and DEPFLAG in
   the Makefile resulting from step a.  Make sure to add all the -D flags to
   LOCAL_CFLAGS, except -DTERMIO; include -DOPENSSL_NO_HW in addition to these.

   (New OpenSSL releases may include additional code meant to be disabled by
   default, so it's important not to miss any -DOPENSSL_NO_foo.
   Usually these should be replicated in crypto/opensslconf.h, but let's not
   take a chance.)


c) Copy the new LICENSE file from OpenSSL distribution as NOTICE.
   Create an empty MODULE_LICENSE_BSD_LIKE file.


d) You may delete the following directories along with their contents,
   since we won't use these (any more):

     MacOS Netware VMS apps/demoCA apps/set bugs certs crypto/bf crypto/cast  \
     crypto/cms crypto/idea crypto/md2 crypto/rc5 crypto/seed demos doc \
     engines ms os2 perl shlib test times tools util

   Also you may delete the following files:

     CHANGES CHANGES.SSLeay ChangeLog.0_9_7-stable_not-in-head                \
     ChangeLog.0_9_7-stable_not-in-head_FIPS Configure FAQ INSTALL            \
     INSTALL.DJGPP INSTALL.MacOS INSTALL.NW INSTALL.OS2 INSTALL.VMS           \
     INSTALL.W32 INSTALL.W64 INSTALL.WCE LICENSE Makefile Makefile.bak        \
     Makefile.org Makefile.shared NEWS PROBLEMS README README.ASN1            \
     README.ENGINE apps/CA.pl.bak config crypto/opensslconf.h.bak             \
     install.com makevms.com openssl.doxy openssl.spec


e) Go to include/openssl.  There's a bunch of symlinks here.  Since symlinks
   can be a special case for version control, replace them by regular files:

      for l in *.h; do cp $l copy_$l; rm $l; mv copy_$l $l; done

   Some symlinks will remain, pointing to files that don't exit
   (you deleted those in step d).  Delete the symlinks.


f) Create Android.mk files based on those you find in the previous OpenSSL port:

      Android.mk

      apps/Android.mk
      crypto/Android.mk
      ssl/Android.mk

   For the latter three, merge in any substantial changes between the
   corresponding Makefiles in the OpenSSL distribution (apps/Makefile,
   crypto/Makefile, crypto/*/Makefile, ssl/Makefile).
   Don't forget to update the directory name for OpenSSL in these files
   and whereever else it is used.


g) Apply the patch found at the end of this file.


h) Finally, create an updated version of this file (README.android)!




Patch for apps directory:

--- openssl-0.9.8h-ORIG/apps/progs.h
+++ openssl-0.9.8h/apps/progs.h
@@ -22,7 +22,9 @@
 extern int x509_main(int argc,char *argv[]);
 extern int genrsa_main(int argc,char *argv[]);
 extern int gendsa_main(int argc,char *argv[]);
+#if 0 /* ANDROID */
 extern int s_server_main(int argc,char *argv[]);
+#endif
 extern int s_client_main(int argc,char *argv[]);
 extern int speed_main(int argc,char *argv[]);
 extern int s_time_main(int argc,char *argv[]);
@@ -97,7 +99,9 @@
 	{FUNC_TYPE_GENERAL,"gendsa",gendsa_main},
 #endif
 #if !defined(OPENSSL_NO_SOCK) && !(defined(OPENSSL_NO_SSL2) && defined(OPENSSL_NO_SSL3))
-	{FUNC_TYPE_GENERAL,"s_server",s_server_main},
+#if 0 /* ANDROID */
+        {FUNC_TYPE_GENERAL,"s_server",s_server_main},
+#endif
 #endif
 #if !defined(OPENSSL_NO_SOCK) && !(defined(OPENSSL_NO_SSL2) && defined(OPENSSL_NO_SSL3))
 	{FUNC_TYPE_GENERAL,"s_client",s_client_main},
diff -ur openssl-0.9.8h-ORIG/apps/speed.c openssl-0.9.8h/apps/speed.c
--- openssl-0.9.8h-ORIG/apps/speed.c
+++ openssl-0.9.8h/apps/speed.c
@@ -108,12 +108,12 @@
 #include <signal.h>
 #endif
 
-#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(OPENSSL_SYS_MACOSX)
+#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(OPENSSL_SYS_MACOSX) || defined(HAVE_ANDROID_OS)
 # define USE_TOD
 #elif !defined(OPENSSL_SYS_MSDOS) && !defined(OPENSSL_SYS_VXWORKS) && (!defined(OPENSSL_SYS_VMS) || defined(__DECC))
 # define TIMES
 #endif
-#if !defined(_UNICOS) && !defined(__OpenBSD__) && !defined(sgi) && !defined(__FreeBSD__) && !(defined(__bsdi) || defined(__bsdi__)) && !defined(_AIX) && !defined(OPENSSL_SYS_MPE) && !defined(__NetBSD__) && !defined(OPENSSL_SYS_VXWORKS) /* FIXME */
+#if !defined(_UNICOS) && !defined(__OpenBSD__) && !defined(sgi) && !defined(__FreeBSD__) && !(defined(__bsdi) || defined(__bsdi__)) && !defined(_AIX) && !defined(OPENSSL_SYS_MPE) && !defined(__NetBSD__) && !defined(OPENSSL_SYS_VXWORKS) && !defined(HAVE_ANDROID_OS) /* FIXME */
 # define TIMEB
 #endif
 
@@ -1836,6 +1836,7 @@
 			}
 		}
 
+#if 0 /* ANDROID */
 	if (doit[D_IGE_128_AES])
 		{
 		for (j=0; j<SIZE_NUM; j++)
@@ -1879,6 +1880,7 @@
 			}
 		}
 #endif
+#endif
 #ifndef OPENSSL_NO_CAMELLIA
 	if (doit[D_CBC_128_CML])
 		{