/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "infobar_ui.h"

static GtkWidget *infobar;
static GtkWidget *infobar_tabs;
static GtkWidget *infobar_toggles;

static GtkWidget *lyr_toggle;
static GtkWidget *bio_toggle;
static GtkWidget *dlt_toggle;

static GtkWidget *lyr_tab;
static GtkWidget *bio_tab;

static GtkWidget *bio_image;
static GdkPixbuf *bio_pixbuf;

static GtkTextBuffer *lyr_buffer;
static GtkTextBuffer *bio_buffer;

static ddb_gtkui_t *gtkui_plugin;

/* Called when user checks "Infobar" item in "View" menu. */
static gboolean
infobar_menu_toggle(GtkMenuItem *item, gpointer data) {
    
    gboolean state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
    state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 
    deadbeef->conf_set_int(CONF_INFOBAR_VISIBLE, (gint) state);
    return FALSE;
}

/* Called when user switches the infobar tabs. When the toggle was clicked,
 * appropriate tab should be selected. */
static gboolean
infobar_tab_changed(GtkToggleButton *toggle, GtkWidget *widget) {
    
    gint index = gtk_notebook_page_num(GTK_NOTEBOOK(infobar_tabs), widget);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(infobar_tabs), index);
    return FALSE;
}

/* Called when user resizes artist's image on "Biography" tab to redraw
 * the image with the new size. */
static gboolean
bio_image_expose(GtkWidget *image, GdkEventExpose *event, gpointer data) {
    
    if(bio_pixbuf) {
        
        float ww = gdk_pixbuf_get_width(bio_pixbuf);
        float wh = gdk_pixbuf_get_height(bio_pixbuf);
    
        float aw = image->allocation.width;
        float ah = image->allocation.height;
    
        if(aw < 10) aw = 10;
        if(ah < 10) ah = 10;
        
        float w = 0, h = 0;
        float ratio = wh / ww;
    
        if(ww > wh) {
            w = ww > aw ? aw : ww;
            h = w * ratio;
        } else {
            h = wh > ah ? ah : wh;
            w = h / ratio;
        }

        if(w > aw) {
            w = aw;
            h = w * ratio;
        }
        if(h > ah) {
            h = ah;
            w = h / ratio;
        }
        
        int pos_x = (aw - w) / 2;
        int pos_y = (ah - h) / 2;
        
        GdkPixbuf *sld = gdk_pixbuf_scale_simple(bio_pixbuf, w, h, GDK_INTERP_BILINEAR);
        if(sld) {
            cairo_t *cr = gdk_cairo_create(image->window);
            if(cr) {
                gdk_cairo_set_source_pixbuf(cr, sld, pos_x, pos_y);
                cairo_paint(cr);
                cairo_destroy(cr);
            }
            g_object_unref(sld);
        }
    }
    return FALSE;
}

/* Called when user clicks on delete cache button. */
static void
delete_cache_clicked(void) {
    
    int res = -1;
    
    GtkWidget *main_wnd = gtkui_plugin->get_mainwin();
    GtkWidget *dlt_dlg = gtk_message_dialog_new(GTK_WINDOW(main_wnd), 
            GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, 
            "Cache files for the current track wiil be removed. Continue?");
    
    gint choise = gtk_dialog_run(GTK_DIALOG(dlt_dlg));
    switch(choise) {
    case GTK_RESPONSE_YES:
    {
        char lyr_path[512] = {0};
        res = get_cache_path(lyr_path, sizeof(lyr_path), LYRICS);
        if(res > 0) {
            char lyr_file[512] = {0};
            res = snprintf(lyr_file, sizeof(lyr_file), "%s/%s-%s", lyr_path, artist, title);
            if(res > 0) {
                res = remove(lyr_file);
                if(res != 0) {
                    trace("infobar: failed to remove lyrics cache file\n");
                }
            } 
        }
        
        char bio_path[512] = {0};
        res = get_cache_path(bio_path, sizeof(bio_path), BIO);
        if(res > 0) {
            char bio_file[512] = {0};
            res = snprintf(bio_file, sizeof(bio_file), "%s/%s", bio_path, artist);
            if(res > 0) {
                res = remove(bio_file);
                if(res != 0) {
                    trace("infobar: failed to remove bio cache file\n");
                }
            }
            
            char bio_img[512] = {0};
            res = snprintf(bio_img, sizeof(bio_img), "%s/%s_img", bio_path, artist);
            if(res > 0) {
                res = remove(bio_img);
                if(res != 0) {
                    trace("infobar: failed to remove bio image file\n");
                }
            }
        }
        memset(old_artist, 0, sizeof(old_artist));
        memset(old_title, 0, sizeof(old_title));
    }
        break;
    case GTK_RESPONSE_NO:
        break;
    }
    gtk_widget_destroy(dlt_dlg);
}

/* Shows/Hides specified tab. */
static void
set_tab_visible(GtkWidget *toggle, GtkWidget *item, gboolean visible) {
    
    if(visible) {
        gtk_widget_show(toggle);
        gtk_widget_show(item);
    } else {
        gtk_widget_hide(toggle);
        gtk_widget_hide(item);
    }
}

/* Returns current alignment type, set in configuration file or
 * align by center as a default type. */
static int
get_align_type(void) {
    
    int type = 0;
    int align = deadbeef->conf_get_int(CONF_LYRICS_ALIGNMENT, 1);
    
    switch(align) {
    case 1: type = GTK_JUSTIFY_LEFT; 
        break;
    case 2: type = GTK_JUSTIFY_CENTER; 
        break;
    case 3: type = GTK_JUSTIFY_RIGHT; 
        break;
    default:
        type = GTK_JUSTIFY_CENTER;
    }
    return type;
} 

/* Creates delete cache button. */
static void
create_dlt_btn(void) {
    
    dlt_toggle = gtk_button_new();
    gtk_widget_set_tooltip_text(dlt_toggle, "Remove current cache files");
    
    GtkWidget *dlt_img = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_SMALL_TOOLBAR); 
    gtk_button_set_image(GTK_BUTTON(dlt_toggle), dlt_img);
    
    g_signal_connect(dlt_toggle, "clicked", G_CALLBACK(delete_cache_clicked), NULL);
} 

/* Creates "Biography" tab. Should be called after the "Lyrics" 
 * tab was created. */
static void
create_bio_tab(void) {
    
    /* Adding "Biography" toggle to the same group as the "Lyrics" toggle. */
    bio_toggle = gtk_radio_button_new_with_label_from_widget(
            GTK_RADIO_BUTTON(lyr_toggle), "Biography");
    
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(bio_toggle), FALSE);
    
    GtkWidget *bio_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bio_scroll), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *bio_view = gtk_text_view_new();
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(bio_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(bio_view), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(bio_view), FALSE);
    gtk_widget_set_can_focus(bio_view, FALSE);
    
    bio_tab = gtk_vpaned_new();
    bio_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bio_view));
    
    GtkWidget *img_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(img_frame), GTK_SHADOW_IN);
    
    bio_image = gtk_drawing_area_new();
    gtk_widget_set_app_paintable(bio_image, TRUE);
    
    gtk_container_add(GTK_CONTAINER(bio_scroll), bio_view);
    gtk_container_add(GTK_CONTAINER(img_frame), bio_image);
    
    gtk_paned_pack1(GTK_PANED(bio_tab), img_frame, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(bio_tab), bio_scroll, TRUE, TRUE);
    
    g_signal_connect(bio_image, "expose-event", G_CALLBACK(bio_image_expose), NULL);
    g_signal_connect(bio_toggle, "toggled", G_CALLBACK(infobar_tab_changed), bio_tab);
    
    gint handle_width = 0;
    gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
    
    int height = deadbeef->conf_get_int(CONF_BIO_IMAGE_HEIGHT, 200);
    gtk_widget_set_size_request(img_frame, -1, height + handle_width);
}

/* Creates "Lyrics" tab. Should be created before the "Biography" tab. */
static void
create_lyrics_tab(void) {
    
    lyr_toggle = gtk_radio_button_new_with_label(NULL, "Lyrics");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(lyr_toggle), FALSE);
    
    lyr_tab = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lyr_tab), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *lyr_view = gtk_text_view_new();
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(lyr_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(lyr_view), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(lyr_view), FALSE);
    gtk_widget_set_can_focus(lyr_view, FALSE);
    
    gtk_container_add(GTK_CONTAINER(lyr_tab), lyr_view);
    
    int type = get_align_type();
    gtk_text_view_set_justification(GTK_TEXT_VIEW(lyr_view), type);
    
    lyr_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lyr_view));
    
    gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), 
            "bold", "weight", PANGO_WEIGHT_BOLD, NULL); 
             
    gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), 
            "large", "scale", PANGO_SCALE_LARGE, NULL);  
                
    gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(lyr_buffer), 
            "italic", "style", PANGO_STYLE_ITALIC, NULL);
            
    g_signal_connect(lyr_toggle, "toggled", G_CALLBACK(infobar_tab_changed), lyr_tab);
}

/* Creates infobar with all available tabs. */
static void
create_infobar(void) {
    
    infobar = gtk_vbox_new(FALSE, 0);
    infobar_tabs = gtk_notebook_new();
    infobar_toggles = gtk_hbox_new(FALSE, 0);
    
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(infobar_tabs), FALSE);

    create_lyrics_tab();
    create_bio_tab();
    create_dlt_btn();

    gtk_box_pack_start(GTK_BOX(infobar_toggles), lyr_toggle, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(infobar_toggles), bio_toggle, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(infobar_toggles), dlt_toggle, FALSE, FALSE, 1);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), lyr_tab, NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), bio_tab, NULL);

    gtk_box_pack_start(GTK_BOX(infobar), infobar_toggles, FALSE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(infobar), infobar_tabs, TRUE, TRUE, 1);

    gtk_widget_show_all(infobar);
}

/* Initializes reference to gtkui plug-in. Should be called on
 * plug-in startup. */
void init_ui_plugin(ddb_gtkui_t *ui_plugin) {
    gtkui_plugin = ui_plugin;
}

/* Disposes reference to gtkui plug-in. Should be called on 
 * plug-in shutdown. */
void free_ui_plugin(void) {
    gtkui_plugin = NULL;
}

/* Disposes pixbuf allocated for biography image. */
void free_bio_pixbuf(void) {
    
    if (bio_pixbuf) {
        g_object_unref(bio_pixbuf);
        bio_pixbuf = NULL; 
    }
}

/* Creates infobar and embeds it into deadbeef's interface (at that moment 
 * deadbeef doesn't have an api, which we can use to easily attach infobar, 
 * so we have to manually rearrange its interface). */
void create_infobar_interface(void) {

    create_infobar();

    GtkWidget *ddb_main = lookup_widget(gtkui_plugin->get_mainwin(), "vbox1");
    GtkWidget *ddb_tabs= lookup_widget(gtkui_plugin->get_mainwin(), "tabstrip");
    GtkWidget *ddb_playlist = lookup_widget(gtkui_plugin->get_mainwin(), "frame1");

    g_object_ref(ddb_tabs);
    g_object_ref(ddb_playlist);

    GtkWidget *playlist_box = gtk_vbox_new(FALSE, 0);

    gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_tabs);
    gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_playlist);

    gtk_box_pack_start(GTK_BOX(playlist_box), ddb_tabs, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(playlist_box), ddb_playlist, TRUE, TRUE, 0);

    g_object_unref(ddb_tabs);
    g_object_unref(ddb_playlist);

    GtkWidget *ddb_main_new = gtk_hpaned_new();

    gtk_paned_pack1(GTK_PANED(ddb_main_new), playlist_box, TRUE, TRUE);
    gtk_paned_pack2(GTK_PANED(ddb_main_new), infobar, FALSE, TRUE);

    gtk_container_add(GTK_CONTAINER(ddb_main), ddb_main_new);
    gtk_box_reorder_child(GTK_BOX(ddb_main), ddb_main_new, 2);
    
    gint handle_width;
    gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
    
    int width = deadbeef->conf_get_int(CONF_INFOBAR_WIDTH, 250);
    gtk_widget_set_size_request(infobar, width + handle_width, -1);
    
    gtk_widget_show(ddb_main_new);
    gtk_widget_show(playlist_box);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar);
}

/* Inserts "Infobar" check box inside "View" menu. */
void attach_infobar_menu_entry(void) {
    
    GtkWidget *menu_item = gtk_check_menu_item_new_with_mnemonic ("_Infobar");
    GtkWidget *view_menu = lookup_widget(gtkui_plugin->get_mainwin(), "View_menu");

    gtk_container_add(GTK_CONTAINER(view_menu), menu_item);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), state);

    g_signal_connect(menu_item, "activate", G_CALLBACK(infobar_menu_toggle), NULL);
    gtk_widget_show(menu_item);
}

/* Updates "Lyrics" tab with the new lyrics. */
gboolean update_lyrics_view(gpointer data) {
    
    LyricsViewData *lyr_data = (LyricsViewData*) data;

    if(lyr_buffer) {
        
        GtkTextIter begin, end;
        
        gtk_text_buffer_get_iter_at_line (lyr_buffer, &begin, 0);
        gtk_text_buffer_get_end_iter (lyr_buffer, &end);
        gtk_text_buffer_delete (lyr_buffer, &begin, &end);
        
        /* Setting "bold" style for the song title. */
        gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyr_buffer), 
                &begin, title, -1, "bold", "large", NULL);
                
        gtk_text_buffer_insert(lyr_buffer, &begin, "\n", -1);
        
        /* Setting "italic" style for the artist name. */
        gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(lyr_buffer),
                &begin, artist, -1, "italic", NULL);
                
        gtk_text_buffer_insert(lyr_buffer, &begin, "\n\n", -1);

        if(lyr_data->txt && lyr_data->len > 0) {
            
            gtk_text_buffer_insert(lyr_buffer, &begin, 
                lyr_data->txt, lyr_data->len);
        } else {
            
            gtk_text_buffer_insert(lyr_buffer, &begin, 
                "Lyrics not found.", -1);
        }
    }
    if(lyr_data->txt) free(lyr_data->txt);
    if(lyr_data) free(lyr_data);
    
    return FALSE;
}

/* Updates "Biography" tab with the new artist's image and biography text. */
gboolean update_bio_view(gpointer data) {
    
    BioViewData *bio_data = (BioViewData*) data;

    /* Drawing artist's image. */
    if(bio_image) {
        
        /* Previous image has to be disposed (if exists). */
        free_bio_pixbuf();
        
        bio_pixbuf = gdk_pixbuf_new_from_file(bio_data->img, NULL);
        gtk_widget_queue_draw(bio_image);
    }

    /* Updating biography text. */
    if(bio_buffer) {
        
        GtkTextIter begin, end;
        
        gtk_text_buffer_get_iter_at_line (bio_buffer, &begin, 0);
        gtk_text_buffer_get_end_iter (bio_buffer, &end);
        gtk_text_buffer_delete (bio_buffer, &begin, &end);

        if(bio_data->txt && bio_data->len > 0) {
            
            gtk_text_buffer_insert(bio_buffer, &begin, 
                bio_data->txt, bio_data->len);
        } else {
            
            gtk_text_buffer_insert(bio_buffer, &begin,
                "Biography not found.", -1);
        }
    }
    if(bio_data->txt) free(bio_data->txt);
    if(bio_data->img) free(bio_data->img);
    if(bio_data) free(bio_data);
        
    return FALSE;
}

/* This function should be invoked, when some changes to the plug-in's 
 * configuration were made. It updates infobar view according to the 
 * new changes. */
gboolean infobar_config_changed(void) {

    gboolean state = FALSE;
    
    /* Showing/hiding "Lyrics" tab. */
    state = deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1);
    if(lyr_toggle && lyr_tab) {
        set_tab_visible(lyr_toggle, lyr_tab, state);
    }
    
    /* Showing/hiding "Biography" tab. */
    state = deadbeef->conf_get_int(CONF_BIO_ENABLED, 1);
    if(bio_toggle && bio_tab) {
        set_tab_visible(bio_toggle, bio_tab, state);
    }
    return FALSE;
}
