/** @file

  Plugin init

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include "ink_config.h"
#include <stdio.h>
#if defined(hpux)
#include <dl.h>
#endif
#include "ink_platform.h"
#include "ink_file.h"
#include "Compatability.h"
#include "ParseRules.h"
#include "I_RecCore.h"
#include "I_Layout.h"
#include "InkAPIInternal.h"
#include "Main.h"
#include "Plugin.h"
#include "PluginDB.h"
#include "stats/Stats.h"

// HPUX:
//   LD_SHAREDCMD=ld -b
// SGI:
//   LD_SHAREDCMD=ld -shared
// OSF:
//   LD_SHAREDCMD=ld -shared -all -expect_unresolved "*"
// Solaris:
//   LD_SHAREDCMD=ld -G


static const char *plugin_dir = ".";
static const char *extensions_dir = ".";
static PluginDB *plugin_db = NULL;

typedef void (*init_func_t) (int argc, char *argv[]);
typedef void (*init_func_w_handle_t) (void *handle, int argc, char *argv[]);
typedef int (*lic_req_func_t) (void);

inkapi int
load_in_export_symbols(int j)
{
  int i = eight_bit_table[j];
  return i;
}

// Plugin registration vars
//
//    plugin_reg_list has an entry for each plugin
//      we've successfully been able to load
//    plugin_reg_current is used to associate the
//      plugin we're in the process of loading with
//      it struct.  We need this global pointer since
//      the API doesn't have any plugin context.  Init
//      is single threaded so we can get away with the
//      global pointer
//
DLL<PluginRegInfo> plugin_reg_list;
PluginRegInfo *plugin_reg_current = NULL;

PluginRegInfo::PluginRegInfo():
plugin_registered(false), plugin_path(NULL), sdk_version(PLUGIN_SDK_VERSION_UNKNOWN),
plugin_name(NULL), vendor_name(NULL), support_email(NULL)
{
}

static void *
dll_open(char *fn, bool global)
{
  int global_flags = global ? RTLD_GLOBAL : 0;
  return (void *) dlopen(fn, RTLD_NOW | global_flags);
}

static void *
dll_findsym(void *dlp, const char *name)
{
  return (void *) dlsym(dlp, name);
}

static char *
dll_error(void *dlp)
{
  return (char *) dlerror();
}

static void
dll_close(void *dlp)
{
  dlclose(dlp);
}


static void
plugin_load(int argc, char *argv[], bool internal)
{
  char path[PATH_NAME_MAX + 1];
  void *handle;
  init_func_t init;
  lic_req_func_t lic_req;
  PluginRegInfo *plugin_reg_temp;
  const char *pdir = internal ? extensions_dir : plugin_dir;

  if (argc < 1) {
    return;
  }
  ink_filepath_make(path, sizeof(path), pdir, argv[0]);

  Note("loading plugin '%s'", path);

  plugin_reg_temp = plugin_reg_list.head;
  while (plugin_reg_temp) {
    if (strcmp(plugin_reg_temp->plugin_path, path) == 0) {
      Warning("multiple loading of plugin %s", path);
      break;
    }
    plugin_reg_temp = (plugin_reg_temp->link).next;
  }

  handle = dll_open(path, (internal ? true : false));
  if (!handle) {
    Error("unable to load '%s': %s", path, dll_error(handle));
    abort();
  }

  lic_req = (lic_req_func_t) dll_findsym(handle, "INKPluginLicenseRequired");
  if (lic_req && lic_req() != 0) {
    PluginDB::CheckLicenseResult result = plugin_db->CheckLicense(argv[0]);
    if (result != PluginDB::license_ok) {
      Error("unable to load '%s': %s", path, PluginDB::CheckLicenseResultStr[result]);
      dll_close(handle);
      abort();
    }
  }
  // Allocate a new registration structure for the
  //    plugin we're starting up
  ink_assert(plugin_reg_current == NULL);
  plugin_reg_current = new PluginRegInfo;
  plugin_reg_current->plugin_path = xstrdup(path);

  init_func_w_handle_t inith = (init_func_w_handle_t) dll_findsym(handle, "INKPluginInitwDLLHandle");
  if (inith) {
    inith(handle, argc, argv);
    return;
  }

  init = (init_func_t) dll_findsym(handle, "INKPluginInit");
  if (!init) {
    Error("unable to find INKPluginInit function '%s': %s", path, dll_error(handle));
    dll_close(handle);
    abort();
  }

  init(argc, argv);

  plugin_reg_list.push(plugin_reg_current);
  plugin_reg_current = NULL;
  //dll_close(handle);
}

static char *
plugin_expand(char *arg)
{
  RecDataT data_type;
  char *str = NULL;

  if (*arg != '$') {
    return (char *) NULL;
  }
  // skip the $ character
  arg += 1;

  if (RecGetRecordDataType(arg, &data_type) != REC_ERR_OKAY) {
    goto not_found;
  }

  switch (data_type) {
  case RECD_STRING:
    {
      RecString str_val;
      if (RecGetRecordString_Xmalloc(arg, &str_val) != REC_ERR_OKAY) {
        goto not_found;
      }
      return (char *) str_val;
      break;
    }
  case RECD_FLOAT:
    {
      RecFloat float_val;
      if (RecGetRecordFloat(arg, &float_val) != REC_ERR_OKAY) {
        goto not_found;
      }
      str = (char *) xmalloc(128);
      snprintf(str, 128, "%f", (float) float_val);
      return str;
      break;
    }
  case RECD_INT:
    {
      RecInt int_val;
      if (RecGetRecordInt(arg, &int_val) != REC_ERR_OKAY) {
        goto not_found;
      }
      str = (char *) xmalloc(128);
      snprintf(str, 128, "%ld", (long int) int_val);
      return str;
      break;
    }
  case RECD_LLONG:
    {
      RecLLong llong_val;
      if (RecGetRecordLLong(arg, &llong_val) != REC_ERR_OKAY) {
        goto not_found;
      }
      str = (char *) xmalloc(128);
      snprintf(str, 128, "%lld", (long long) llong_val);
      return str;
      break;
    }
  case RECD_COUNTER:
    {
      RecCounter count_val;
      if (RecGetRecordCounter(arg, &count_val) != REC_ERR_OKAY) {
        goto not_found;
      }
      str = (char *) xmalloc(128);
      snprintf(str, 128, "%ld", (long int) count_val);
      return str;
      break;
    }
  default:
    goto not_found;
    break;
  }


not_found:
  Warning("plugin.config: unable to find parameter %s", arg);
  return NULL;
}


int
plugins_exist(const char *config_dir)
{
  char path[PATH_NAME_MAX + 1];
  char line[1024], *p;
  int fd;
  int plugin_count = 0;

  // XXX: Commented out.
  //      Why reading plugin_dir when unused?
  //
  // RecGetRecordString_Xmalloc("proxy.config.plugin.plugin_dir", (char **)&plugin_dir);

  ink_filepath_make(path, sizeof(path), config_dir, "plugin.config");
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    Warning("unable to open plugin config file '%s': %d, %s", path, errno, strerror(errno));
    return 0;
  }
  while (ink_file_fd_readline(fd, sizeof(line) - 1, line) > 0) {
    p = line;
    // strip leading white space and test for comment or blank line
    while (*p && ParseRules::is_wslfcr(*p))
      ++p;
    if ((*p == '\0') || (*p == '#'))
      continue;
    plugin_count++;
  }
  close(fd);
  return plugin_count;
}

void
plugin_init(const char *config_dir, bool internal)
{
  char path[PATH_NAME_MAX + 1];
  char line[1024], *p;
  char *argv[64];
  char *vars[64];
  int argc;
  int fd;
  int i;
  static bool INIT_ONCE = true;

  if (INIT_ONCE) {
    api_init();
    init_inkapi_stat_system();
    char *cfg = NULL;

    RecGetRecordString_Xmalloc("proxy.config.plugin.plugin_dir", (char**)&cfg);
    if (cfg != NULL) {
      plugin_dir = Layout::get()->relative(cfg);
      xfree(cfg);
      cfg = NULL;
    }
    RecGetRecordString_Xmalloc("proxy.config.plugin.extensions_dir", (char**)&cfg);
    if (cfg != NULL) {
      extensions_dir = Layout::get()->relative(cfg);
      xfree(cfg);
      cfg = NULL;
    }
    ink_filepath_make(path, sizeof(path), config_dir, "plugin.db");
    plugin_db = new PluginDB(path);
    INIT_ONCE = false;
  }

  ink_assert(plugin_db);

  if (internal == false) {
    ink_filepath_make(path, sizeof(path), config_dir, "plugin.config");
  } else {
    ink_filepath_make(path, sizeof(path), config_dir, "extensions.config");
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    /* secret extensions dont complain */
    if (internal == false) {
      Warning("unable to open plugin config file '%s': %d, %s", path, errno, strerror(errno));
    }
    return;
  }

  while (ink_file_fd_readline(fd, sizeof(line) - 1, line) > 0) {
    argc = 0;
    p = line;

    // strip leading white space and test for comment or blank line
    while (*p && ParseRules::is_wslfcr(*p))
      ++p;
    if ((*p == '\0') || (*p == '#'))
      continue;

    // not comment or blank, so rip line into tokens
    while (1) {
      while (*p && ParseRules::is_wslfcr(*p))
        ++p;
      if ((*p == '\0') || (*p == '#'))
        break;                  // EOL

      if (*p == '\"') {
        p += 1;

        argv[argc++] = p;

        while (*p && (*p != '\"')) {
          p += 1;
        }
        if (*p == '\0') {
          break;
        }
        *p++ = '\0';
      } else {
        argv[argc++] = p;

        while (*p && !ParseRules::is_wslfcr(*p) && (*p != '#')) {
          p += 1;
        }
        if ((*p == '\0') || (*p == '#')) {
          break;
        }
        *p++ = '\0';
      }
    }

    for (i = 0; i < argc; i++) {
      vars[i] = plugin_expand(argv[i]);
      if (vars[i]) {
        argv[i] = vars[i];
      }
    }

    plugin_load(argc, argv, internal);

    for (i = 0; i < argc; i++) {
      if (vars[i]) {
        xfree(vars[i]);
      }
    }
  }

  close(fd);
}

