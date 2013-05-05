/**
 * @file NCDSugar.c
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
 */

#include <stdlib.h>

#include <misc/debug.h>

#include "NCDSugar.h"

struct desugar_state {
    NCDProgram *prog;
    size_t template_name_ctr;
};

static int add_template (struct desugar_state *state, NCDBlock block, NCDValue *out_name_val);
static int desugar_block (struct desugar_state *state, NCDBlock *block);
static int desugar_if (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next);
static int desugar_foreach (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next);

static int add_template (struct desugar_state *state, NCDBlock block, NCDValue *out_name_val)
{
    char name[40];
    snprintf(name, sizeof(name), "__tmpl%zu", state->template_name_ctr);
    state->template_name_ctr++;
    
    if (!desugar_block(state, &block)) {
        NCDBlock_Free(&block);
        return 0;
    }
    
    NCDProcess proc_tmp;
    if (!NCDProcess_Init(&proc_tmp, 1, name, block)) {
        NCDBlock_Free(&block);
        return 0;
    }
    
    NCDProgramElem elem;
    NCDProgramElem_InitProcess(&elem, proc_tmp);
    
    if (!NCDProgram_PrependElem(state->prog, elem)) {
        NCDProgramElem_Free(&elem);
        return 0;
    }
    
    if (!NCDValue_InitString(out_name_val, name)) {
        return 0;
    }
    
    return 1;
}

static int desugar_block (struct desugar_state *state, NCDBlock *block)
{
    NCDStatement *stmt = NCDBlock_FirstStatement(block);
    
    while (stmt) {
        switch (NCDStatement_Type(stmt)) {
            case NCDSTATEMENT_REG: {
                stmt = NCDBlock_NextStatement(block, stmt);
            } break;
            
            case NCDSTATEMENT_IF: {
                if (!desugar_if(state, block, stmt, &stmt)) {
                    return 0;
                }
            } break;
            
            case NCDSTATEMENT_FOREACH: {
                if (!desugar_foreach(state, block, stmt, &stmt)) {
                    return 0;
                }
            } break;
            
            default: ASSERT(0);
        }
    }
    
    return 1;
}

static int desugar_if (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next)
{
    ASSERT(NCDStatement_Type(stmt) == NCDSTATEMENT_IF)
    
    NCDValue args;
    NCDValue_InitList(&args);
    
    NCDIfBlock *ifblock = NCDStatement_IfBlock(stmt);
    
    while (NCDIfBlock_FirstIf(ifblock)) {
        NCDIf ifc = NCDIfBlock_GrabIf(ifblock, NCDIfBlock_FirstIf(ifblock));
        
        NCDValue if_cond;
        NCDBlock if_block;
        NCDIf_FreeGrab(&ifc, &if_cond, &if_block);
        
        if (!NCDValue_ListAppend(&args, if_cond)) {
            NCDValue_Free(&if_cond);
            NCDBlock_Free(&if_block);
            goto fail;
        }
        
        NCDValue action_arg;
        if (!add_template(state, if_block, &action_arg)) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&args, action_arg)) {
            NCDValue_Free(&action_arg);
            goto fail;
        }
    }
    
    if (NCDStatement_IfElse(stmt)) {
        NCDBlock else_block = NCDStatement_IfGrabElse(stmt);
        
        NCDValue action_arg;
        if (!add_template(state, else_block, &action_arg)) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&args, action_arg)) {
            NCDValue_Free(&action_arg);
            goto fail;
        }
    }
    
    NCDStatement new_stmt;
    if (!NCDStatement_InitReg(&new_stmt, NCDStatement_Name(stmt), NULL, "embcall2_multif", args)) {
        goto fail;
    }
    
    stmt = NCDBlock_ReplaceStatement(block, stmt, new_stmt);
    
    *out_next = NCDBlock_NextStatement(block, stmt);
    return 1;
    
fail:
    NCDValue_Free(&args);
    return 0;
}

static int desugar_foreach (struct desugar_state *state, NCDBlock *block, NCDStatement *stmt, NCDStatement **out_next)
{
    ASSERT(NCDStatement_Type(stmt) == NCDSTATEMENT_FOREACH)
    
    NCDValue args;
    NCDValue_InitList(&args);
    
    NCDValue collection;
    NCDBlock foreach_block;
    NCDStatement_ForeachGrab(stmt, &collection, &foreach_block);
    
    NCDValue template_arg;
    if (!add_template(state, foreach_block, &template_arg)) {
        NCDValue_Free(&collection);
        goto fail;
    }
    
    if (!NCDValue_ListAppend(&args, collection)) {
        NCDValue_Free(&template_arg);
        NCDValue_Free(&collection);
        goto fail;
    }
    
    if (!NCDValue_ListAppend(&args, template_arg)) {
        NCDValue_Free(&template_arg);
        goto fail;
    }
    
    NCDValue name1_arg;
    if (!NCDValue_InitString(&name1_arg, NCDStatement_ForeachName1(stmt))) {
        goto fail;
    }
    
    if (!NCDValue_ListAppend(&args, name1_arg)) {
        NCDValue_Free(&name1_arg);
        goto fail;
    }
    
    if (NCDStatement_ForeachName2(stmt)) {
        NCDValue name2_arg;
        if (!NCDValue_InitString(&name2_arg, NCDStatement_ForeachName2(stmt))) {
            goto fail;
        }
        
        if (!NCDValue_ListAppend(&args, name2_arg)) {
            NCDValue_Free(&name2_arg);
            goto fail;
        }
    }
    
    NCDStatement new_stmt;
    if (!NCDStatement_InitReg(&new_stmt, NCDStatement_Name(stmt), NULL, "foreach_emb", args)) {
        goto fail;
    }
    
    stmt = NCDBlock_ReplaceStatement(block, stmt, new_stmt);
    
    *out_next = NCDBlock_NextStatement(block, stmt);
    return 1;
    
fail:
    NCDValue_Free(&args);
    return 0;
}

int NCDSugar_Desugar (NCDProgram *prog)
{
    ASSERT(!NCDProgram_ContainsElemType(prog, NCDPROGRAMELEM_INCLUDE))
    ASSERT(!NCDProgram_ContainsElemType(prog, NCDPROGRAMELEM_INCLUDE_GUARD))
    
    struct desugar_state state;
    state.prog = prog;
    state.template_name_ctr = 0;
    
    for (NCDProgramElem *elem = NCDProgram_FirstElem(prog); elem; elem = NCDProgram_NextElem(prog, elem)) {
        ASSERT(NCDProgramElem_Type(elem) == NCDPROGRAMELEM_PROCESS)
        NCDProcess *proc = NCDProgramElem_Process(elem);
        
        if (!desugar_block(&state, NCDProcess_Block(proc))) {
            return 0;
        }
    }
    
    return 1;
}
