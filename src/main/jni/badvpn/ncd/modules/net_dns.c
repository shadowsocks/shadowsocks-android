/**
 * @file net_dns.c
 * @author Ambroz Bizjak <ambrop7@gmail.com>
 * 
 * @section LICENSE
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @section DESCRIPTION
 * 
 * DNS servers module.
 * 
 * Synopsis: net.dns(list(string) servers, string priority)
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <misc/offset.h>
#include <misc/bsort.h>
#include <misc/balloc.h>
#include <misc/compare.h>
#include <structure/LinkedList1.h>
#include <ncd/NCDModule.h>
#include <ncd/extra/NCDIfConfig.h>
#include <ncd/extra/value_utils.h>

#include <generated/blog_channel_ncd_net_dns.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)
#define ModuleGlobal(i) ((i)->m->group->group_state)

struct instance {
    NCDModuleInst *i;
    LinkedList1 ipv4_dns_servers;
    LinkedList1Node instances_node; // node in instances
};

struct ipv4_dns_entry {
    LinkedList1Node list_node; // node in instance.ipv4_dns_servers
    uint32_t addr;
    int priority;
};

struct global {
    LinkedList1 instances;
};

static struct ipv4_dns_entry * add_ipv4_dns_entry (struct instance *o, uint32_t addr, int priority)
{
    // allocate entry
    struct ipv4_dns_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    
    // set info
    entry->addr = addr;
    entry->priority = priority;
    
    // add to list
    LinkedList1_Append(&o->ipv4_dns_servers, &entry->list_node);
    
    return entry;
}

static void remove_ipv4_dns_entry (struct instance *o, struct ipv4_dns_entry *entry)
{
    // remove from list
    LinkedList1_Remove(&o->ipv4_dns_servers, &entry->list_node);
    
    // free entry
    free(entry);
}

static void remove_ipv4_dns_entries (struct instance *o)
{
    LinkedList1Node *n;
    while (n = LinkedList1_GetFirst(&o->ipv4_dns_servers)) {
        struct ipv4_dns_entry *e = UPPER_OBJECT(n, struct ipv4_dns_entry, list_node);
        remove_ipv4_dns_entry(o, e);
    }
}

static size_t num_servers (struct global *g)
{
    size_t c = 0;
    
    for (LinkedList1Node *n = LinkedList1_GetFirst(&g->instances); n; n = LinkedList1Node_Next(n)) {
        struct instance *o = UPPER_OBJECT(n, struct instance, instances_node);
        for (LinkedList1Node *en = LinkedList1_GetFirst(&o->ipv4_dns_servers); en; en = LinkedList1Node_Next(en)) {
            c++;
        }
    }
    
    return c;
}

struct dns_sort_entry {
    uint32_t addr;
    int priority;
};

static int dns_sort_comparator (const void *v1, const void *v2)
{
    const struct dns_sort_entry *e1 = v1;
    const struct dns_sort_entry *e2 = v2;
    return B_COMPARE(e1->priority, e2->priority);
}

static int set_servers (struct global *g)
{
    int ret = 0;
    
    // count servers
    size_t num_ipv4_dns_servers = num_servers(g);
    
    // allocate sort array
    struct dns_sort_entry *servers = BAllocArray(num_ipv4_dns_servers, sizeof(servers[0]));
    if (!servers) {
        goto fail0;
    }
    size_t num_servers = 0;
    
    // fill sort array
    for (LinkedList1Node *n = LinkedList1_GetFirst(&g->instances); n; n = LinkedList1Node_Next(n)) {
        struct instance *o = UPPER_OBJECT(n, struct instance, instances_node);
        for (LinkedList1Node *en = LinkedList1_GetFirst(&o->ipv4_dns_servers); en; en = LinkedList1Node_Next(en)) {
            struct ipv4_dns_entry *e = UPPER_OBJECT(en, struct ipv4_dns_entry, list_node);
            servers[num_servers].addr = e->addr;
            servers[num_servers].priority= e->priority;
            num_servers++;
        }
    }
    ASSERT(num_servers == num_ipv4_dns_servers)
    
    // sort by priority
    // use a custom insertion sort instead of qsort() because we want a stable sort
    struct dns_sort_entry sort_temp;
    BInsertionSort(servers, num_servers, sizeof(servers[0]), dns_sort_comparator, &sort_temp);
    
    // copy addresses into an array
    uint32_t *addrs = BAllocArray(num_servers, sizeof(addrs[0]));
    if (!addrs) {
        goto fail1;
    }
    for (size_t i = 0; i < num_servers; i++) {
        addrs[i] = servers[i].addr;
    }
    
    // set servers
    if (!NCDIfConfig_set_dns_servers(addrs, num_servers)) {
        goto fail2;
    }
    
    ret = 1;
    
fail2:
    BFree(addrs);
fail1:
    BFree(servers);
fail0:
    return ret;
}

static int func_globalinit (struct NCDInterpModuleGroup *group, const struct NCDModuleInst_iparams *params)
{
    // allocate global state structure
    struct global *g = BAlloc(sizeof(*g));
    if (!g) {
        BLog(BLOG_ERROR, "BAlloc failed");
        return 0;
    }
    
    // set group state pointer
    group->group_state = g;
    
    // init instances list
    LinkedList1_Init(&g->instances);
    
    return 1;
}

static void func_globalfree (struct NCDInterpModuleGroup *group)
{
    struct global *g = group->group_state;
    ASSERT(LinkedList1_IsEmpty(&g->instances))
    
    // free global state structure
    BFree(g);
}

static void func_new (void *vo, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    struct global *g = ModuleGlobal(i);
    struct instance *o = vo;
    o->i = i;
    
    // init servers list
    LinkedList1_Init(&o->ipv4_dns_servers);
    
    // get arguments
    NCDValRef servers_arg;
    NCDValRef priority_arg;
    if (!NCDVal_ListRead(params->args, 2, &servers_arg, &priority_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong arity");
        goto fail1;
    }
    if (!NCDVal_IsList(servers_arg) || !NCDVal_IsString(priority_arg)) {
        ModuleLog(o->i, BLOG_ERROR, "wrong type");
        goto fail1;
    }
    
    uintmax_t priority;
    if (!ncd_read_uintmax(priority_arg, &priority) || priority > INT_MAX) {
        ModuleLog(o->i, BLOG_ERROR, "wrong priority");
        goto fail1;
    }
    
    // read servers
    size_t count = NCDVal_ListCount(servers_arg);
    for (size_t j = 0; j < count; j++) {
        NCDValRef server_arg = NCDVal_ListGet(servers_arg, j);
        
        if (!NCDVal_IsString(server_arg)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong type");
            goto fail1;
        }
        
        uint32_t addr;
        if (!ipaddr_parse_ipv4_addr_bin((char *)NCDVal_StringData(server_arg), NCDVal_StringLength(server_arg), &addr)) {
            ModuleLog(o->i, BLOG_ERROR, "wrong addr");
            goto fail1;
        }
        
        if (!add_ipv4_dns_entry(o, addr, priority)) {
            ModuleLog(o->i, BLOG_ERROR, "failed to add dns entry");
            goto fail1;
        }
    }
    
    // add to instances
    LinkedList1_Append(&g->instances, &o->instances_node);
    
    // set servers
    if (!set_servers(g)) {
        ModuleLog(o->i, BLOG_ERROR, "failed to set DNS servers");
        goto fail2;
    }
    
    // signal up
    NCDModuleInst_Backend_Up(o->i);
    return;
    
fail2:
    LinkedList1_Remove(&g->instances, &o->instances_node);
fail1:
    remove_ipv4_dns_entries(o);
    NCDModuleInst_Backend_DeadError(i);
}

static void func_die (void *vo)
{
    struct instance *o = vo;
    struct global *g = ModuleGlobal(o->i);
    
    // remove from instances
    LinkedList1_Remove(&g->instances, &o->instances_node);
    
    // set servers
    set_servers(g);
    
    // free servers
    remove_ipv4_dns_entries(o);
    
    NCDModuleInst_Backend_Dead(o->i);
}

static struct NCDModule modules[] = {
    {
        .type = "net.dns",
        .func_new2 = func_new,
        .func_die = func_die,
        .alloc_size = sizeof(struct instance)
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_net_dns = {
    .func_globalinit = func_globalinit,
    .func_globalfree = func_globalfree,
    .modules = modules
};
