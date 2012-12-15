/* status.c - Allow control of a running server using a socket

   Copyright (C) 2000, 2001 Thomas Moestl
   Copyright (C) 2002, 2003, 2004, 2005, 2007, 2008, 2009, 2011 Paul A. Rombouts

  This file is part of the pdnsd package.

  pdnsd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  pdnsd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pdnsd; see the file COPYING. If not, see
  <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>	/* for offsetof */
#include "ipvers.h"
#include "status.h"
#include "thread.h"
#include "cache.h"
#include "error.h"
#include "servers.h"
#include "dns_answer.h"
#include "helpers.h"
#include "conf-parser.h"

#if !defined(HAVE_ALLOCA) && !defined(alloca)
#define alloca malloc
#endif


char *sock_path=NULL;
int stat_sock;


/* Print an error to the socket */
static int print_serr(int rs, const char *msg)
{
	uint16_t cmd;

	DEBUG_MSG("Sending error message to control socket: '%s'\n",msg);
	cmd=htons(1);
	if(write(rs,&cmd,sizeof(cmd))!=sizeof(cmd) ||
	   write_all(rs,msg,strlen(msg))<0)
	{
		DEBUG_MSG("Error writing to control socket: %s\n",strerror(errno));
		return 0;
	}
	return 1;
}

/* Print a success code to the socket */
static int print_succ(int rs)
{
	uint16_t cmd;

	cmd=htons(0);
	if(write(rs,&cmd,sizeof(cmd))!=sizeof(cmd)) {
		DEBUG_MSG("Error writing to control socket: %s\n"
			  "Failed to send success code.\n",strerror(errno));
		return 0;
	}
	return 1;
}

/* Read a cmd short */
static int read_short(int fh, uint16_t *res)
{
	uint16_t cmd;

	if (read(fh,&cmd,sizeof(cmd))!=sizeof(cmd)) {
		/* print_serr(fh,"Bad arg."); */
		return 0;
	}
	*res= ntohs(cmd);
	return 1;
}

/* Read a cmd long */
static int read_long(int fh, uint32_t *res)
{
	uint32_t cmd;

	if (read(fh,&cmd,sizeof(cmd))!=sizeof(cmd)) {
		/* print_serr(fh,"Bad arg."); */
		return 0;
	}
	*res= ntohl(cmd);
	return 1;
}

/* Read a string preceded by a char count.
   A buffer of the right size is allocated to hold the result.
   A return value of 1 means success,
   -1 means the result is undefined (*res is set to NULL),
   0 means read or allocation error.
*/
static int read_allocstring(int fh, char **res, unsigned *len)
{
	uint16_t count;
	char *buf;
	unsigned int nread;

	if(!read_short(fh,&count)) return 0;
	if(count==(uint16_t)(~0)) {*res=NULL; return -1;}
	if(!(buf=malloc(count+1))) return 0;
	nread=0;
	while(nread<count) {
		ssize_t m=read(fh,buf+nread,count-nread);
		if(m<=0) {free(buf); return 0;}
		nread+=m;
	}
	buf[count]=0;
	*res=buf;
	if(len) *len=count;
	return 1;
}

/* Read a string preceded by a char count.
   Place it in a buffer of size buflen and terminate with a null char.
   A return value of 1 means success, -1 means not defined,
   0 means error (read error, buffer too small).
*/
static int read_domain(int fh, char *buf, unsigned int buflen)
{
	uint16_t count;
	unsigned int nread;

	if(!read_short(fh,&count)) return 0;
	if(count==(uint16_t)(~0)) return -1;
	if(count >=buflen) return 0;
	nread=0;
	while(nread<count) {
		ssize_t m=read(fh,buf+nread,count-nread);
		if(m<=0) return 0;
		nread+=m;
	}
	buf[count]=0;
#if 0
	if(count==0 || buf[count-1]!='.') {
		if(count+1>=buflen) return 0;
		buf[count]='.'; buf[count+1]=0;
	}
#endif
	return 1;
}

static void *status_thread (void *p)
{
	THREAD_SIGINIT;
	/* (void)p; */  /* To inhibit "unused variable" warning */

	if (!global.strict_suid) {
		if (!run_as(global.run_as)) {
			pdnsd_exit();
		}
	}

	if (listen(stat_sock,5)==-1) {
		log_warn("Error: could not listen on socket: %s.\nStatus readback will be impossible",strerror(errno));
		goto exit_thread;
	}
	for(;;) {
		struct sockaddr_un ra;
		socklen_t res=sizeof(ra);
		int rs;
		if ((rs=accept(stat_sock,(struct sockaddr *)&ra,&res))!=-1) {
			uint16_t cmd;
			DEBUG_MSG("Status socket query pending.\n");
			if (read_short(rs,&cmd)) {
			    /* Check magic number in command */
				if((cmd & 0xff00) == CTL_CMDVERNR) {
				const char *errmsg;
				cmd &= 0xff;
				switch(cmd) {
				case CTL_STATS: {
					struct utsname nm;
					DEBUG_MSG("Received STATUS query.\n");
					if(!print_succ(rs))
						break;
					uname(&nm);
					if(fsprintf(rs,"pdnsd-%s running on %s.\n",VERSION,nm.nodename)<0 ||
					   report_cache_stat(rs)<0 ||
					   report_thread_stat(rs)<0 ||
					   report_conf_stat(rs)<0)
					{
						DEBUG_MSG("Error writing to control socket: %s\n"
							  "Failed to send status report.\n",strerror(errno));
					}
				}
					break;
				case CTL_SERVER: {
					char *label,*dnsaddr;
					int indx;
					uint16_t cmd2;
					DEBUG_MSG("Received SERVER command.\n");
					if (read_allocstring(rs,&label,NULL)<=0) {
					    print_serr(rs,"Error reading server label.");
					    break;
					}
					if (!read_short(rs,&cmd2)) {
					    print_serr(rs,"Missing up|down|retest.");
					    goto free_label_break;
					}
					if(!read_allocstring(rs, &dnsaddr,NULL)) {
					    print_serr(rs,"Error reading DNS addresses.");
					    goto free_label_break;
					}
					/* Note by Paul Rombouts:
					   We are about to access server configuration data.
					   Now that the configuration can be changed during run time,
					   we should be using locks before accessing server config data, even if it
					   is read-only access.
					   However, as long as this is the only thread that calls reload_config_file()
					   it should be OK to read the server config without locks, but it is
					   something to keep in mind.
					*/
					{
					    char *endptr;
					    indx=strtol(label,&endptr,0);
					    if(!*endptr) {
						if (indx<0 || indx>=DA_NEL(servers)) {
						    print_serr(rs,"Server index out of range.");
						    goto free_dnsaddr_label_break;
						}
					    }
					    else {
						if (!strcmp(label, "all"))
						    indx=-2; /* all servers */
						else
						    indx=-1; /* compare names */
					    }
					}
					if(cmd2==CTL_S_UP || cmd2==CTL_S_DOWN || cmd2==CTL_S_RETEST) {
					    if(!dnsaddr) {
						if (indx==-1) {
						    int i;
						    for (i=0;i<DA_NEL(servers);++i) {
							char *servlabel=DA_INDEX(servers,i).label;
							if (servlabel && !strcmp(servlabel,label))
							    goto found_label;
						    }
						    print_serr(rs,"Bad server label.");
						    goto free_dnsaddr_label_break;
						found_label:;
						}
						if(mark_servers(indx,(indx==-1)?label:NULL,(cmd2==CTL_S_RETEST)?-1:(cmd2==CTL_S_UP))==0)
						    print_succ(rs);
						else
						    print_serr(rs,"Could not start up or signal server status thread.");
					    }
					    else { /* Change server addresses */
						if(indx==-2) {
						    print_serr(rs,"Can't use label \"all\" to change server addresses.");
						    goto free_dnsaddr_label_break;
						}
						if(indx==-1) {
						    int i;
						    for(i=0;i<DA_NEL(servers);++i) {
							char *servlabel=DA_INDEX(servers,i).label;
							if (servlabel && !strcmp(servlabel,label)) {
							    if(indx!=-1) {
								print_serr(rs,"server label must be unique to change server addresses.");
								goto free_dnsaddr_label_break;
							    }
							    indx=i;
							}
						    }
						    if(indx==-1) {
							print_serr(rs,"Bad server label.");
							goto free_dnsaddr_label_break;
						    }
						}
						{
						    char *ipstr,*q=dnsaddr;
						    addr_array ar=NULL;
						    pdnsd_a addr;
						    int err;
						    for(;;) {
							for(;;) {
							    if(!*q) goto change_servs;
							    if(*q!=',' && !isspace(*q)) break;
							    ++q;
							}
							ipstr=q;
							for(;;) {
							    ++q;
							    if(!*q) break;
							    if(*q==',' || isspace(*q)) {*q++=0; break; }
							}
							if(!str2pdnsd_a(ipstr,&addr)) {
							    print_serr(rs,"Bad server ip");
							    goto free_ar;
							}
							if(!(ar=DA_GROW1(ar))) {
							    print_serr(rs,"Out of memory.");
							    goto free_dnsaddr_label_break;
							}
							DA_LAST(ar)=addr;
						    }
						change_servs:
						    err=change_servers(indx,ar,(cmd2==CTL_S_RETEST)?-1:(cmd2==CTL_S_UP));
						    if(err==0)
							print_succ(rs);
						    else
							print_serr(rs,err==ETIMEDOUT?"Timed out while trying to gain access to server data.":
								      err==ENOMEM?"Out of memory.":
								      "Could not start up or signal server status thread.");
						free_ar:
						    da_free(ar);
						}
					    }
					}
					else
					    print_serr(rs,"Bad command.");

				free_dnsaddr_label_break:
					free(dnsaddr);
				free_label_break:
					free(label);
				}
					break;
				case CTL_RECORD: {
					uint16_t cmd2;
					unsigned char name[DNSNAMEBUFSIZE],buf[DNSNAMEBUFSIZE];
					DEBUG_MSG("Received RECORD command.\n");
					if (!read_short(rs,&cmd2))
						goto incomplete_command;
					if (read_domain(rs, charp buf, sizeof(buf))<=0)
						goto incomplete_command;
					if ((errmsg=parsestr2rhn(buf,sizeof(buf),name))!=NULL)
						goto bad_domain_name;
					switch (cmd2) {
					case CTL_R_DELETE:
						del_cache(name);
						print_succ(rs);
						break;
					case CTL_R_INVAL:
						invalidate_record(name);
						print_succ(rs);
						break;
					default:
						print_serr(rs,"Bad command.");
					}
				}
					break;
				case CTL_SOURCE: {
					uint32_t ttl;
					char *fn;
					uint16_t servaliases,flags;
					unsigned char buf[DNSNAMEBUFSIZE],owner[DNSNAMEBUFSIZE];

					DEBUG_MSG("Received SOURCE command.\n");
					if (read_allocstring(rs,&fn,NULL)<=0) {
						print_serr(rs,"Bad filename name.");
						break;
					}
					if (read_domain(rs, charp buf, sizeof(buf))<=0 ||
					    !read_long(rs,&ttl) ||
					    !read_short(rs,&servaliases) ||	/* serve aliases */
					    !read_short(rs,&flags))		/* caching flags */
					{
						print_serr(rs,"Malformed or incomplete command.");
						goto free_fn;
					}
					if ((errmsg=parsestr2rhn(buf,sizeof(buf),owner))!=NULL) {
						print_serr(rs,errmsg);
						goto free_fn;
					}
					if (ttl < 0) {
						print_serr(rs, "Bad TTL.");
						goto free_fn;
					}
					if(flags&DF_NEGATIVE) {
						print_serr(rs, "Bad cache flags.");
						goto free_fn;
					}
					{
						char *errmsg;
						if (read_hosts(fn,owner,ttl,flags,servaliases,&errmsg))
							print_succ(rs);
						else {
							print_serr(rs,errmsg?:"Out of memory.");
							free(errmsg);
						}
					}
				free_fn:
					free(fn);
				}
					break;
				case CTL_ADD: {
					uint32_t ttl;
					unsigned sz;
					uint16_t tp,flags,nadr=0;
					unsigned char name[DNSNAMEBUFSIZE],buf[DNSNAMEBUFSIZE],dbuf[2+DNSNAMEBUFSIZE];
					size_t adrbufsz=0;
					unsigned char *adrbuf=NULL;

					DEBUG_MSG("Received ADD command.\n");
					if (!read_short(rs,&tp))
						goto incomplete_command;
					if (read_domain(rs, charp buf, sizeof(buf))<=0)
						goto incomplete_command;
					if (!read_long(rs,&ttl))
						goto incomplete_command;
					if (!read_short(rs,&flags))	/* caching flags */
						goto incomplete_command;
					if ((errmsg=parsestr2rhn(buf,sizeof(buf),name))!=NULL)
						goto bad_domain_name;
					if (ttl < 0)
						goto bad_ttl;
					if(flags&DF_NEGATIVE)
						goto bad_flags;

					switch (tp) {
					case T_A:
						sz=sizeof(struct in_addr);
    #if ALLOW_LOCAL_AAAA
						goto read_adress_list;
					case T_AAAA:
						sz=sizeof(struct in6_addr);
					read_adress_list:
    #endif
						if (!read_short(rs,&nadr))
							goto incomplete_command;
						if (!nadr)
							goto bad_arg;
						adrbufsz= nadr * (size_t)sz;
						adrbuf= malloc(adrbufsz);
						if(!adrbuf)
							goto out_of_memory;
						{
							size_t nread=0;
							while(nread<adrbufsz) {
								ssize_t m=read(rs,adrbuf+nread,adrbufsz-nread);
								if(m<=0) {free(adrbuf); goto bad_arg;}
								nread += m;
							}
						}
						break;
					case T_CNAME:
					case T_PTR:
					case T_NS:
						if (read_domain(rs, charp buf, sizeof(buf))<=0)
							goto incomplete_command;
						if ((errmsg=parsestr2rhn(buf,sizeof(buf),dbuf))!=NULL)
							goto bad_domain_name;
						sz=rhnlen(dbuf);
						break;
					case T_MX:
						if (read(rs,dbuf,2)!=2)
							goto bad_arg;
						if (read_domain(rs, charp buf, sizeof(buf))<=0)
							goto incomplete_command;
						if ((errmsg=parsestr2rhn(buf,sizeof(buf),dbuf+2))!=NULL)
							goto bad_domain_name;
						sz=rhnlen(dbuf+2)+2;
						break;
					default:
						goto bad_arg;
					}
					{
						dns_cent_t cent;

						if (!init_cent(&cent, name, 0, 0, flags  DBG1)) {
							free(adrbuf);
							goto out_of_memory;
						}
						if(adrbuf) {
							unsigned char *adrp; int i;
							for(adrp=adrbuf,i=0; i<nadr; adrp += sz,++i) {
								if (!add_cent_rr(&cent,tp,ttl,0,CF_LOCAL,sz,adrp  DBG1)) {
									free_cent(&cent  DBG1);
									free(adrbuf);
									goto out_of_memory;
								}
							}
							free(adrbuf);
						}
						else if (!add_cent_rr(&cent,tp,ttl,0,CF_LOCAL,sz,dbuf  DBG1)) {
							free_cent(&cent  DBG1);
							goto out_of_memory;
						}

						if(cent.qname[0]==1 && cent.qname[1]=='*') {
							/* Wild card record.
							   Set the DF_WILD flag for the name with '*.' removed. */
							if(!set_cent_flags(&cent.qname[2],DF_WILD)) {
								print_serr(rs,
									   "Before defining records for a name with a wildcard"
									   " you must first define some records for the name"
									   " with '*.' removed.");
								goto cleanup_cent;
							}
						}

						add_cache(&cent);
						print_succ(rs);
					cleanup_cent:
						free_cent(&cent  DBG1);
					}
				}
					break;
				case CTL_NEG: {
					uint32_t ttl;
					uint16_t tp;
					unsigned char name[DNSNAMEBUFSIZE],buf[DNSNAMEBUFSIZE];

					DEBUG_MSG("Received NEG command.\n");
					if (read_domain(rs, charp buf, sizeof(buf))<=0)
						goto incomplete_command;
					if (!read_short(rs,&tp))
						goto incomplete_command;
					if (!read_long(rs,&ttl))
						goto incomplete_command;
					if ((errmsg=parsestr2rhn(buf,sizeof(buf),name))!=NULL) {
						DEBUG_MSG("NEG: received bad domain name.\n");
						goto bad_domain_name;
					}
					if (tp!=255 && PDNSD_NOT_CACHED_TYPE(tp)) {
						DEBUG_MSG("NEG: received bad record type.\n");
						print_serr(rs,"Bad record type.");
						break;
					}
					if (ttl < 0)
						goto bad_ttl;
					{
						dns_cent_t cent;

						if (tp==255) {
							if (!init_cent(&cent, name, ttl, 0, DF_LOCAL|DF_NEGATIVE  DBG1))
								goto out_of_memory;
						} else {
							if (!init_cent(&cent, name, 0, 0, 0  DBG1))
								goto out_of_memory;
							if (!add_cent_rrset_by_type(&cent,tp,ttl,0,CF_LOCAL|CF_NEGATIVE  DBG1)) {
								free_cent(&cent  DBG1);
								goto out_of_memory;
							}
						}
						add_cache(&cent);
						free_cent(&cent DBG1);
					}
					print_succ(rs);
				}
					break;
				case CTL_CONFIG: {
					char *fn,*errmsg;
					DEBUG_MSG("Received CONFIG command.\n");
					if (!read_allocstring(rs,&fn,NULL)) {
						print_serr(rs,"Bad filename name.");
						break;
					}
					if (reload_config_file(fn,&errmsg))
						print_succ(rs);
					else {
						print_serr(rs,errmsg?:"Out of memory.");
						free(errmsg);
					}
					free(fn);
				}
					break;
				case CTL_INCLUDE: {
					char *fn,*errmsg;
					DEBUG_MSG("Received INCLUDE command.\n");
					if (read_allocstring(rs,&fn,NULL)<=0) {
						print_serr(rs,"Bad filename name.");
						break;
					}
					if (read_config_file(fn,NULL,NULL,0,&errmsg))
						print_succ(rs);
					else {
						print_serr(rs,errmsg?:"Out of memory.");
						free(errmsg);
					}
					free(fn);
				}
					break;
				case CTL_EVAL: {
					char *str,*errmsg;
					DEBUG_MSG("Received EVAL command.\n");
					if (!read_allocstring(rs,&str,NULL)) {
						print_serr(rs,"Bad input string.");
						break;
					}
					if (confparse(NULL,str,NULL,NULL,0,&errmsg))
						print_succ(rs);
					else {
						print_serr(rs,errmsg?:"Out of memory.");
						free(errmsg);
					}
					free(str);
				}
					break;
				case CTL_EMPTY: {
					slist_array sla=NULL;
					char *names; unsigned len;

					DEBUG_MSG("Received EMPTY command.\n");
					if (!read_allocstring(rs,&names,&len)) {
						print_serr(rs,"Bad arguments.");
						break;
					}
					if(names) {
						char *p=names, *last=names+len;

						while(p<last) {
							int tp;
							char *q;
							slist_t *sl;
							unsigned sz;
							unsigned char rhn[DNSNAMEBUFSIZE];

							if(*p=='-') {
								tp=C_EXCLUDED;
								++p;
							}
							else {
								tp=C_INCLUDED;
								if(*p=='+') ++p;
							}
							/* skip a possible leading dot. */
							if(p+1<last && *p=='.' && *(p+1)) ++p;
							q=p;
							while(q<last && *q) ++q;
							if ((errmsg=parsestr2rhn(ucharp p,q-p,rhn))!=NULL) {
								DEBUG_MSG("EMPTY: received bad domain name: %s\n",p);
								print_serr(rs,errmsg);
								goto free_sla_names_break;
							}
							sz=rhnlen(rhn);
							if (!(sla=DA_GROW1_F(sla,free_slist_domain))) {
								print_serr(rs,"Out of memory.");
								goto free_names_break;
							}
							sl=&DA_LAST(sla);

							if (!(sl->domain=malloc(sz))) {
								print_serr(rs,"Out of memory.");
								goto free_sla_names_break;
							}
							memcpy(sl->domain,rhn,sz);
							sl->exact=0;
							sl->rule=tp;
							p = q+1;
						}
					}
					if(empty_cache(sla))
						print_succ(rs);
					else
						print_serr(rs,"Could not lock the cache.");
				free_sla_names_break:
					free_slist_array(sla);
				free_names_break:
					free(names);
				}
					break;
				case CTL_DUMP: {
					int rv,exact=0;
					unsigned char *nm=NULL;
					char buf[DNSNAMEBUFSIZE];
					unsigned char rhn[DNSNAMEBUFSIZE];
					DEBUG_MSG("Received DUMP command.\n");
					if (!(rv=read_domain(rs,buf,sizeof(buf)))) {
						print_serr(rs,"Bad domain name.");
						break;
					}
					if(rv>0) {
						int sz;
						exact=1; nm= ucharp buf; sz=sizeof(buf);
						if(buf[0]=='.' && buf[1]) {
							exact=0; ++nm; --sz;
						}
						if ((errmsg=parsestr2rhn(nm,sz,rhn))!=NULL)
							goto bad_domain_name;
						nm=rhn;
					}
					if(!print_succ(rs))
						break;
					if((rv=dump_cache(rs,nm,exact))<0 ||
					   (!rv && fsprintf(rs,"Could not find %s%s in the cache.\n",
							    exact?"":nm?"any entries matching ":"any entries",
							    nm?buf:"")<0))
					{
						DEBUG_MSG("Error writing to control socket: %s\n",strerror(errno));
					}
				}
					break;
				incomplete_command:
					print_serr(rs,"Malformed or incomplete command.");
					break;
				bad_arg:
					print_serr(rs,"Bad arg.");
					break;
				bad_domain_name:
					print_serr(rs,errmsg);
					break;
				bad_ttl:
					print_serr(rs, "Bad TTL.");
					break;
				bad_flags:
					print_serr(rs, "Bad cache flags.");
					break;
				out_of_memory:
					print_serr(rs,"Out of memory.");
					break;
				default:
					print_serr(rs,"Unknown command.");
				}
			    }
			    else {
				    DEBUG_MSG("Incorrect magic number in status-socket command code: %02x\n",cmd>>8);
				    print_serr(rs,"Command code contains incompatible version number.");
			    }
			}
			else {
				DEBUG_MSG("short status-socket query\n");
				print_serr(rs,"Command code missing or too short.");
			}
			close(rs);
			usleep_r(100000); /* sleep some time. I do not want the query frequency to be too high. */
		}
		else if (errno!=EINTR) {
			log_warn("Failed to accept connection on status socket: %s. "
				 "Status readback will be impossible",strerror(errno));
			break;
		}
	}

 exit_thread:
	stat_pipe=0;
	close(stat_sock);
	statsock_thrid=main_thrid;

	return NULL;
}

/*
 * Initialize the status socket
 */
void init_stat_sock()
{
	struct sockaddr_un *sa;
	/* Should I include the terminating null byte in the calculation of the length parameter
	   for the socket address? The glibc info page "Details of Local Namespace" tells me I should not,
	   yet it is immediately followed by an example that contradicts that.
	   The SUN_LEN macro seems to be defined as
	   (offsetof(struct sockaddr_un, sun_path) + strlen(sa->sun_path)),
	   so I conclude it is not necessary to count the null byte, but it probably makes no
	   difference if you do.
	*/
	unsigned int sa_len = (offsetof(struct sockaddr_un, sun_path) + strlitlen("/pdnsd.status") + strlen(global.cache_dir));

	sa=(struct sockaddr_un *)alloca(sa_len+1);
	stpcpy(stpcpy(sa->sun_path,global.cache_dir),"/pdnsd.status");

	if (unlink(sa->sun_path)!=0 && errno!=ENOENT) { /* Delete the socket */
		log_warn("Failed to unlink %s: %s.\nStatus readback will be disabled",sa->sun_path, strerror(errno));
		stat_pipe=0;
		return;
	}
	if ((stat_sock=socket(PF_UNIX,SOCK_STREAM,0))==-1) {
		log_warn("Failed to open socket: %s. Status readback will be impossible",strerror(errno));
		stat_pipe=0;
		return;
	}
	sa->sun_family=AF_UNIX;
#ifdef BSD44_SOCKA
	sa->sun_len=SUN_LEN(sa);
#endif
	/* Early initialization, so that umask can be used race-free. */
	{
		mode_t old_mask = umask((S_IRWXU|S_IRWXG|S_IRWXO)&(~global.ctl_perms));
		if (bind(stat_sock,(struct sockaddr *)sa,sa_len)==-1) {
			log_warn("Error: could not bind socket: %s.\nStatus readback will be impossible",strerror(errno));
			close(stat_sock);
			stat_pipe=0;
		}
		umask(old_mask);
	}

	if(stat_pipe) sock_path= strdup(sa->sun_path);
}

/*
 * Start the status socket thread (see above)
 */
int start_stat_sock()
{
	pthread_t st;

	int rv=pthread_create(&st,&attr_detached,status_thread,NULL);
	if (rv)
		log_warn("Failed to start status thread. The status socket will be unuseable");
	else {
		statsock_thrid=st;
		log_info(2,"Status thread started.");
	}
	return rv;
}
