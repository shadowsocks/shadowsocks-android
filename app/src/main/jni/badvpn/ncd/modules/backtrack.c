/**
 * @file backtrack.c
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
 * Synopsis:
 *   backtrack_point()
 *   backtrack_point::go()
 * 
 * Description:
 *   The backtrack_point() statement creates a backtrack point, going up immedietely.
 *   The go() method triggers backtracking to the backtrack point, i.e. makes the
 *   backtrack_point() statement go down and back up at atomically. The go() method
 *   itself goes up immedietely, but side effects of triggering backtracking have
 *   priority.
 */

#include <stddef.h>

#include <ncd/NCDModule.h>

#include <generated/blog_channel_ncd_backtrack.h>

#define ModuleLog(i, ...) NCDModuleInst_Backend_Log((i), BLOG_CURRENT_CHANNEL, __VA_ARGS__)

static void func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    if (!NCDVal_ListRead(params->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    // go up
    NCDModuleInst_Backend_Up(i);
    return;
    
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static void go_func_new (void *unused, NCDModuleInst *i, const struct NCDModuleInst_new_params *params)
{
    // check arguments
    if (!NCDVal_ListRead(params->args, 0)) {
        ModuleLog(i, BLOG_ERROR, "wrong arity");
        goto fail0;
    }
    
    // get backtrack point
    NCDModuleInst *backtrack_point_inst = params->method_user;
    
    // go up (after toggling)
    NCDModuleInst_Backend_Up(i);
    
    // toggle backtrack point
    NCDModuleInst_Backend_DownUp(backtrack_point_inst);
    return;
    
fail0:
    NCDModuleInst_Backend_DeadError(i);
}

static struct NCDModule modules[] = {
    {
        .type = "backtrack_point",
        .func_new2 = func_new
    }, {
        .type = "backtrack_point::go",
        .func_new2 = go_func_new
    }, {
        .type = NULL
    }
};

const struct NCDModuleGroup ncdmodule_backtrack = {
    .modules = modules
};
