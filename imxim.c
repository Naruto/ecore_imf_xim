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
#include <langinfo.h>

#define _(x) x

static Eina_List *open_ims = NULL;

typedef struct _XIM_Im_Info XIM_Im_Info;
struct _XIM_Im_Info
{
    Ecore_X_Window win;
    char *locale;
    XIM      im;
    XIC ic; 
};

typedef struct _XIM_Data XIM_Data;
struct _XIM_Data
{
    Ecore_X_Window win;
    long mask;
    XIC      ic; /* Input context for composed characters */
    char *locale;
    XIM_Im_Info *im_info;
};

/* prototype */
XIM_Data *    xim_data_new();
void                    xim_data_destroy(XIM_Data *xim_data);
Ecore_X_Window          xim_data_window_get(XIM_Data *xim_data);
void xim_data_window_set(XIM_Data *xim_data, Ecore_X_Window win);
void xim_data_event_mask_set(XIM_Data *xim_data, long mask);
long xim_data_event_mask_get(XIM_Data *xim_data);
void xim_data_ic_set(XIM_Data *xim_data, XIC ic);
void xim_data_ic_reinitialize(XIM_Data *xim_data);
XIC                     xim_data_ic_get(XIM_Data *xim_data);
XIM_Im_Info *xim_data_im_info_get(XIM_Data *xim_data);
void xim_data_im_info_set(XIM_Data *xim_data, XIM_Im_Info *im);
void xim_data_locale_set(XIM_Data *xim_data, char *locale);
char *xim_data_locale_get(XIM_Data *xim_data);

static XIC get_ic(XIM_Data *xim_data) {
   XIC ic = xim_data_ic_get(xim_data);
   if(!ic) {
      XIM_Im_Info *im_info = xim_data_im_info_get(xim_data);
      long mask;
      /* XXX */
      ic = XCreateIC(im_info->im,
                     XNInputStyle,
                     XIMPreeditNothing | XIMStatusNothing,
                     XNClientWindow,
                     xim_data->win,
                     NULL);
      if(!ic) return NULL;

      XGetICValues(ic, XNFilterEvents, &mask, NULL);
      ecore_x_event_mask_set(xim_data->win, mask);
      xim_data->mask = mask;
      xim_data->ic = ic; 
   }

   return ic;
}

static void _ecore_imf_context_xim_add(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("in");
   XIM_Data *xim_data = NULL;

   xim_data = xim_data_new();
   if(!xim_data) return;

   ecore_imf_context_data_set(ctx, xim_data);

   return;
}

static void _ecore_imf_context_xim_del(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("in");
   XIM_Data *xim_data;
   xim_data = (XIM_Data *)ecore_imf_context_data_get(ctx);
   xim_data_destroy(xim_data);
}

static XIM_Im_Info *get_im(Ecore_X_Window window, char *locale) {
   EINA_LOG_DBG("in");
   Eina_List *l; 
   XIM_Im_Info *im_info = NULL;
   XIM_Im_Info *info = NULL;
   EINA_LIST_FOREACH(open_ims, l, im_info) {
      if(im_info->win == window &&
         strcmp(im_info->locale, locale) == 0) {
         if(im_info->im) {
            return im_info;
         } else {
            info = im_info;
            break;
         }
      }
   }

   if(!info) {
      info = calloc(1, sizeof(XIM_Im_Info));
      if(!info) return NULL;
      info->win = window;
      info->locale = strdup(locale);
   }

   Ecore_X_Display *dsp;
   dsp = ecore_x_display_get();
   if(!dsp) goto error;
   if(!XSetLocaleModifiers("")) goto error;
   info->im = XOpenIM(dsp, NULL, NULL, NULL);
   if(!info->im)
       goto error;
   EINA_LOG_INFO("im:%p", info->im);

   char *ret;
   XIMStyles *supported_styles;
   ret = XGetIMValues(info->im, XNQueryInputStyle, &supported_styles, NULL);
   if(ret || !supported_styles)
       goto error;

   int i;
   XIMStyle chosen_style;
   for(i = 0; i < supported_styles->count_styles; i++) {
      if(supported_styles->supported_styles[i] ==
         (XIMPreeditNothing | XIMStatusNothing))
          chosen_style = supported_styles->supported_styles[i];
   }
   XFree(supported_styles);
   if(!chosen_style)
       goto error;

   EINA_LOG_INFO("info:%p", info);
   return info;
 error:
   free(info->locale);
   free(info->im);
   free(info);
   return NULL;
}

static void set_ic_client_window(XIM_Data *xim_data, Ecore_X_Window window) {
   EINA_LOG_DBG("in");
   Ecore_X_Window old_win;

   /* reinitialize IC */
   xim_data_ic_reinitialize(xim_data);

   old_win = xim_data_window_get(xim_data);
   EINA_LOG_DBG("old_win:%d", old_win);
   if(old_win) {
      xim_data_window_set(xim_data, window);
   }

   if(window) {
      XIM_Im_Info *info = NULL;
      char *locale;
      locale = xim_data_locale_get(xim_data);
      info = get_im(window, locale);
      xim_data_im_info_set(xim_data, info);
   }
}

static void _ecore_imf_context_xim_client_window_set(Ecore_IMF_Context *ctx,
                                                     void              *window) {
   EINA_LOG_DBG("in");
   XIM_Data *xim_data;

   xim_data = ecore_imf_context_data_get(ctx);
   set_ic_client_window(xim_data, (Ecore_X_Window)((Ecore_Window)window));
} /* _ecore_imf_context_xim_client_window_set */

static void _ecore_imf_context_xim_preedit_string_get(Ecore_IMF_Context *ctx,
                                                      char **str,
                                                      int *cursor_pos) {
   EINA_LOG_DBG("in");
   return;
}

static void _ecore_imf_context_xim_focus_in(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("in");

   XIC ic;
   XIM_Data *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data);
   if(ic) {
      char *str;
#if 0
      XSetICValues(ic, XNFocusWindow, xevent->xfocus.window, NULL);
#endif

#ifdef X_HAVE_UTF8_STRING
      if ((str = Xutf8ResetIC(ic)))
#else
          if ((str = XmbResetIC(ic)))
#endif
              XFree(str);

      XSetICFocus(ic);
   }
} /* _ecore_imf_context_xim_focus_in */

static void _ecore_imf_context_xim_focus_out(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("%s in", __FUNCTION__);

   XIC ic;
   XIM_Data *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data);
   if(ic)
       XUnsetICFocus(ic);
} /* _ecore_imf_context_xim_focus_out */

static void _ecore_imf_context_xim_reset(Ecore_IMF_Context *ctx) {
   return;
}

static void _ecore_imf_context_xim_cursor_position_set(Ecore_IMF_Context *ctx,
                                                       int cursor_pos) {
   EINA_LOG_DBG("in");
   return;
}

static void _ecore_imf_context_xim_use_preedit_set(Ecore_IMF_Context *ctx,
                                                   Eina_Bool use_preedit) {
   EINA_LOG_DBG("in");
   return;
}

static unsigned int
_ecore_x_event_reverse_modifiers(unsigned int state) {
   unsigned int modifiers = 0;

   /**< "Control" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_CTRL)
       modifiers |= ControlMask;

   /**< "Alt" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_ALT)
       ;

   /**< "Shift" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_SHIFT)
      modifiers |= ShiftMask;

   /**< "Win" (between "Ctrl" and "A */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_WIN)
       ;

   return modifiers;
}

#if 0
static unsigned int
_ecore_x_event_reverse_locks(unsigned int state) {
   unsigned int locks = 0;

   /**< "Num" lock is active */
   if(state & ECORE_IMF_KEYBOARD_LOCK_NUM)
       ;

   if(state & ECORE_IMF_KEYBOARD_LOCK_CAPS)
       ;

   if(state & ECORE_IMF_KEYBOARD_LOCK_SCROLL)
       ;

   return locks;
}
#endif

static Eina_Bool _ecore_imf_context_xim_filter_event(Ecore_IMF_Context   *ctx,
                                                     Ecore_IMF_Event_Type type,
                                                     Ecore_IMF_Event     *event) {
   EINA_LOG_DBG("%s in", __FUNCTION__);
   XIM_Data *xim_data;
   XIC ic;

   Ecore_X_Display *dsp;
   Ecore_X_Window win;

   int val;
   char compose_buffer[256];
   KeySym sym;
   char *compose = NULL;
   char *tmp = NULL;
   XKeyPressedEvent xev;
   KeyCode _keycode;

   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data); 
   if(!ic) {
      ic = get_ic(xim_data);
   }
   EINA_LOG_DBG("ic:%p", ic);

   if(type == ECORE_IMF_EVENT_KEY_DOWN) {
      Ecore_IMF_Event_Key_Down *ev = (Ecore_IMF_Event_Key_Down *)event;
      EINA_LOG_DBG("%s in", __FUNCTION__);

      dsp = ecore_x_display_get();
      win = xim_data_window_get(xim_data);

      xev.type = KeyPress;
      xev.serial = 0;            /* hope it doesn't matter */
      xev.send_event = 0;        /* XXX */
      xev.display = dsp;
      xev.window = win;
      xev.root = ecore_x_window_root_get(xev.window);
      xev.subwindow = xev.window;
      xev.time = ev->timestamp;
      xev.x = xev.x_root = 0;
      xev.y = xev.y_root = 0;
#if 0
      EINA_LOG_INFO("modifiers:%d", ev->modifiers);
      EINA_LOG_INFO("after modifiers:%d",
                    _ecore_x_event_reverse_modifiers(ev->modifiers));
#endif
      xev.state = _ecore_x_event_reverse_modifiers(ev->modifiers);
      _keycode = XKeysymToKeycode(dsp,
                                  XStringToKeysym(event->key_down.keyname));
      xev.keycode = _keycode;
      xev.same_screen = True;

      if(ic) {
         Status mbstatus;
         EINA_LOG_DBG("ic:%p", ic);
#ifdef X_HAVE_UTF8_STRING
         val = Xutf8LookupString(ic,
                                 &xev,
                                 compose_buffer,
                                 sizeof(compose_buffer) - 1,
                                 &sym,
                                 &mbstatus);
#else /* ifdef X_HAVE_UTF8_STRING */
         val = XmbLookupString(ic,
                               &xev,
                               compose_buffer,
                               sizeof(compose_buffer) - 1,
                               &sym,
                               &mbstatus);
#endif /* ifdef X_HAVE_UTF8_STRING */
         if (mbstatus == XBufferOverflow) {
            tmp = malloc(sizeof (char) * (val + 1));
            if (!tmp) {
               return EINA_FALSE;
            }

            compose = tmp;

#ifdef X_HAVE_UTF8_STRING
            val = Xutf8LookupString(ic,
                                    (XKeyEvent *)&xev,
                                    tmp,
                                    val,
                                    &sym,
                                    &mbstatus);
#else /* ifdef X_HAVE_UTF8_STRING */
            val = XmbLookupString(ic,
                                  (XKeyEvent *)&xev,
                                  tmp,
                                  val,
                                  &sym,
                                  &mbstatus);
#endif /* ifdef X_HAVE_UTF8_STRING */
            if (val > 0)
                {
                   tmp[val] = '\0';
#ifndef X_HAVE_UTF8_STRING
                   compose = eina_str_convert(nl_langinfo(CODESET),
                                              "UTF-8", tmp);
                   free(tmp);
                   tmp = compose;
#endif /* ifndef X_HAVE_UTF8_STRING */
                }
            else
                compose = NULL;
         } else if (val > 0) {
            compose_buffer[val] = '\0';
#ifdef X_HAVE_UTF8_STRING
            compose = strdup(compose_buffer);
#else /* ifdef X_HAVE_UTF8_STRING */
            compose = eina_str_convert(nl_langinfo(CODESET), "UTF-8",
                                       compose_buffer);
#endif /* ifdef X_HAVE_UTF8_STRING */
         }
      } else
          {
             XComposeStatus status;
             val = XLookupString(&xev,
                                 compose_buffer,
                                 sizeof(compose_buffer),
                                 &sym,
                                 &status);
             if (val > 0) {
                compose_buffer[val] = '\0';
                compose = eina_str_convert(nl_langinfo(CODESET),
                                           "UTF-8", compose_buffer);
             }
          }

      if(compose) {
         // EINA_LOG_INFO("compose:%s", compose);
         ecore_imf_context_commit_event_add(ctx, compose);
         free(compose);
         return EINA_TRUE;
      }
   }

   return EINA_FALSE;
} /* _ecore_imf_context_xim_filter_event */

static const Ecore_IMF_Context_Info xim_info = {
    .id = "xim",
    .description = _("X input method"),
    .default_locales = "ko:ja:th:zh",
    .canvas_type = "evas",
    .canvas_required = 1,
};

static Ecore_IMF_Context_Class xim_class = {
    .add = _ecore_imf_context_xim_add,
    .del = _ecore_imf_context_xim_del,
    .client_window_set = _ecore_imf_context_xim_client_window_set,
    .client_canvas_set = NULL,
    .show = NULL,
    .hide = NULL,
    .preedit_string_get = _ecore_imf_context_xim_preedit_string_get,
    .focus_in = _ecore_imf_context_xim_focus_in,
    .focus_out = _ecore_imf_context_xim_focus_out,
    .reset = _ecore_imf_context_xim_reset,
    .cursor_position_set = _ecore_imf_context_xim_cursor_position_set,
    .use_preedit_set = _ecore_imf_context_xim_use_preedit_set,
    .input_mode_set = NULL,
    .filter_event = _ecore_imf_context_xim_filter_event,
};

Ecore_IMF_Context *xim_imf_module_create(void) {
   EINA_LOG_DBG("%s in", __FUNCTION__);
   Ecore_IMF_Context *ctx = NULL;

   ctx = ecore_imf_context_new(&xim_class);
   if(!ctx)
       goto error;

   return ctx;

 error:
   free(ctx);
   return NULL;
} /* xim_imf_module_create */

Ecore_IMF_Context *xim_imf_module_exit(void) {
   return NULL;
} /* xim_imf_module_exit */

Eina_Bool ecore_imf_xim_init(void) {
   EINA_LOG_DBG("%s in", __FUNCTION__);
   ecore_x_init(NULL);
   ecore_imf_module_register(&xim_info,
                             xim_imf_module_create,
                             xim_imf_module_exit);

   return EINA_TRUE;
} /* ecore_imf_xim_init */

void ecore_imf_xim_shutdown(void) {
   ecore_x_shutdown();
} /* ecore_imf_xim_shutdown */

EINA_MODULE_INIT(ecore_imf_xim_init);
EINA_MODULE_SHUTDOWN(ecore_imf_xim_shutdown);

/*
 * iternal function
 */
XIM_Data *xim_data_new()
{
   XIM_Data *xim_data = NULL;
   char *locale;

   locale = setlocale(LC_CTYPE, "");
   if(!locale) return NULL;

   if(!XSupportsLocale()) return NULL;

   xim_data = calloc(1, sizeof(XIM_Data));
   if(!xim_data) return NULL;

   xim_data->locale = strdup(locale);
   if(!xim_data->locale) goto error;

   return xim_data;
 error:
   xim_data_destroy(xim_data);
   return NULL;
} /* xim_data_new */

void xim_data_destroy(XIM_Data *xim_data) {
   if(!xim_data)
       return;

   if(xim_data->ic)
       XDestroyIC(xim_data->ic);

   free(xim_data->locale);
   free(xim_data);
} /* xim_data_destroy */

Ecore_X_Window xim_data_window_get(XIM_Data *xim_data) {
   if(!xim_data) return -1;     /* XXX */
   return xim_data->win;
}

void xim_data_window_set(XIM_Data *xim_data, Ecore_X_Window win) {
   if(!xim_data) return;
   xim_data->win = win;
}

void xim_data_event_mask_set(XIM_Data *xim_data, long mask) {
   if(!xim_data) return;
   xim_data->mask = mask;
}

long xim_data_event_mask_get(XIM_Data *xim_data) {
   if(!xim_data) return 0;
   return xim_data->mask;
}

void xim_data_ic_set(XIM_Data *xim_data, XIC ic) {
   if(!xim_data)
       return;
   xim_data->ic = ic;
}

XIC xim_data_ic_get(XIM_Data *xim_data) {
   if(!xim_data)
       return NULL;

   return xim_data->ic;
} /* xim_data_ic_set */

void xim_data_ic_reinitialize(XIM_Data *xim_data) {
   if(!xim_data)
       return;
   if(xim_data->ic) {
      XDestroyIC(xim_data->ic);
      xim_data->ic = NULL;
   }
}

XIM_Im_Info *xim_data_im_info_get(XIM_Data *xim_data) {
   if(!xim_data)
       return NULL;

   return xim_data->im_info;
} /* xim_data_im_get */

void xim_data_im_info_set(XIM_Data *xim_data, XIM_Im_Info *im_info) {
   if(!xim_data)
       return;
   xim_data->im_info = im_info;
} /* xim_data_im_set */

void xim_data_locale_set(XIM_Data *xim_data, char *locale) {
   if(!xim_data) return;
   if(xim_data->locale) free(xim_data->locale);
   xim_data->locale = strdup(locale);
}

char *xim_data_locale_get(XIM_Data *xim_data) {
   if(!xim_data) return NULL;
   return xim_data->locale;
}
