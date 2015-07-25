#include "ppvarutils.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_messaging.h"

#include <string.h>


/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|.
 */
struct PP_Var CStrToVar2(const char* str) {
  return g_ppb_var->VarFromUtf8(str, strlen(str));
}


