/**
 * k2pdfopt_module.c
 *
 * This file implements a Native Client module that wraps around
 * k2pdfopt. The module mounts the HTML5 file system in memory at
 * /temporary and then sets up a worker thread that waits for a
 * ppapi message of the form
 *  {
 *    "cmd": "k2pdfopt",
 *    "args": ['...', '...', ...]
 *  }
 * Once the worker thread receives a message that matches the above
 * form it calls k2pdfopt's main function with the provided command
 * line arguments.
 *
 * This file is based on the "nacl_io_demo.c" example file of the
 * Native Client SDK, to which the following license applies:
 *
 * BEGIN LICENSE FOR NACL_IO_DEMO.C ====================================
 *
 * Copyright 2011, The Chromium Authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * END LICENSE FOR NACL_IO_DEMO.C ======================================
 */

#include "k2pdfopt_module.h"
#include "k2pdfopt_nacl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <pthread.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_io/nacl_io.h"

#include "queue.h"


static PP_Instance g_instance = 0;
static PPB_GetInterface g_get_browser_interface = NULL;
static PPB_Messaging* g_ppb_messaging = NULL;
PPB_Var* g_ppb_var = NULL;
PPB_VarArray* g_ppb_var_array = NULL;
PPB_VarDictionary* g_ppb_var_dictionary = NULL;


/** A handle to the thread that handles messages. */
static pthread_t g_handle_message_thread;

/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|.
 */
struct PP_Var CStrToVar(const char* str) {
  return g_ppb_var->VarFromUtf8(str, strlen(str));
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A printf format string.
 * @param[in] args The printf arguments.
 * @return The newly constructed string. Caller takes ownership. */
char* VprintfToNewString(const char* format, va_list args) {
  va_list args_copy;
  int length;
  char* buffer;
  int result;

  va_copy(args_copy, args);
  length = vsnprintf(NULL, 0, format, args);
  buffer = (char*)malloc(length + 1); /* +1 for NULL-terminator. */
  result = vsnprintf(&buffer[0], length + 1, format, args_copy);
  if (result != length) {
    assert(0);
    return NULL;
  }
  return buffer;
}

/**
 * Vprintf to a new PP_Var.
 * @param[in] format A print format string.
 * @param[in] va_list The printf arguments.
 * @return A new PP_Var.
 */
static struct PP_Var VprintfToVar(const char* format, va_list args) {
  struct PP_Var var;
  char* string = VprintfToNewString(format, args);
  var = g_ppb_var->VarFromUtf8(string, strlen(string));
  free(string);
  return var;
}

/**
 * Convert a PP_Var to a C string.
 * @param[in] var The PP_Var to convert.
 * @return A newly allocated, NULL-terminated string.
 */
static char* VarToCStr(struct PP_Var var) {
  uint32_t length;
  const char* str = g_ppb_var->VarToUtf8(var, &length);
  if (str == NULL) {
    return NULL;
  }

   // str is NOT NULL-terminated. Copy using memcpy. 
  char* new_str = (char*)malloc(length + 1);
  memcpy(new_str, str, length);
  new_str[length] = 0;
  return new_str;
}

/**
 * Get a value from a Dictionary, given a string key.
 * @param[in] dict The dictionary to look in.
 * @param[in] key The key to look up.
 * @return PP_Var The value at |key| in the |dict|. If the key doesn't exist,
 *     return a PP_Var with the undefined value.
 */
struct PP_Var GetDictVar(struct PP_Var dict, const char* key) {
  struct PP_Var key_var = CStrToVar(key);
  struct PP_Var value = g_ppb_var_dictionary->Get(dict, key_var);
  g_ppb_var->Release(key_var);
  return value;
}

/**
 * Post a message to JavaScript.
 * @param[in] format A printf format string.
 * @param[in] ... The printf arguments.
 */
int pp_post_message(const char* category, const char* format, ...) {
  va_list args;

  struct PP_Var dict_var = g_ppb_var_dictionary->Create();

  struct PP_Var cat_key = CStrToVar("category");
  struct PP_Var cat_value = CStrToVar(category);
  PP_Bool result = g_ppb_var_dictionary->Set(dict_var, cat_key, cat_value);
  g_ppb_var->Release(cat_key);
  g_ppb_var->Release(cat_value);

  if (!result) {
    g_ppb_var->Release(dict_var);
    return 1;
  }

  struct PP_Var msg_key = CStrToVar("msg");
  va_start(args, format);
  struct PP_Var msg_value = VprintfToVar(format, args);
  va_end(args);

  result = g_ppb_var_dictionary->Set(dict_var, msg_key, msg_value);
  g_ppb_var->Release(msg_key);
  g_ppb_var->Release(msg_value);

  if (!result) {
    g_ppb_var->Release(dict_var);
    return 2;
  }

  g_ppb_messaging->PostMessage(g_instance, dict_var);
  g_ppb_var->Release(dict_var);

  return 0;
}


/**
 * Post a "progress" message to JavaScript. It has the form
 * {
 *   "category": "progress",
 *   "current": <number>,
 *   "total": <number>
 * }
 */
int pp_post_progress(int current_page, int page_count) {
  struct PP_Var dict_var = g_ppb_var_dictionary->Create();

  struct PP_Var cat_key = CStrToVar("category");
  struct PP_Var cat_value = CStrToVar("progress");
  PP_Bool result = g_ppb_var_dictionary->Set(dict_var, cat_key, cat_value);
  g_ppb_var->Release(cat_key);
  g_ppb_var->Release(cat_value);
  if (!result) {
    g_ppb_var->Release(dict_var);
    return 1;
  }

  struct PP_Var cur_key = CStrToVar("current");
  struct PP_Var cur_value = PP_MakeInt32(current_page);
  result = g_ppb_var_dictionary->Set(dict_var, cur_key, cur_value);
  g_ppb_var->Release(cur_key);
  g_ppb_var->Release(cur_value);
  if (!result) {
    g_ppb_var->Release(dict_var);
    return 2;
  }

  struct PP_Var total_key = CStrToVar("total");
  struct PP_Var total_value = PP_MakeInt32(page_count);
  result = g_ppb_var_dictionary->Set(dict_var, total_key, total_value);
  g_ppb_var->Release(total_key);
  g_ppb_var->Release(total_value);
  if (!result) {
    g_ppb_var->Release(dict_var);
    return 3;
  }

  g_ppb_messaging->PostMessage(g_instance, dict_var);
  g_ppb_var->Release(dict_var);

  return 0;
}


/**
 * Given a message from JavaScript, parse it for functions and parameters.
 *
 * The format of the message is:
 * {
 *  "cmd": <function name>,
 *  "args": [<arg0>, <arg1>, ...]
 * }
 *
 * @param[in] message The message to parse.
 * @param[out] out_function The function name.
 * @param[out] out_params A PP_Var array.
 * @return 0 if successful, otherwise 1.
 */
static int ParseMessage(struct PP_Var message,
                        char** out_function,
                        struct PP_Var* out_params) {
  if (message.type != PP_VARTYPE_DICTIONARY) {
    return 1;
  }

  struct PP_Var cmd_value = GetDictVar(message, "cmd");
  if (cmd_value.type != PP_VARTYPE_STRING) {
    g_ppb_var->Release(cmd_value);
    return 1;
  }
  *out_function = VarToCStr(cmd_value);
  g_ppb_var->Release(cmd_value);

  *out_params = GetDictVar(message, "args");
  if (out_params->type != PP_VARTYPE_ARRAY) {
    g_ppb_var->Release(*out_params);
    return 1;
  }

  return 0;
}


/**
 * Handle as message from JavaScript on the worker thread.
 *
 * @param[in] message The message to parse and handle.
 */
static void HandleMessage(struct PP_Var message) {
  pp_post_message("debug", "Received a message.");

  char* command;
  struct PP_Var params;
  if (ParseMessage(message, &command, &params)) {
    pp_post_message("error", "Unable to parse message.");
    return;
  }

  if (!strcmp(command, "k2pdfopt")) {
    pp_post_message("debug", "Received k2pdfopt command.");

    int numargs = g_ppb_var_array->GetLength(params);
    int argc = numargs + 1;
    char **argv;
    argv = malloc(argc * sizeof(char*));
    argv[0] = command;

    for (int i=0; i<numargs; i++) {
      struct PP_Var argvar = g_ppb_var_array->Get(params, i);
      argv[i+1] = VarToCStr(argvar);
      g_ppb_var->Release(argvar);
    }

    pp_post_message("status", "start");
    k2pdfoptmain(numargs+1, argv);
    pp_post_message("status", "done");

    // Fre argv[1...numargs+1]; argv[0] will be freed below by calling free(command)
    for (int i=1; i<numargs+1; i++) {
      free(argv[i]);
    }
    free(argv);
  }

  free(command);
}


/**
 * A worker thread that handles messages from JavaScript.
 * @param[in] user_data Unused.
 * @return unused.
 */
void* HandleMessageThread(void* user_data) {
  while (1) {
    struct PP_Var message = DequeueMessage();
    HandleMessage(message);
    g_ppb_var->Release(message);
  }
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_instance = instance;
  nacl_io_init_ppapi(instance, g_get_browser_interface);

  // By default, nacl_io mounts / to pass through to the original NaCl
  // filesystem (which doesn't do much). Let's remount it to a memfs
  // filesystem.
  umount("/");
  mount("", "/", "memfs", 0, "");
  mount("", "/temporary", "html5fs", 0, "type=TEMPORARY,expected_size=5242880");

  pthread_create(&g_handle_message_thread, NULL, &HandleMessageThread, NULL);

  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {
}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  /* NaCl modules do not need to handle the document load function. */
  return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  g_ppb_var->AddRef(message);
  if (!EnqueueMessage(message)) {
    g_ppb_var->Release(message);
    pp_post_message("error", "dropped message because the queue was full.");
  }
}

#define GET_INTERFACE(var, type, name)            \
  var = (type*)(get_browser(name));               \
  if (!var) {                                     \
    printf("Unable to get interface " name "\n"); \
    return PP_ERROR_FAILED;                       \
  }

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
  g_get_browser_interface = get_browser;
  GET_INTERFACE(g_ppb_messaging, PPB_Messaging, PPB_MESSAGING_INTERFACE);
  GET_INTERFACE(g_ppb_var, PPB_Var, PPB_VAR_INTERFACE);
  GET_INTERFACE(g_ppb_var_array, PPB_VarArray, PPB_VAR_ARRAY_INTERFACE);
  GET_INTERFACE(
      g_ppb_var_dictionary, PPB_VarDictionary, PPB_VAR_DICTIONARY_INTERFACE);
  return PP_OK;
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    static PPP_Instance instance_interface = {
        &Instance_DidCreate,
        &Instance_DidDestroy,
        &Instance_DidChangeView,
        &Instance_DidChangeFocus,
        &Instance_HandleDocumentLoad,
    };
    return &instance_interface;
  } else if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0) {
    static PPP_Messaging messaging_interface = {
        &Messaging_HandleMessage,
    };
    return &messaging_interface;
  }
  return NULL;
}

PP_EXPORT void PPP_ShutdownModule() {
}
