/*
  gcc -Wall -fPIC -DENABLE_XIM `pkg-config ecore --cflags --libs` -c imxim.c 
  gcc -Wall -shared -Wl,-soname,im-xim.so -o im-xim.so ./imxim.o
*/
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Evas.h>
#include <Ecore_X.h>

#include <stdio.h>
#include <stdlib.h>

#define _(x) x

static void ecore_imf_context_xim_client_window_set(Ecore_IMF_Context *ctx,
                                                    void *window) {
   Ecore_Window win = (Ecore_Window)window;

}

static const Ecore_IMF_Context_Info xim_info = {
    "xim",                      /* ID */
    _("X input method"),           /* Description */
    "ko:ja:th:zh",              /* Default locales */
    "evas",                     /* Canvas type */
    1,                         /* Canvas required */
};

static Ecore_IMF_Context_Class xim_class = {
    NULL,                       /* add */
    NULL,                       /* del */
    NULL,                       /* client_window_set */
    NULL,                       /* client_canvas_set */
    NULL,                       /* show */
    NULL,                       /* hide */
    NULL,                       /* get_preedit_string */
    NULL,                       /* focus_in */
    NULL,                       /* focus_out */
    NULL,                       /* reset */
    NULL,                       /* cursor_position_set */
    NULL,                       /* use_preedit_set */
    NULL,                       /* input_mode_set */
    NULL,                       /* filter_event */
};

Ecore_IMF_Context *imf_module_create(void) {
#ifdef ENABLE_XIM
   Ecore_IMF_Context *ctx;
   ctx = ecore_imf_context_new(&xim_class);
   return ctx;
#else
   return NULL;
#endif  /* ENABLE_XIM */
}

Ecore_IMF_Context *imf_module_exit(void) {
#ifdef ENABLE_XIM
   return NULL;
#else
   return NULL;
#endif  /* ENABLE_XIM */
}

Eina_Bool ecore_imf_xim_init(void) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);
#ifdef ENABLE_XIM
   ecore_x_init(NULL);
   ecore_imf_module_register(&xim_info,
                             imf_module_create,
                             imf_module_exit);
   return EINA_TRUE;
#else
   return EINA_FALSE;
#endif  /* ENABLE_XIM */
}

void ecore_imf_xim_shutdown(void) {
#ifdef ENABLE_XIM
   ecore_x_shutdown();
#endif  /* ENABLE_XIM */
}

EINA_MODULE_INIT(ecore_imf_xim_init);
EINA_MODULE_SHUTDOWN(ecore_imf_xim_shutdown);
