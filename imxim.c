/*
   gcc -Wall -fPIC -DENABLE_XIM `pkg-config ecore --cflags --libs` -c imxim.c
   gcc -Wall -shared -Wl,-soname,im-xim.so -o im-xim.so ./imxim.o
 */
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Evas.h>
#include <Ecore_X.h>
#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _(x) x

struct _xim_data_t
{
   Display *dsp;
   XIM      im;
   XIC      ic; /* Input context for composed characters */
};

/* prototype */
struct _xim_data_t *    xim_data_new(Display *dsp);
void                    xim_data_destroy(struct _xim_data_t *xim_data);
XIC                     xim_data_ic_get(struct _xim_data_t *xim_data);
XIM                     xim_data_im_get(struct _xim_data_t *xim_data);
void                    xim_data_ic_set(struct _xim_data_t *xim_data, XIC ic);
void                    xim_data_im_set(struct _xim_data_t *xim_data, XIM im);

static void _ecore_imf_context_xim_client_window_set(Ecore_IMF_Context *ctx,
                                                     void              *window) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);
   struct _xim_data_t *xim_data;
   Ecore_Window win;
   Ecore_X_Window client_window;
   XIC ic;
   XIM im;

   xim_data = ecore_imf_context_data_get(ctx);
   win = (Ecore_Window)window;
   client_window = (Ecore_X_Window)win;

   im = xim_data_im_get(xim_data);
   if(im) {
      ic = XCreateIC(im,
                     XNInputStyle,
                     XIMPreeditNothing | XIMStatusNothing,
                     XNClientWindow,
                     client_window,
                     NULL);
      if (ic)
          xim_data_ic_set(xim_data, ic);
   }
} /* _ecore_imf_context_xim_client_window_set */

static void _ecore_imf_context_xim_focus_in(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);

   XIC ic;
   struct _xim_data_t *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data);
   if(ic)
      XSetICFocus(ic);
} /* _ecore_imf_context_xim_focus_in */

static void _ecore_imf_context_xim_focus_out(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);

   XIC ic;
   struct _xim_data_t *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data);
   if(ic)
      XUnsetICFocus(ic);
} /* _ecore_imf_context_xim_focus_out */

static Eina_Bool _ecore_imf_context_xim_filter_event(Ecore_IMF_Context   *ctx,
                                                     Ecore_IMF_Event_Type type,
                                                     Ecore_IMF_Event     *event) {
   struct _xim_data_t *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);

#if 0
   if (XFilterEvent(&ev, ev.xkey.window))
       continue;
#endif
   return EINA_TRUE;
} /* _ecore_imf_context_xim_filter_event */

static const Ecore_IMF_Context_Info xim_info = {
   "xim", /* ID */
   _("X input method"), /* Description */
   "ko:ja:th:zh", /* Default locales */
   "evas", /* Canvas type */
   1, /* Canvas required */
};

static Ecore_IMF_Context_Class xim_class = {
   NULL, /* add */
   NULL, /* del */
   _ecore_imf_context_xim_client_window_set,  /* client_window_set */
   NULL, /* client_canvas_set */
   NULL, /* show */
   NULL, /* hide */
   NULL, /* get_preedit_string */
   _ecore_imf_context_xim_focus_in, /* focus_in */
   _ecore_imf_context_xim_focus_out, /* focus_out */
   NULL, /* reset */
   NULL, /* cursor_position_set */
   NULL, /* use_preedit_set */
   NULL, /* input_module_set */
   _ecore_imf_context_xim_filter_event, /* filter_event */
};

Ecore_IMF_Context *xim_imf_module_create(void) {
#ifdef ENABLE_XIM
   struct _xim_data_t *xim_data = NULL;
   Ecore_X_Display *dsp = NULL;
   Ecore_IMF_Context *ctx = NULL;

   dsp = ecore_x_display_get();
   if(!dsp)
      goto error;

   xim_data = xim_data_new(dsp);
   if(!xim_data)
      goto error;

   ctx = ecore_imf_context_new(&xim_class);
   if(!ctx)
      goto error;

   ecore_imf_context_data_set(ctx, xim_data);

   return ctx;

error:
   xim_data_destroy(xim_data);
   free(ctx);
   return NULL;
#else /* ifdef ENABLE_XIM */
   return NULL;
#endif  /* ENABLE_XIM */
} /* xim_imf_module_create */

Ecore_IMF_Context *xim_imf_module_exit(void) {
#ifdef ENABLE_XIM
   return NULL;
#else /* ifdef ENABLE_XIM */
   return NULL;
#endif  /* ENABLE_XIM */
} /* xim_imf_module_exit */

Eina_Bool ecore_imf_xim_init(void) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);
#ifdef ENABLE_XIM
   ecore_x_init(NULL);
   ecore_imf_module_register(&xim_info,
                             xim_imf_module_create,
                             xim_imf_module_exit);
   return EINA_TRUE;
#else /* ifdef ENABLE_XIM */
   return EINA_FALSE;
#endif  /* ENABLE_XIM */
} /* ecore_imf_xim_init */

void ecore_imf_xim_shutdown(void) {
#ifdef ENABLE_XIM
   ecore_x_shutdown();
#endif  /* ENABLE_XIM */
} /* ecore_imf_xim_shutdown */

EINA_MODULE_INIT(ecore_imf_xim_init);
EINA_MODULE_SHUTDOWN(ecore_imf_xim_shutdown);

/*
 * iternal function
 */
struct _xim_data_t *xim_data_new(Display *dsp)
{
   struct _xim_data_t *xim_data = NULL;
   XIMStyle chosen_style;
   XIMStyles *supported_styles;
   char *ret;
   int i;

   if(!dsp)
      return NULL;

   if(!XSupportsLocale())
      return NULL;

   xim_data = calloc(1, sizeof(struct _xim_data_t));
   if(!xim_data)
      return NULL;

   xim_data->dsp = dsp;

   XSetLocaleModifiers("");
   xim_data->im = XOpenIM(xim_data->dsp, NULL, NULL, NULL);
   if(!xim_data->im)
      goto error;

   ret = XGetIMValues(xim_data->im, XNQueryInputStyle,
                      &supported_styles, NULL);
   if(ret || !supported_styles)
      goto error;

   for(i = 0; i < supported_styles->count_styles; i++) {
        if(supported_styles->supported_styles[i] ==
           (XIMPreeditNothing | XIMStatusNothing))
           chosen_style = supported_styles->supported_styles[i];
     }
   XFree(supported_styles);
   if(!chosen_style)
      goto error;

   return xim_data;
error:
   xim_data_destroy(xim_data);
   return NULL;
} /* xim_data_new */

void xim_data_destroy(struct _xim_data_t *xim_data) {
   if(!xim_data)
      return;

   if(xim_data->ic)
      XDestroyIC(xim_data->ic);

   if(xim_data->im)
      XCloseIM(xim_data->im);

   free(xim_data);
} /* xim_data_destroy */

XIC xim_data_ic_get(struct _xim_data_t *xim_data) {
   if(!xim_data)
      return NULL;

   return xim_data->ic;
} /* xim_data_ic_get */

XIM xim_data_im_get(struct _xim_data_t *xim_data) {
   if(!xim_data)
      return NULL;

   return xim_data->im;
} /* xim_data_im_get */

void xim_data_ic_set(struct _xim_data_t *xim_data, XIC ic) {
   if(!xim_data)
       return;
   xim_data->ic = ic;
} /* xim_data_ic_set */

void xim_data_im_set(struct _xim_data_t *xim_data, XIM im) {
   if(!xim_data)
      return;
   xim_data->im = im;
} /* xim_data_im_set */

