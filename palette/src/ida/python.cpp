#if PYTHON_SUPPORT
#include <Python.h>

#include "plugin.h"

void init_python_module();
void initpy();

class gil_scoped_acquire {
  PyGILState_STATE state;
  bool reset;

 public:
  gil_scoped_acquire() {
    reset = false;
    state = PyGILState_Ensure();
  }

  ~gil_scoped_acquire() {
    if (!reset) PyGILState_Release(state);
  }
};

// SDK 9.4 event listener replacing the deprecated HT_UI notification callback.
// It waits for the IDAPython plugin to load, installs the module, then unhooks
// itself. va is only read for the ui_plugin_loaded event it carries a
// plugin_info_t for.
struct python_loaded_listener_t : event_listener_t {
  ssize_t idaapi on_event(ssize_t code, va_list va) override {
    if (code == ui_plugin_loaded) {
      auto info = va_arg(va, plugin_info_t*);
      if (info && !strcmp(info->org_name, "IDAPython")) {
        initpy();
        unhook_event_listener(HT_UI, this);
      }
    }
    return 0;
  }
};
static python_loaded_listener_t g_python_loaded_listener;

void initpy() {
  if (!Py_IsInitialized())
    hook_event_listener(HT_UI, &g_python_loaded_listener, &PLUGIN);
  else {
    gil_scoped_acquire gil;
    init_python_module();
  }
}

#endif