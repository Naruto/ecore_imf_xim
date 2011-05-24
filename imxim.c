/*
  gcc -Wall -Werror -fPIC `pkg-config ecore --cflags --libs` -c imxim.c
  gcc -Wall -Werror -shared -Wl,-soname,im-xim.so -o im-xim.so ./imxim.o
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
#include <assert.h>

#define _(x) x

static Eina_List *open_ims = NULL;

typedef struct _XIM_Im_Info XIM_Im_Info;
struct _XIM_Im_Info
{
    Ecore_X_Window win;
    char *locale;
    XIM      im;
    Eina_List *ics;
    Eina_Bool reconnecting;
    XIMStyles *xim_styles;    
};

typedef struct _Ecore_IMF_Context_Data Ecore_IMF_Context_Data;
struct _Ecore_IMF_Context_Data
{
    Ecore_X_Window win;
    long mask;
    XIC ic; /* Input context for composed characters */
    char *locale;
    XIM_Im_Info *im_info;
    int preedit_length;
    int preedit_cursor;
    Eina_Bool use_preedit;
    Eina_Bool filter_key_release;
    Eina_Bool finalizing;       /* XXX finalize() に該当するものは？ */
    Eina_Bool has_focus;
    Eina_Bool in_toplevel;

    XIMCallback preedit_start_cb;
    XIMCallback preedit_done_cb;
    XIMCallback preedit_draw_cb;
    XIMCallback preedit_caret_cb;
};

/* prototype */
Ecore_IMF_Context_Data *imf_context_data_new();
void             imf_context_data_destroy(Ecore_IMF_Context_Data *imf_context_data);
static void reinitialize_ic(Ecore_IMF_Context_Data *imf_context_data);
// static void reinitialize_all_ics(XIM_Im_Info *info);

static int
preedit_start_callback(XIC xic, XPointer client_data, XPointer call_data)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context *ctx = (Ecore_IMF_Context *)client_data;
#if 0
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);
#endif

   /* XXX check finalize */
   ecore_imf_context_preedit_start_event_add(ctx);

   return -1;
}

static void
preedit_done_callback(XIC xic, XPointer client_data, XPointer call_data)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context *ctx = (Ecore_IMF_Context *)client_data;
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);

   if(imf_context_data->preedit_length) {
      imf_context_data->preedit_length = 0;
      ecore_imf_context_preedit_changed_event_add(ctx);
   }
   /* XXX check finalize */
   ecore_imf_context_preedit_end_event_add(ctx);
}

static void
preedit_draw_callback(XIC xic, XPointer client_data, 
                      XIMPreeditDrawCallbackStruct *call_data)
{
   EINA_LOG_DBG("in");
#if 0
   Ecore_IMF_Context *ctx = (Ecore_IMF_Context *)client_data;
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);
#endif

   printf("caret:%d chg_first:%d chg_length:%d\n",
          call_data->caret, call_data->chg_first, call_data->chg_length);
   XIMText *t = call_data->text;
   if(t) {
      printf("XIM_TEXT length:%d encoding:%d\n",
             t->length, t->encoding_is_wchar);
   }
}

static void
preedit_caret_callback(XIC xic, XPointer client_data,
                       XIMPreeditCaretCallbackStruct *call_data)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context *ctx = (Ecore_IMF_Context *)client_data;
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);

   if(call_data->direction == XIMAbsolutePosition) {
      printf("call_data->position:%d\n", call_data->position);
      imf_context_data->preedit_cursor = call_data->position;
      ecore_imf_context_preedit_changed_event_add(ctx);
   }
}

static XVaNestedList
preedit_callback_set(Ecore_IMF_Context *ctx)
{
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);

   imf_context_data->preedit_start_cb.client_data = (XPointer)ctx;
   imf_context_data->preedit_start_cb.callback = (XIMProc)preedit_start_callback;

   imf_context_data->preedit_done_cb.client_data = (XPointer)ctx;
   imf_context_data->preedit_done_cb.callback = (XIMProc)preedit_done_callback;

   imf_context_data->preedit_draw_cb.client_data = (XPointer)ctx;
   imf_context_data->preedit_draw_cb.callback = (XIMProc)preedit_draw_callback;

   imf_context_data->preedit_caret_cb.client_data = (XPointer)ctx;
   imf_context_data->preedit_caret_cb.callback = (XIMProc)preedit_caret_callback;

   return XVaCreateNestedList(0,
                              XNPreeditStartCallback,
                              &imf_context_data->preedit_start_cb,
                              XNPreeditDoneCallback,
                              &imf_context_data->preedit_done_cb,
                              XNPreeditDrawCallback,
                              &imf_context_data->preedit_draw_cb,
                              XNPreeditCaretCallback,
                              &imf_context_data->preedit_caret_cb,
                              NULL);
}

static XIC
get_ic(Ecore_IMF_Context *ctx)
{
   Ecore_IMF_Context_Data *imf_context_data;
   XIC ic;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);
   ic = imf_context_data->ic;
   if(!ic) {
      XIM_Im_Info *im_info = imf_context_data->im_info;
      long mask;
      XVaNestedList preedit_attr = preedit_callback_set(ctx);
      // XIMStyle im_style = XIMPreeditNothing | XIMStatusNothing;
      XIMStyle im_style = XIMPreeditCallbacks | XIMStatusNothing;

      ic = XCreateIC(im_info->im,
                     XNInputStyle, im_style,
                     XNClientWindow, imf_context_data->win,
                     XNPreeditAttributes, preedit_attr,
                     XNStatusAttributes, NULL,
                     NULL);
      XFree(preedit_attr);
      if(!ic) return NULL;

      XGetICValues(ic, XNFilterEvents, &mask, NULL);
      ecore_x_event_mask_set(imf_context_data->win, mask);
      imf_context_data->mask = mask;
      imf_context_data->ic = ic;
   }

   return ic;
}

static void
_ecore_imf_context_xim_add(Ecore_IMF_Context *ctx)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context_Data *imf_context_data = NULL;

   imf_context_data = imf_context_data_new();
   if(!imf_context_data) return;

   imf_context_data->use_preedit = EINA_TRUE;
   imf_context_data->filter_key_release = EINA_FALSE;
   imf_context_data->finalizing = EINA_FALSE;
   imf_context_data->has_focus = EINA_FALSE;
   imf_context_data->in_toplevel = EINA_FALSE;

   ecore_imf_context_data_set(ctx, imf_context_data);
}

static void
_ecore_imf_context_xim_del(Ecore_IMF_Context *ctx)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = (Ecore_IMF_Context_Data *)ecore_imf_context_data_get(ctx);
   imf_context_data_destroy(imf_context_data);
}

static void
xim_destroy_callback(XIM xim, XPointer client_data, XPointer call_data)
{
   return;
} 

static void
setup_im(XIM_Im_Info *info)
{
   XIMValuesList *ic_values = NULL;
   XIMCallback im_destroy_callback;

   if(!info->im)
       return;

  im_destroy_callback.client_data = (XPointer)info;
  im_destroy_callback.callback = (XIMProc)xim_destroy_callback;
  XSetIMValues(info->im,
               XNDestroyCallback, &im_destroy_callback,
               NULL);

  XGetIMValues(info->im,
               XNQueryInputStyle, &info->xim_styles,
               XNQueryICValuesList, &ic_values,
               NULL);

  if(ic_values) {
     int i;
     
     for(i = 0; i < ic_values->count_values; i++)
         if(strcmp (ic_values->supported_values[i],
                    XNStringConversionCallback) == 0) {
            // info->supports_string_conversion = TRUE;
            break;
         }
#if 0
     for(i = 0; i < ic_values->count_values; i++)
         printf("%s\n", ic_values->supported_values[i]);
     for(i = 0; i < info->xim_styles->count_styles; i++)
         printf("%lx\n", info->xim_styles->supported_styles[i]);
#endif
     XFree(ic_values);
  }

#if 0                           /* legacy code */
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
#endif
}

static void
xim_instantiate_callback(Display *display, XPointer client_data,
                         XPointer call_data)
{
   XIM_Im_Info *info = (XIM_Im_Info *)client_data;
   XIM im = NULL;

   im = XOpenIM(display, NULL, NULL, NULL);

   if(!im)
       return;

   info->im = im;
   setup_im (info);

   XUnregisterIMInstantiateCallback (display, NULL, NULL, NULL,
                                     xim_instantiate_callback,
                                     (XPointer)info);
   info->reconnecting = EINA_FALSE;
}

/* initialize info->im */
static void
xim_info_try_im(XIM_Im_Info *info)
{
   Ecore_X_Display *dsp;

   assert(info->im == NULL);
   if (info->reconnecting == EINA_TRUE)
       return;

   if(XSupportsLocale()) {
      if (!XSetLocaleModifiers (""))
          EINA_LOG_WARN("Unable to set locale modifiers with XSetLocaleModifiers()");
      dsp = ecore_x_display_get();
      info->im = XOpenIM(dsp, NULL, NULL, NULL);
      if(!info->im) {
         XRegisterIMInstantiateCallback(dsp,
                                        NULL, NULL, NULL,
                                        xim_instantiate_callback,
                                        (XPointer)info);
         info->reconnecting = EINA_TRUE;
         return;
      }
      setup_im(info);
   } 
}

static XIM_Im_Info*
get_im(Ecore_X_Window window, char *locale)
{
   EINA_LOG_DBG("in");

   Eina_List *l; 
   XIM_Im_Info *im_info = NULL;
   XIM_Im_Info *info = NULL;
   EINA_LIST_FOREACH(open_ims, l, im_info) {
      if(strcmp(im_info->locale, locale) == 0) {
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
      open_ims = eina_list_prepend(open_ims, info);
      info->win = window;
      info->locale = strdup(locale);
      info->reconnecting = EINA_FALSE;
   }

   xim_info_try_im(info);
   return info;
}

static void
reinitialize_ic(Ecore_IMF_Context_Data *imf_context_data)
{
   XIC ic;
   ic = imf_context_data->ic;
   if(ic) {
      XDestroyIC(ic);
      imf_context_data->ic = NULL;
      if(imf_context_data->preedit_length) {
         imf_context_data->preedit_length = 0;
         if (imf_context_data->finalizing == EINA_FALSE)
             ;
             /* XXX */
             /* ecore_imf_context_preedit_changed_event_add(); */
      }
   }
}

#if 0
static void
reinitialize_all_ics(XIM_Im_Info *info)
 {
#if 0
   Eina_List *tmp_list;
   void *data;
   EINA_LIST_FOREACH(info->ics, tmp_list, data)
    reinitialize_ic (tmp_list->data);
#endif
}
#endif

static void
set_ic_client_window(Ecore_IMF_Context_Data *imf_context_data,
                     Ecore_X_Window window)
{
   EINA_LOG_DBG("in");
   Ecore_X_Window old_win;

   /* reinitialize IC */
   reinitialize_ic(imf_context_data);

   old_win = imf_context_data->win;
   EINA_LOG_DBG("old_win:%d window:%d ", old_win, window);
   if(old_win != 0 && old_win != window) { /* XXX how do check window... */
      XIM_Im_Info *info;
      info = imf_context_data->im_info;
      EINA_LOG_DBG("info:%p", info);
      info->ics = eina_list_remove(info->ics, imf_context_data);
      imf_context_data->im_info = NULL;
   }

   imf_context_data->win = window;

   if(window) {                 /* XXX */
         XIM_Im_Info *info = NULL;
         char *locale;
         locale = imf_context_data->locale;
         info = get_im(window, locale);
         imf_context_data->im_info = info;
   }
}

static void
_ecore_imf_context_xim_client_window_set(Ecore_IMF_Context *ctx,
                                         void              *window)
{
   EINA_LOG_DBG("in");
   Ecore_IMF_Context_Data *imf_context_data;

   imf_context_data = ecore_imf_context_data_get(ctx);
   set_ic_client_window(imf_context_data,
                        (Ecore_X_Window)((Ecore_Window)window));
} /* _ecore_imf_context_xim_client_window_set */

static void
_ecore_imf_context_xim_preedit_string_get(Ecore_IMF_Context *ctx,
                                          char **str,
                                          int *cursor_pos)
{
#if 0
   XIM_Context *xim_context;

   xim_context = ecore_imf_context_data_get(ctx);
#endif
}

static void
_ecore_imf_context_xim_focus_in(Ecore_IMF_Context *ctx)
{
   EINA_LOG_DBG("in");

   XIC ic;
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = ecore_imf_context_data_get(ctx);
   ic = imf_context_data->ic;
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
   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = ecore_imf_context_data_get(ctx);
   ic = imf_context_data->ic;
   if(ic)
       XUnsetICFocus(ic);
} /* _ecore_imf_context_xim_focus_out */

static void _ecore_imf_context_xim_reset(Ecore_IMF_Context *ctx) {
   XIC ic;
   Ecore_IMF_Context_Data *imf_context_data;
   char *result;

   /* restore conversion state after resetting ic later */
   XIMPreeditState preedit_state = XIMPreeditUnKnown;
   XVaNestedList preedit_attr;
   Eina_Bool  have_preedit_state = EINA_FALSE;

   imf_context_data = ecore_imf_context_data_get(ctx);
   ic = imf_context_data->ic;
   if(!ic)
       return;

   if(imf_context_data->preedit_length == 0)
       return;

   preedit_attr = XVaCreateNestedList(0,
                                      XNPreeditState, &preedit_state,
                                      NULL);
   if(!XGetICValues(ic,
                    XNPreeditAttributes, preedit_attr,
                    NULL))
       have_preedit_state = EINA_TRUE;

   XFree(preedit_attr);

   result = XmbResetIC(ic);

   preedit_attr = XVaCreateNestedList(0,
                                      XNPreeditState, preedit_state,
                                      NULL);
   if(have_preedit_state)
       XSetICValues(ic,
                    XNPreeditAttributes, preedit_attr,
                    NULL);

   XFree(preedit_attr);

   if(result) {
      /* XXX convert to utf8 */
      char *result_utf8 = strdup(result);
      if(result_utf8) {
         ecore_imf_context_commit_event_add(ctx, result_utf8);
         free(result_utf8);
      }
   }

   if(imf_context_data->preedit_length) {
      imf_context_data->preedit_length = 0;
      ecore_imf_context_preedit_changed_event_add(ctx); 
   }

   XFree (result);
}

static void
_ecore_imf_context_xim_cursor_position_set(Ecore_IMF_Context *ctx,
                                           int cursor_pos)
{
   EINA_LOG_DBG("in");
   return;
}

static void
_ecore_imf_context_xim_use_preedit_set(Ecore_IMF_Context *ctx,
                                       Eina_Bool use_preedit)
{
   EINA_LOG_DBG("in");

   Ecore_IMF_Context_Data *imf_context_data;
   imf_context_data = ecore_imf_context_data_get(ctx);
  
   use_preedit = use_preedit != EINA_FALSE;

   if(imf_context_data->use_preedit != use_preedit) {
      imf_context_data->use_preedit = use_preedit;
      reinitialize_ic(imf_context_data);
   }
}

static unsigned int
_ecore_x_event_reverse_modifiers(unsigned int state)
{
   unsigned int modifiers = 0;

   /**< "Control" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_CTRL)
       modifiers |= ControlMask;

   /**< "Alt" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_ALT)
       modifiers |= Mod1Mask;

   /**< "Shift" is pressed */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_SHIFT)
      modifiers |= ShiftMask;

   /**< "Win" (between "Ctrl" and "A */
   if(state & ECORE_IMF_KEYBOARD_MODIFIER_WIN)
     modifiers |= Mod5Mask;

   return modifiers;
}

static unsigned int
_ecore_x_event_reverse_locks(unsigned int state) {
   unsigned int locks = 0;

   /**< "Num" lock is active */
   if(state & ECORE_IMF_KEYBOARD_LOCK_NUM)
       locks |= Mod3Mask;

   if(state & ECORE_IMF_KEYBOARD_LOCK_CAPS)
       locks |= LockMask;

   if(state & ECORE_IMF_KEYBOARD_LOCK_SCROLL)
       ;                        /* XXX */

   return locks;
}

static KeyCode _keycode_get(Ecore_X_Display *dsp, const char *keyname) {
   KeyCode keycode;

   EINA_LOG_DBG("keyname:%s keysym:%lu", keyname, XStringToKeysym(keyname));

   if(strcmp(keyname, "Keycode-0") == 0) { /* XXX fix */
      keycode = 0;
   } else {
      keycode = XKeysymToKeycode(dsp, XStringToKeysym(keyname));
   }

   return keycode;
}

static Eina_Bool _ecore_imf_context_xim_filter_event(Ecore_IMF_Context   *ctx,
                                                     Ecore_IMF_Event_Type type,
                                                     Ecore_IMF_Event     *event) {
   EINA_LOG_DBG("%s in", __FUNCTION__);
   Ecore_IMF_Context_Data *imf_context_data;
   XIC ic;

   Ecore_X_Display *dsp;
   Ecore_X_Window win;

   int val;
   char compose_buffer[256];
   KeySym sym;
   char *compose = NULL;
   char *tmp = NULL;
   XKeyPressedEvent xev;

   imf_context_data = ecore_imf_context_data_get(ctx);
   ic = imf_context_data->ic;
   if(!ic) {
      ic = get_ic(ctx);
   }

   if(type == ECORE_IMF_EVENT_KEY_DOWN) {
      Ecore_IMF_Event_Key_Down *ev = (Ecore_IMF_Event_Key_Down *)event;
      EINA_LOG_DBG("ECORE_IMF_EVENT_KEY_DOWN");

      dsp = ecore_x_display_get();
      win = imf_context_data->win;

      xev.type = KeyPress;
      xev.serial = 0;            /* hope it doesn't matter */
      xev.send_event = 0;        /* XXX change send_event value */
      xev.display = dsp;
      xev.window = win;
      xev.root = ecore_x_window_root_get(win);
      xev.subwindow = win;
      xev.time = ev->timestamp;
      xev.x = xev.x_root = 0;
      xev.y = xev.y_root = 0;
#if 1
      EINA_LOG_INFO("modifiers:%d", ev->modifiers);
      EINA_LOG_INFO("after modifiers:%d",
                    _ecore_x_event_reverse_modifiers(ev->modifiers));
#endif
      xev.state = 0;
      xev.state |= _ecore_x_event_reverse_modifiers(ev->modifiers);
      xev.state |= _ecore_x_event_reverse_locks(ev->locks);
      xev.keycode = _keycode_get(dsp, ev->keyname);
      xev.same_screen = True;

#if 0
      if (XFilterEvent((XEvent *)&xev, (Window)win) == True) {
         printf("filter event\n");
         return EINA_TRUE;
      }
#endif

      if(ic) {
         Status mbstatus;
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
      } else {
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

      // EINA_LOG_INFO("compose:%s", compose);
      if(compose) {
         EINA_LOG_INFO("compose:%s", compose);
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
    .prediction_allow_set = NULL,
    .autocapital_type_set = NULL,
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
   eina_init();
   ecore_x_init(NULL);
   ecore_imf_module_register(&xim_info,
                             xim_imf_module_create,
                             xim_imf_module_exit);

   return EINA_TRUE;
} /* ecore_imf_xim_init */

void ecore_imf_xim_shutdown(void) {
   ecore_x_shutdown();
   eina_shutdown();
} /* ecore_imf_xim_shutdown */

EINA_MODULE_INIT(ecore_imf_xim_init);
EINA_MODULE_SHUTDOWN(ecore_imf_xim_shutdown);

/*
 * iternal function
 */
Ecore_IMF_Context_Data *imf_context_data_new()
{
   Ecore_IMF_Context_Data *imf_context_data = NULL;
   char *locale;

   locale = setlocale(LC_CTYPE, "");
   if(!locale) return NULL;

   if(!XSupportsLocale()) return NULL;

   imf_context_data = calloc(1, sizeof(Ecore_IMF_Context_Data));
   if(!imf_context_data) return NULL;

   imf_context_data->locale = strdup(locale);
   if(!imf_context_data->locale) goto error;

   return imf_context_data;
 error:
   imf_context_data_destroy(imf_context_data);
   return NULL;
} /* imf_context_data_new */

void imf_context_data_destroy(Ecore_IMF_Context_Data *imf_context_data) {
   if(!imf_context_data)
       return;

   if(imf_context_data->ic)
       XDestroyIC(imf_context_data->ic);

   free(imf_context_data->locale);
   free(imf_context_data);
} /* imf_context_data_destroy */

