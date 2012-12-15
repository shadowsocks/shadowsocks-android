#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <openssl/bn.h>

#define SPARCV9_TICK_PRIVILEGED	(1<<0)
#define SPARCV9_PREFER_FPU	(1<<1)
#define SPARCV9_VIS1		(1<<2)
#define SPARCV9_VIS2		(1<<3)	/* reserved */
#define SPARCV9_FMADD		(1<<4)	/* reserved for SPARC64 V */
static int OPENSSL_sparcv9cap_P=SPARCV9_TICK_PRIVILEGED;

int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np,const BN_ULONG *n0, int num)
	{
	int bn_mul_mont_fpu(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np,const BN_ULONG *n0, int num);
	int bn_mul_mont_int(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np,const BN_ULONG *n0, int num);

	if ((OPENSSL_sparcv9cap_P&(SPARCV9_PREFER_FPU|SPARCV9_VIS1)) ==
		(SPARCV9_PREFER_FPU|SPARCV9_VIS1))
		return bn_mul_mont_fpu(rp,ap,bp,np,n0,num);
	else
		return bn_mul_mont_int(rp,ap,bp,np,n0,num);
	}

unsigned long OPENSSL_rdtsc(void)
	{
	unsigned long _sparcv9_rdtick(void);

	if (OPENSSL_sparcv9cap_P&SPARCV9_TICK_PRIVILEGED)
#if defined(__sun) && defined(__SVR4)
		return gethrtime();
#else
		return 0;
#endif
	else
		return _sparcv9_rdtick();
	}

#if defined(__sun) && defined(__SVR4)

#include <dlfcn.h>
#include <libdevinfo.h>
#include <sys/systeminfo.h>

typedef di_node_t (*di_init_t)(const char *,uint_t);
typedef void      (*di_fini_t)(di_node_t);
typedef char *    (*di_node_name_t)(di_node_t);
typedef int       (*di_walk_node_t)(di_node_t,uint_t,di_node_name_t,int (*)(di_node_t,di_node_name_t));

#define DLLINK(h,name) (name=(name##_t)dlsym((h),#name))

static int walk_nodename(di_node_t node, di_node_name_t di_node_name)
	{
	char *name = (*di_node_name)(node);

	/* This is expected to catch all UltraSPARC flavors prior T1 */
	if (!strcmp (name,"SUNW,UltraSPARC") ||
	    !strncmp(name,"SUNW,UltraSPARC-I",17))  /* covers II,III,IV */
		{
		OPENSSL_sparcv9cap_P |= SPARCV9_PREFER_FPU|SPARCV9_VIS1;

		/* %tick is privileged only on UltraSPARC-I/II, but not IIe */
		if (name[14]!='\0' && name[17]!='\0' && name[18]!='\0')
			OPENSSL_sparcv9cap_P &= ~SPARCV9_TICK_PRIVILEGED;

		return DI_WALK_TERMINATE;
		}
	/* This is expected to catch remaining UltraSPARCs, such as T1 */
	else if (!strncmp(name,"SUNW,UltraSPARC",15))
		{
		OPENSSL_sparcv9cap_P &= ~SPARCV9_TICK_PRIVILEGED;

		return DI_WALK_TERMINATE;
		}

	return DI_WALK_CONTINUE;
	}

void OPENSSL_cpuid_setup(void)
	{
	void *h;
	char *e,si[256];
	static int trigger=0;

	if (trigger) return;
	trigger=1;

	if ((e=getenv("OPENSSL_sparcv9cap")))
		{
		OPENSSL_sparcv9cap_P=strtoul(e,NULL,0);
		return;
		}

	if (sysinfo(SI_MACHINE,si,sizeof(si))>0)
		{
		if (strcmp(si,"sun4v"))
			/* FPU is preferred for all CPUs, but US-T1/2 */
			OPENSSL_sparcv9cap_P |= SPARCV9_PREFER_FPU;
		}

	if (sysinfo(SI_ISALIST,si,sizeof(si))>0)
		{
		if (strstr(si,"+vis"))
			OPENSSL_sparcv9cap_P |= SPARCV9_VIS1;
		if (strstr(si,"+vis2"))
			{
			OPENSSL_sparcv9cap_P |= SPARCV9_VIS2;
			OPENSSL_sparcv9cap_P &= ~SPARCV9_TICK_PRIVILEGED;
			return;
			}
		}

	if ((h = dlopen("libdevinfo.so.1",RTLD_LAZY))) do
		{
		di_init_t	di_init;
		di_fini_t	di_fini;
		di_walk_node_t	di_walk_node;
		di_node_name_t	di_node_name;
		di_node_t	root_node;

		if (!DLLINK(h,di_init))		break;
		if (!DLLINK(h,di_fini))		break;
		if (!DLLINK(h,di_walk_node))	break;
		if (!DLLINK(h,di_node_name))	break;

		if ((root_node = (*di_init)("/",DINFOSUBTREE))!=DI_NODE_NIL)
			{
			(*di_walk_node)(root_node,DI_WALK_SIBFIRST,
					di_node_name,walk_nodename);
			(*di_fini)(root_node);
			}
		} while(0);

	if (h) dlclose(h);
	}

#else

void OPENSSL_cpuid_setup(void)
	{
	char *e;
 
	if ((e=getenv("OPENSSL_sparcv9cap")))
		{
		OPENSSL_sparcv9cap_P=strtoul(e,NULL,0);
		return;
		}

	/* For now we assume that the rest supports UltraSPARC-I* only */
	OPENSSL_sparcv9cap_P |= SPARCV9_PREFER_FPU|SPARCV9_VIS1;
	}

#endif
