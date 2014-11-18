/**
 * @file NCDObject.h
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

#ifndef BADVPN_NCDOBJECT_H
#define BADVPN_NCDOBJECT_H

#include <stddef.h>

#include <misc/debug.h>
#include <ncd/NCDVal.h>
#include <ncd/NCDStringIndex.h>
#include <ncd/static_strings.h>

/**
 * Represents an NCD object.
 * Objects expose the following functionalities:
 * - resolving variables by name,
 * - resolving objects by name,
 * - provide information for calling method-like statements.
 * 
 * The NCDObject structure must not be stored persistently; it is only
 * valid at the time it was obtained, and any change of state in the
 * execution of the NCD program may render the object invalid.
 * However, the structure does not contain any resources, and can freely
 * be passed around by value.
 */
typedef struct NCDObject_s NCDObject;

/**
 * Callback function for variable resolution requests.
 * 
 * @param obj const pointer to the NCDObject this is being called for.
 *            {@link NCDObject_DataPtr} and {@link NCDObject_DataInt} can be
 *            used to retrieve state information which was passed to
 *            {@link NCDObject_Build} or {@link NCDObject_BuildFull}.
 * @param name name of the variable being resolved, in form of an {@link NCDStringIndex}
 *             string identifier
 * @param mem pointer to the memory object where the resulting value should be
 *            constructed
 * @param out_value If the variable exists, *out_value should be set to the value
 *                  reference to the result value. If the variable exists but there
 *                  was an error constructing the value, should be set to an
 *                  invalid value reference. Can be modified even if the variable
 *                  does not exist.
 * @return 1 if the variable exists, 0 if not
 */
typedef int (*NCDObject_func_getvar) (const NCDObject *obj, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value);

/**
 * Callback function for object resolution requests.
 * 
 * @param obj const pointer to the NCDObject this is being called for.
 *            {@link NCDObject_DataPtr} and {@link NCDObject_DataInt} can be
 *            used to retrieve state information which was passed to
 *            {@link NCDObject_Build} or {@link NCDObject_BuildFull}.
 * @param name name of the object being resolved, in form of an {@link NCDStringIndex}
 *             string identifier
 * @param out_object If the object exists, *out_object should be set to the result
 *                   object. Can be modified even if the object does not exist.
 * @return 1 if the object exists, 0 if not
 */
typedef int (*NCDObject_func_getobj) (const NCDObject *obj, NCD_string_id_t name, NCDObject *out_object);

struct NCDObject_s {
    NCD_string_id_t type;
    int data_int;
    void *data_ptr;
    void *method_user;
    NCDObject_func_getvar func_getvar;
    NCDObject_func_getobj func_getobj;
};

/**
 * Basic object construction function.
 * This is equivalent to calling {@link NCDObject_BuildFull} with data_int=0
 * and method_user=data_ptr. See that function for detailed documentation.
 */
static NCDObject NCDObject_Build (NCD_string_id_t type, void *data_ptr, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj);

/**
 * Constructs an {@link NCDObject} structure.
 * This is the full version where all supported parameters have to be provided.
 * In most cases, {@link NCDObject_Build} will suffice.
 * 
 * @param type type of the object for the purpose of calling method-like statements
 *             on the object, in form of an {@link NCDStringIndex} string identifier.
 *             May be set to -1 if the object has no methods.
 * @param data_ptr state-keeping pointer which can be restored from callbacks using
 *                 {@link NCDObject_DataPtr}
 * @param data_int state-keeping integer which can be restored from callbacks using
 *                 {@link NCDObject_DataInt}
 * @param method_user state-keeping pointer to be passed to new method-like statements
 *                    created using this object. The value of this argument will be
 *                    available as params->method_user within the {@link NCDModule_func_new2}
 *                    module backend callback.
 * @param func_getvar callback for resolving variables within the object. This must not
 *                    be NULL; if the object exposes no variables, pass {@link NCDObject_no_getvar}.
 * @param func_getobj callback for resolving objects within the object. This must not
 *                    be NULL; if the object exposes no objects, pass {@link NCDObject_no_getobj}.
 * @return an NCDObject structure encapsulating the information given
 */
static NCDObject NCDObject_BuildFull (NCD_string_id_t type, void *data_ptr, int data_int, void *method_user, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj);

/**
 * Returns the 'type' attribute; see {@link NCDObject_BuildFull}.
 */
static NCD_string_id_t NCDObject_Type (const NCDObject *o);

/**
 * Returns the 'data_ptr' attribute; see {@link NCDObject_BuildFull}.
 */
static void * NCDObject_DataPtr (const NCDObject *o);

/**
 * Returns the 'data_int' attribute; see {@link NCDObject_BuildFull}.
 */
static int NCDObject_DataInt (const NCDObject *o);

/**
 * Returns the 'method_user' attribute; see {@link NCDObject_BuildFull}.
 */
static void * NCDObject_MethodUser (const NCDObject *o);

/**
 * Attempts to resolve a variable within the object.
 * This just calls {@link NCDObject_func_getvar}, but also has some assertions to detect
 * incorrect behavior of the callback.
 */
static int NCDObject_GetVar (const NCDObject *o, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value) WARN_UNUSED;

/**
 * Attempts to resolve an object within the object.
 * This just calls {@link NCDObject_func_getobj}, but also has some assertions to detect
 * incorrect behavior of the callback.
 */
static int NCDObject_GetObj (const NCDObject *o, NCD_string_id_t name, NCDObject *out_object) WARN_UNUSED;

/**
 * Resolves a variable expression starting with this object.
 * A variable expression is usually represented in dotted form,
 * e.g. object1.object2.variable (for a named variable) or object1.object2.object3
 * (for an empty string variable). This function however receives the expression
 * as an array of string identifiers.
 * 
 * Consult the implementation for exact semantics of variable expression resolution.
 * 
 * @param o object to start the resolution with
 * @param names pointer to an array of names for the resolution. May be NULL if num_names is 0.
 * @param num_names number in names in the array
 * @param mem pointer to the memory object where the resulting value
 *            should be constructed
 * @param out_value If the variable exists, *out_value will be set to the value
 *                  reference to the result value. If the variable exists but there
 *                  was an error constructing the value, will be set to an
 *                  invalid value reference. May be modified even if the variable
 *                  does not exist.
 * @return 1 if the variable exists, 0 if not
 */
static int NCDObject_ResolveVarExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDValMem *mem, NCDValRef *out_value) WARN_UNUSED;

/**
 * Resolves an object expression starting with this object.
 * An object expression is usually represented in dotted form,
 * e.g. object1.object2.object3. This function however receives the expression
 * as an array of string identifiers.
 * 
 * Consult the implementation for exact semantics of object expression resolution.
 * 
 * @param o object to start the resolution with
 * @param names pointer to an array of names for the resolution. May be NULL if num_names is 0.
 * @param num_names number in names in the array
 * @param out_object If the object exists, *out_object will be set to the result
 *                   object. May be modified even if the object does not exist.
 * @return 1 if the object exists, 0 if not
 */
static int NCDObject_ResolveObjExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDObject *out_object) WARN_UNUSED;

/**
 * Returns 0. This can be used as a dummy variable resolution callback.
 */
int NCDObject_no_getvar (const NCDObject *obj, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value);

/**
 * Returns 0. This can be used as a dummy object resolution callback.
 */
int NCDObject_no_getobj (const NCDObject *obj, NCD_string_id_t name, NCDObject *out_object);

//

NCDObject NCDObject_Build (NCD_string_id_t type, void *data_ptr, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj)
{
    ASSERT(type >= -1)
    ASSERT(func_getvar)
    ASSERT(func_getobj)
    
    NCDObject obj;
    obj.type = type;
    obj.data_int = 0;
    obj.data_ptr = data_ptr;
    obj.method_user = data_ptr;
    obj.func_getvar = func_getvar;
    obj.func_getobj = func_getobj;
    
    return obj;
}

NCDObject NCDObject_BuildFull (NCD_string_id_t type, void *data_ptr, int data_int, void *method_user, NCDObject_func_getvar func_getvar, NCDObject_func_getobj func_getobj)
{
    ASSERT(type >= -1)
    ASSERT(func_getvar)
    ASSERT(func_getobj)
    
    NCDObject obj;
    obj.type = type;
    obj.data_int = data_int;
    obj.data_ptr = data_ptr;
    obj.method_user = method_user;
    obj.func_getvar = func_getvar;
    obj.func_getobj = func_getobj;
    
    return obj;
}

NCD_string_id_t NCDObject_Type (const NCDObject *o)
{
    return o->type;
}

void * NCDObject_DataPtr (const NCDObject *o)
{
    return o->data_ptr;
}

int NCDObject_DataInt (const NCDObject *o)
{
    return o->data_int;
}

void * NCDObject_MethodUser (const NCDObject *o)
{
    return o->method_user;
}

int NCDObject_GetVar (const NCDObject *o, NCD_string_id_t name, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(name >= 0)
    ASSERT(mem)
    ASSERT(out_value)
    
    int res = o->func_getvar(o, name, mem, out_value);
    
    ASSERT(res == 0 || res == 1)
    ASSERT(res == 0 || (NCDVal_Assert(*out_value), 1))
    
    return res;
}

int NCDObject_GetObj (const NCDObject *o, NCD_string_id_t name, NCDObject *out_object)
{
    ASSERT(name >= 0)
    ASSERT(out_object)
    
    int res = o->func_getobj(o, name, out_object);
    
    ASSERT(res == 0 || res == 1)
    
    return res;
}

static NCDObject NCDObject__dig_into_object (NCDObject object)
{
    NCDObject obj2;
    while (NCDObject_GetObj(&object, NCD_STRING_EMPTY, &obj2)) {
        object = obj2;
    }
    
    return object;
}

int NCDObject_ResolveVarExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDValMem *mem, NCDValRef *out_value)
{
    ASSERT(num_names == 0 || names)
    ASSERT(mem)
    ASSERT(out_value)
    
    NCDObject object = NCDObject__dig_into_object(*o);
    
    while (num_names > 0) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, *names, &obj2)) {
            if (num_names == 1 && NCDObject_GetVar(&object, *names, mem, out_value)) {
                return 1;
            }
            
            return 0;
        }
        
        object = NCDObject__dig_into_object(obj2);
        
        names++;
        num_names--;
    }
    
    return NCDObject_GetVar(&object, NCD_STRING_EMPTY, mem, out_value);
}

int NCDObject_ResolveObjExprCompact (const NCDObject *o, const NCD_string_id_t *names, size_t num_names, NCDObject *out_object)
{
    ASSERT(num_names == 0 || names)
    ASSERT(out_object)
    
    NCDObject object = NCDObject__dig_into_object(*o);
    
    while (num_names > 0) {
        NCDObject obj2;
        if (!NCDObject_GetObj(&object, *names, &obj2)) {
            return 0;
        }
        
        object = NCDObject__dig_into_object(obj2);
        
        names++;
        num_names--;
    }
    
    *out_object = object;
    return 1;
}

#endif
