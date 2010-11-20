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

struct _xim_im_info_t {
   Ecore_X_Window win;
   char *locale;
   XIM      im;
};

struct _xim_data_t
{
   Ecore_X_Window win;
   long mask;
   XIC      ic; /* Input context for composed characters */
   char *locale;
    
};

/* prototype */
struct _xim_data_t *    xim_data_new();
void                    xim_data_destroy(struct _xim_data_t *xim_data);
Ecore_X_Window          xim_data_window_get(struct _xim_data_t *xim_data);
void xim_data_window_set(struct _xim_data_t *xim_data, Ecore_X_Window win);
void xim_data_event_mask_set(struct _xim_data_t *xim_data, long mask);
long xim_data_event_mask_get(struct _xim_data_t *xim_data);
XIC                     xim_data_ic_get(struct _xim_data_t *xim_data);
XIM                     xim_data_im_get(struct _xim_data_t *xim_data);
void                    xim_data_im_set(struct _xim_data_t *xim_data, XIM im);

static XIM get_im(Ecore_X_Window *window, char *locale) {
   XIM im = NULL;
   Eina_list *l; 
   void *data;
   struct _xim_im_info_t *im_info = NULL;
   struct _xim_im_info_t *info = NULL;
   EINA_LIST_FOREACH(open_ims, l, im_info) {
      if(im_info->win == win &&
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
      info = calloc(1, sizeof(struct _xim_im_info_t));
      if(!info) return NULL;
      info->win = window;
      info->locale = strdup(locale);
   }

   if(!XSupportsLocale("")) 

   info->im = XOpenIM(dsp, NULL, NULL, NULL);
   if(!info->im) goto error;
   
   

   return im;
 error:
   free(info->locale);
   free(info->im);
   free(info);
   return NULL;
}

static void set_ic_client_window(struct _xim_data_t *xim_data,
                                 Ecore_X_Window *window) {
   /* reinitialize IC */
   if(xim_data->ic) {
      XDestroyIC(xim_data->ic);
      xim_data->ic = NULL;
   }

   if(xim_data->win) {
      ;
   }
   xim_data->win = window;
   if(xim_data->win) {
      xim_data->im = get_im(xim_data->win, xim_data->locale);
   }
}         

static void _ecore_imf_context_xim_client_window_set(Ecore_IMF_Context *ctx,
                                                     void              *window) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);
   struct _xim_data_t *xim_data;
   Ecore_X_Window win;
   Ecore_X_Window old_win;
   XIC ic;
   XIM im;

   xim_data = ecore_imf_context_data_get(ctx);
   old_win = xim_data_window_get(xim_data);
   win = (Ecore_X_Window)((Ecore_Window)window);
   
   im = xim_data_im_get(xim_data);
   if(im) {
      ic = XCreateIC(im,
                     XNInputStyle,
                     XIMPreeditNothing | XIMStatusNothing,
                     XNClientWindow,
                     win,
                     NULL);
      if (ic) {
         long mask;

         XGetICValues(ic, XNFilterEvents, &mask, NULL);
         printf("ICValues mask:0x%ld\n", mask);
         ecore_x_event_mask_set(win, mask);
         xim_data_window_set(xim_data, win);
         xim_data_event_mask_set(xim_data, mask);
      }
   }
} /* _ecore_imf_context_xim_client_window_set */

static void _ecore_imf_context_xim_get_preedit_string(Ecore_IMF_Context *ctx,
                                                      char **str,
                                                      int *cursor_pos) {
   return;
}

static void _ecore_imf_context_xim_focus_in(Ecore_IMF_Context *ctx) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);

   XIC ic;
   struct _xim_data_t *xim_data;
   xim_data = ecore_imf_context_data_get(ctx);
   ic = xim_data_ic_get(xim_data);
   if(ic) {
#if 0                           /* XXX */
      char *str;
        XSetICValues(ic, XNFocusWindow, xevent->xfocus.window, NULL);
#ifdef X_HAVE_UTF8_STRING
        if ((str = Xutf8ResetIC(ic)))
#else
        if ((str = XmbResetIC(ic)))
#endif
           XFree(str);
#endif

      XSetICFocus(ic);
   }
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

static void _ecore_imf_context_xim_reset(Ecore_IMF_Context *ctx) {
   return;
}

static void _ecore_imf_context_xim_cursor_position_set(Ecore_IMF_Context *ctx,
                                                       int cursor_pos) {
   return;
}

static void _ecore_imf_context_xim_use_preedit_set(Ecore_IMF_Context *ctx,
                                                   int use_preedit) {
   return;
}

static unsigned int
_ecore_x_event_reverse_modifiers(unsigned int state)
{
#if 0
   unsigned int modifiers = 0;

   if (state & ECORE_EVENT_MODIFIER_SHIFT)
       modifiers |= ECORE_X_MODIFIER_SHIFT;

   if (state & ECORE_EVENT_MODIFIER_CTRL)
       modifiers |= ECORE_X_MODIFIER_CTRL;

   if (state & ECORE_EVENT_MODIFIER_ALT)
       modifiers |= ECORE_X_MODIFIER_ALT;

   if (state & ECORE_EVENT_MODIFIER_WIN)
      modifiers |= ECORE_X_MODIFIER_WIN;

   if (state & ECORE_EVENT_LOCK_SCROLL)
      modifiers |= ECORE_X_LOCK_SCROLL;

   if (state & ECORE_EVENT_LOCK_NUM)
      modifiers |= ECORE_X_LOCK_NUM;

   if (state & ECORE_EVENT_LOCK_CAPS)
      modifiers |= ECORE_X_LOCK_CAPS;

   return modifiers;
#endif
   return state;
}

static Eina_Bool _ecore_imf_context_xim_filter_event(Ecore_IMF_Context   *ctx,
                                                     Ecore_IMF_Event_Type type,
                                                     Ecore_IMF_Event     *event) {
   EINA_LOG_DBG("%s in\n", __FUNCTION__);
   struct _xim_data_t *xim_data;
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

   if(type == ECORE_IMF_EVENT_KEY_DOWN) {
      Ecore_IMF_Event_Key_Down *ev = (Ecore_IMF_Event_Key_Down *)event;
      EINA_LOG_DBG("%s in\n", __FUNCTION__);
      printf("ev->string:%s ev->compose:%s\n", ev->string, ev->compose);

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
      xev.state = _ecore_x_event_reverse_modifiers(ev->modifiers);
      _keycode = XKeysymToKeycode(dsp,
                                  XStringToKeysym(event->key_down.keyname));
      xev.keycode = _keycode;
      xev.same_screen = True;

#if 0
      if(ic) {
         Status mbstatus;
         EINA_LOG_DBG("%s in\n", __FUNCTION__);
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
         printf("mb val:%d status:%d\n", val, mbstatus);
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
                printf("mb overflow val:%d status:%d\n", val, mbstatus);
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
#endif
          {
         XComposeStatus status;
         val = XLookupString(&xev,
                             compose_buffer,
                             sizeof(compose_buffer),
                             &sym,
                             &status);
         printf("lookup val:%d status.chars_matched:%d\n", val,
                status.chars_matched);
         if (val > 0) {
            compose_buffer[val] = '\0';
            compose = eina_str_convert(nl_langinfo(CODESET),
                                       "UTF-8", compose_buffer);
         }
      }

      printf("compose:%s\n", compose);
      if(compose) {
         ecore_imf_context_commit_event_add(ctx, compose);
         free(compose);
         return EINA_TRUE;
      }
   }

   return EINA_FALSE;
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
   _ecore_imf_context_xim_get_preedit_string, /* get_preedit_string */
   _ecore_imf_context_xim_focus_in, /* focus_in */
   _ecore_imf_context_xim_focus_out, /* focus_out */
   _ecore_imf_context_xim_reset, /* reset */
   _ecore_imf_context_xim_cursor_position_set, /* cursor_position_set */
   _ecore_imf_context_xim_use_preedit_set, /* use_preedit_set */
   NULL, /* input_module_set */
   _ecore_imf_context_xim_filter_event, /* filter_event */
};

Ecore_IMF_Context *xim_imf_module_create(void) {
#ifdef ENABLE_XIM
   struct _xim_data_t *xim_data = NULL;
   Ecore_IMF_Context *ctx = NULL;

   xim_data = xim_data_new();
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
struct _xim_data_t *xim_data_new()
{
   Ecore_X_Display *dsp;
   struct _xim_data_t *xim_data = NULL;
   XIMStyle chosen_style;
   XIMStyles *supported_styles;
   char *ret;
   int i;
   char *locale;

   dsp = ecore_x_display_get();
   if(!dsp) return NULL;

   locale = setlocale(LC_CTYPE, "");
   if(!locale) return NULL;

   if(!XSupportsLocale()) return NULL;

   xim_data = calloc(1, sizeof(struct _xim_data_t));
   if(!xim_data) return NULL;

   xim_data->locale = strdup(locale);
   if(!xim_data->locale) goto error;

   if(!XSetLocaleModifiers("")) goto error;
   xim_data->im = XOpenIM(dsp, NULL, NULL, NULL);
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

   free(xim_data->locale);
   free(xim_data);
} /* xim_data_destroy */

Ecore_X_Window xim_data_window_get(struct _xim_data_t *xim_data) {
   if(!xim_data) return -1;     /* XXX */
   return xim_data->win;
}

void xim_data_window_set(struct _xim_data_t *xim_data, Ecore_X_Window win) {
   if(!xim_data) return;
   xim_data->win = win;
}

void xim_data_event_mask_set(struct _xim_data_t *xim_data, long mask) {
   if(!xim_data) return;
   xim_data->mask = mask;
}

long xim_data_event_mask_get(struct _xim_data_t *xim_data) {
   if(!xim_data) return 0;
   return xim_data->mask;
}

XIC xim_data_ic_get(struct _xim_data_t *xim_data) {
   if(!xim_data)
       return NULL;

   if(!xim_data->ic) {
      XIM ic = NULL;
      ic = XCreateIC(xim_data->im,
                     XNInputStyle,
                     XIMPreeditNothing | XIMStatusNothing,
                     XNClientWindow,
                     xim_data->win,
                     NULL);
      if (ic) {
         long mask;

         XGetICValues(ic, XNFilterEvents, &mask, NULL);
         ecore_x_event_mask_set(xim_data->win, mask);
         xim_data->mask = mask;
         xim_data->ic = ic; 
      }
   }
   return xim_data->ic;
} /* xim_data_ic_set */

XIM xim_data_im_get(struct _xim_data_t *xim_data) {
   if(!xim_data)
      return NULL;

   return xim_data->im;
} /* xim_data_im_get */

void xim_data_im_set(struct _xim_data_t *xim_data, XIM im) {
   if(!xim_data)
      return;
   xim_data->im = im;
} /* xim_data_im_set */

