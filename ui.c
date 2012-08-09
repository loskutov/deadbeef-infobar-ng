/*
    Infobar plugin for DeaDBeeF music player
    Copyright (C) 2011-2012 Dmitriy Simbiriatin <dmitriy.simbiriatin@gmail.com>

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

#include "ui.h"

static GtkWidget *infobar;
static GtkWidget *infobar_tabs;
static GtkWidget *infobar_toggles;

static GtkWidget *lyr_toggle;
static GtkWidget *bio_toggle;
static GtkWidget *sim_toggle;
static GtkWidget *dlt_toggle;

static GtkWidget *lyr_tab;
static GtkWidget *bio_tab;
static GtkWidget *sim_tab;

static GtkWidget *lyr_view;
static GtkWidget *img_frame;
static GtkWidget *bio_image;
static GtkWidget *sim_list;
static GdkPixbuf *bio_pixbuf;

#if GTK_CHECK_VERSION(3, 0, 0)
static GtkWidget *ddb_main_new;
#endif

static GtkTextBuffer *lyr_buffer;
static GtkTextBuffer *bio_buffer;

static ddb_gtkui_t *gtkui_plugin;

/* Called when user checks "Infobar" item in "View" menu. */
static gboolean
infobar_menu_toggle(GtkMenuItem *item, gpointer data) {
    
    gboolean state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
    state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar); 
    deadbeef->conf_set_int(CONF_INFOBAR_VISIBLE, (int) state);
    return FALSE;
}

/* Called when user switches the infobar tabs. When the toggle was clicked,
 * appropriate tab should be selected. */
static gboolean
infobar_tab_changed(GtkToggleButton *toggle, GtkWidget *widget) {
    
    int index = gtk_notebook_page_num(GTK_NOTEBOOK(infobar_tabs), widget);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(infobar_tabs), index);
    return FALSE;
}

/* Utility method which is used to get pointer to widget using its name. */
static GtkWidget*
lookup_widget(GtkWidget *widget, const gchar *widget_name) {
    
    GtkWidget *parent = NULL;
    GtkWidget *found_widget = NULL;

    for (;;) {
        
        if (GTK_IS_MENU(widget)) {
            parent = gtk_menu_get_attach_widget(GTK_MENU (widget));
        } else {
#if GTK_CHECK_VERSION(3, 0, 0)
            parent = gtk_widget_get_parent(widget);
#else
            parent = widget->parent;
#endif
        }
        if (!parent)
            parent = (GtkWidget*) g_object_get_data(G_OBJECT (widget), "GladeParentKey");
            
        if (parent == NULL)
            break;
            
        widget = parent;
    }
    found_widget = (GtkWidget*) g_object_get_data(G_OBJECT (widget), widget_name);
    
    if (!found_widget)
        g_warning ("Widget not found: %s", widget_name);
        
    return found_widget;
}

/* Called when user resizes artist's image on "Biography" tab to redraw
 * the image with the new size. */
static gboolean
#if GTK_CHECK_VERSION(3, 0, 0)
bio_image_expose(GtkWidget *image, cairo_t *ctx, gpointer data) {
#else
bio_image_expose(GtkWidget *image, GdkEventExpose *event, gpointer data) {
#endif
    
    if (bio_pixbuf) {
        
        float ww = gdk_pixbuf_get_width(bio_pixbuf);
        float wh = gdk_pixbuf_get_height(bio_pixbuf);
#if GTK_CHECK_VERSION(3, 0, 0)
        float aw = gtk_widget_get_allocated_width(image);
        float ah = gtk_widget_get_allocated_height(image);
#else
        float aw = image->allocation.width;
        float ah = image->allocation.height;
#endif
        /* This is a workaround which prevents application
         * hanging when we make infobar width == 0 */
        if (aw < 10) aw = 10;
        if (ah < 10) ah = 10;
    
        Res new_res = {0};
        find_new_resolution(ww, wh, aw, ah, &new_res);
        
        int pos_x = (aw - new_res.width) / 2;
        int pos_y = (ah - new_res.height) / 2;
        
        GdkPixbuf *sld = gdk_pixbuf_scale_simple(bio_pixbuf, new_res.width,
                new_res.height, GDK_INTERP_BILINEAR);
                
        if (sld) {
#if GTK_CHECK_VERSION(3, 0, 0)
            GdkWindow *window = gtk_widget_get_window(image);
            cairo_t *cr = gdk_cairo_create(window);
#else
            cairo_t *cr = gdk_cairo_create(image->window);
#endif
            if (cr) {
                gdk_cairo_set_source_pixbuf(cr, sld, pos_x, pos_y);
                cairo_paint(cr);
                cairo_destroy(cr);
            }
            g_object_unref(sld);
        }
    }
    return FALSE;
}

/* Disables any keyboard events in the "Similar" list. */
static gboolean
sim_list_dis_key(GtkWidget *widget, GdkEvent *event, gpointer data) {
    return TRUE;
}

/* Called when user double-clicks on similar list row. */
static gboolean
sim_list_row_active(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer data) {
    
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(sim_list));
    if (model) {
        GtkTreeIter it = {0};
        if (gtk_tree_model_get_iter(model, &it, path)) {

            GValue value = {0};
            gtk_tree_model_get_value(model, &it, URL, &value);
            
            if (G_IS_VALUE(&value) && G_VALUE_HOLDS_STRING(&value)) {
                
                const char *url = g_value_get_string(&value);
                if (url) {
                    char *cmd = NULL;
                    if (asprintf(&cmd, "xdg-open http://%s", url) != -1) {
                        system(cmd);
                        free(cmd);
                    }
                }
                g_value_unset(&value);
            }
        }
    }
    return FALSE;
}

/* Called when user clicks on delete cache button. */
static void
delete_cache_clicked(void) {
    
    GtkWidget *main_wnd = gtkui_plugin->get_mainwin();
    GtkWidget *dlt_dlg = gtk_message_dialog_new(GTK_WINDOW(main_wnd), 
            GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, 
            "Cache files for the current track will be removed. Continue?");
    
    int choise = gtk_dialog_run(GTK_DIALOG(dlt_dlg));
    switch(choise) {
    case GTK_RESPONSE_YES:
    {
        DB_playItem_t *track = deadbeef->streamer_get_playing_track();
        if (track) {

            char *artist = NULL, *title = NULL;
            
            if (get_artist_and_title_info(track, &artist, &title) == 0) {
                del_lyr_cache(artist, title);
                del_bio_cache(artist);
                
                free(artist);
                free(title);
            }
            deadbeef->pl_item_unref(track);
        }
        break;
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
    
    if (visible) {
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
    int align = deadbeef->conf_get_int(CONF_LYRICS_ALIGNMENT, 0);
    
    switch (align) {
    case 0: type = GTK_JUSTIFY_LEFT; 
        break;
    case 1: type = GTK_JUSTIFY_CENTER; 
        break;
    case 2: type = GTK_JUSTIFY_RIGHT; 
        break;
    default:
        break;
    }
    return type;
} 

/* Creates delete cache button. */
static void
create_dlt_btn(void) {
    
    dlt_toggle = gtk_button_new();
    gtk_widget_set_tooltip_text(dlt_toggle, "Remove cache of current track");
    
    GtkWidget *dlt_img = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_SMALL_TOOLBAR); 
    gtk_button_set_image(GTK_BUTTON(dlt_toggle), dlt_img);
    
    g_signal_connect(dlt_toggle, "clicked", G_CALLBACK(delete_cache_clicked), NULL);
}

/* Creates "Similar" tab. Should be called after the "Biography" tab
 * was created. */
static void
create_sim_tab(void) {
    
    sim_toggle = gtk_radio_button_new_with_label_from_widget(
            GTK_RADIO_BUTTON(bio_toggle), "Similar");
    
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(sim_toggle), FALSE);
    
    sim_tab = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sim_tab),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkListStore *sim_store = gtk_list_store_new(3, G_TYPE_STRING, 
            G_TYPE_STRING, G_TYPE_STRING);
    
    sim_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sim_store));
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(sim_list), GTK_TREE_VIEW_GRID_LINES_BOTH);
    
    GtkCellRenderer *name_renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(name_renderer), "style", PANGO_STYLE_OBLIQUE, NULL);

    GtkCellRenderer *match_renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(match_renderer), "weight", PANGO_WEIGHT_BOLD, NULL);
    
    GtkTreeViewColumn *name_column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(name_column, TRUE);
    gtk_tree_view_column_set_title(name_column, "Artist name");
    gtk_tree_view_column_pack_start(name_column, name_renderer, TRUE);
    gtk_tree_view_column_add_attribute(name_column, name_renderer, "text", NAME);
    
    GtkTreeViewColumn *match_column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(match_column, FALSE);
    gtk_tree_view_column_set_title(match_column, "Match");
    gtk_tree_view_column_pack_start(match_column, match_renderer, TRUE);
    gtk_tree_view_column_add_attribute(match_column, match_renderer, "text", MATCH);
    
    gtk_tree_view_append_column(GTK_TREE_VIEW(sim_list), name_column);
    gtk_tree_view_append_column(GTK_TREE_VIEW(sim_list), match_column);
    gtk_container_add(GTK_CONTAINER(sim_tab), sim_list);

    g_signal_connect(sim_toggle, "toggled", G_CALLBACK(infobar_tab_changed), sim_tab);
    g_signal_connect(sim_list, "key-press-event", G_CALLBACK(sim_list_dis_key), NULL);
    g_signal_connect(sim_list, "row-activated", G_CALLBACK(sim_list_row_active), NULL);
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
#if GTK_CHECK_VERSION(3, 0, 0)
    bio_tab = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
#else
    bio_tab = gtk_vpaned_new();
#endif
    bio_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bio_view));
    
    img_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(img_frame), GTK_SHADOW_IN);

    bio_image = gtk_drawing_area_new();
    gtk_widget_set_app_paintable(bio_image, TRUE);
    
    gtk_container_add(GTK_CONTAINER(bio_scroll), bio_view);
    gtk_container_add(GTK_CONTAINER(img_frame), bio_image);
    
    gtk_paned_pack1(GTK_PANED(bio_tab), img_frame, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(bio_tab), bio_scroll, TRUE, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
    g_signal_connect(bio_image, "draw", G_CALLBACK(bio_image_expose), NULL);
#else
    g_signal_connect(bio_image, "expose-event", G_CALLBACK(bio_image_expose), NULL);
#endif
    g_signal_connect(bio_toggle, "toggled", G_CALLBACK(infobar_tab_changed), bio_tab);
    
    int handle_width = 0;
    gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
    
    int height = deadbeef->conf_get_int(CONF_BIO_IMAGE_HEIGHT, 220);
#if GTK_CHECK_VERSION(3, 0, 0)
    gtk_paned_set_position(GTK_PANED(bio_tab), height + handle_width);
#else
    gtk_widget_set_size_request(img_frame, -1, height + handle_width);
#endif
}

/* Creates "Lyrics" tab. Should be created before the "Biography" tab. */
static void
create_lyr_tab(void) {
    
    lyr_toggle = gtk_radio_button_new_with_label(NULL, "Lyrics");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(lyr_toggle), FALSE);
    
    lyr_tab = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lyr_tab), 
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    lyr_view = gtk_text_view_new();
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

#if GTK_CHECK_VERSION(3, 0, 0)
    infobar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    infobar = gtk_vbox_new(FALSE, 0);
#endif
    infobar_tabs = gtk_notebook_new();
#if GTK_CHECK_VERSION(3, 0, 0)
    infobar_toggles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
    infobar_toggles = gtk_hbox_new(FALSE, 0);
#endif
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(infobar_tabs), FALSE);

    create_lyr_tab();
    create_bio_tab();
    create_sim_tab();
    create_dlt_btn();

    gtk_box_pack_start(GTK_BOX(infobar_toggles), lyr_toggle, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(infobar_toggles), bio_toggle, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(infobar_toggles), sim_toggle, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(infobar_toggles), dlt_toggle, FALSE, FALSE, 1);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), lyr_tab, NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), bio_tab, NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(infobar_tabs), sim_tab, NULL);

    gtk_box_pack_start(GTK_BOX(infobar), infobar_toggles, FALSE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(infobar), infobar_tabs, TRUE, TRUE, 1);

    gtk_widget_show_all(infobar);
}

/* Creates infobar and embeds it into deadbeef's interface (at that moment 
 * deadbeef doesn't have an api, which we can use to easily attach infobar, 
 * so we have to manually rearrange its interface). */
static void 
create_infobar_interface(void) {

    create_infobar();

    GtkWidget *ddb_main = lookup_widget(gtkui_plugin->get_mainwin(), "vbox1");
    GtkWidget *ddb_tabs= lookup_widget(gtkui_plugin->get_mainwin(), "tabstrip");
    GtkWidget *ddb_playlist = lookup_widget(gtkui_plugin->get_mainwin(), "frame1");
    if (!ddb_playlist) {
        // deadbeef 0.5.6 support
        ddb_playlist = lookup_widget(gtkui_plugin->get_mainwin(), "playlist");
    }
    g_object_ref(ddb_tabs);
    g_object_ref(ddb_playlist);
    
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkWidget *playlist_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    GtkWidget *playlist_box = gtk_vbox_new(FALSE, 0);
#endif
    gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_tabs);
    gtk_container_remove(GTK_CONTAINER(ddb_main), ddb_playlist);

    gtk_box_pack_start(GTK_BOX(playlist_box), ddb_tabs, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(playlist_box), ddb_playlist, TRUE, TRUE, 0);

    g_object_unref(ddb_tabs);
    g_object_unref(ddb_playlist);
#if GTK_CHECK_VERSION(3, 0, 0)
    ddb_main_new = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
#else
    GtkWidget *ddb_main_new = gtk_hpaned_new();
#endif
    gtk_paned_pack1(GTK_PANED(ddb_main_new), playlist_box, TRUE, TRUE);
    gtk_paned_pack2(GTK_PANED(ddb_main_new), infobar, FALSE, TRUE);

    gtk_container_add(GTK_CONTAINER(ddb_main), ddb_main_new);
    gtk_box_reorder_child(GTK_BOX(ddb_main), ddb_main_new, 2);
    
    int handle_width = 0;
    gtk_widget_style_get(bio_tab, "handle-size", &handle_width, NULL);
    
#if GTK_CHECK_VERSION(3, 0, 0)
    int pos = deadbeef->conf_get_int(CONF_INFOBAR_SEPARATOR_POS, -1);
    if (pos > 0) {
        gtk_paned_set_position(GTK_PANED(ddb_main_new), pos + handle_width);
    }
#else
    int width = deadbeef->conf_get_int(CONF_INFOBAR_WIDTH, 250);
    gtk_widget_set_size_request(infobar, width + handle_width, -1);
#endif
    gtk_widget_show(ddb_main_new);
    gtk_widget_show(playlist_box);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    state ? gtk_widget_show(infobar) : gtk_widget_hide(infobar);
}

/* Inserts "Infobar" check box inside "View" menu. */
static void 
attach_infobar_menu_entry(void) {
    
    GtkWidget *menu_item = gtk_check_menu_item_new_with_mnemonic ("_Infobar");
    GtkWidget *view_menu = lookup_widget(gtkui_plugin->get_mainwin(), "View_menu");

    gtk_container_add(GTK_CONTAINER(view_menu), menu_item);

    gboolean state = deadbeef->conf_get_int(CONF_INFOBAR_VISIBLE, 0);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), state);

    g_signal_connect(menu_item, "activate", G_CALLBACK(infobar_menu_toggle), NULL);
    gtk_widget_show(menu_item);
}

/* Disposes pixbuf allocated for biography image. */
static void 
free_bio_pixbuf(void) {
    
    if (bio_pixbuf) {
        g_object_unref(bio_pixbuf);
        bio_pixbuf = NULL; 
    }
}

/* Initializes reference to gtkui plug-in and creates infobar interface. 
 * Should be called on plug-in startup. */
int init_ui_plugin(void) {
    
#if GTK_CHECK_VERSION(3, 0, 0)
    ddb_gtkui_t* ui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id("gtkui3");
#else
    ddb_gtkui_t* ui_plugin = (ddb_gtkui_t*) deadbeef->plug_get_for_id("gtkui");
#endif
    if (!ui_plugin)
        return -1;
    
    gtkui_plugin = ui_plugin;
        
    create_infobar_interface();
    attach_infobar_menu_entry();
    infobar_config_changed();
    return 0;
}

/* Disposes reference to gtkui plug-in and saves ui settings. 
 * Should be called on plug-in shutdown. */
void free_ui_plugin(void) {
    
    free_bio_pixbuf();
    
    if (gtkui_plugin) {
#if GTK_CHECK_VERSION(3, 0, 0)
        int pos = gtk_paned_get_position(GTK_PANED(ddb_main_new));
        int height = gtk_paned_get_position(GTK_PANED(bio_tab));
        
        deadbeef->conf_set_int(CONF_BIO_IMAGE_HEIGHT, height);
        deadbeef->conf_set_int(CONF_INFOBAR_SEPARATOR_POS, pos);
#else
        int aw = infobar->allocation.width;
        int ah = img_frame->allocation.height;
        
        deadbeef->conf_set_int(CONF_INFOBAR_WIDTH, aw);
        deadbeef->conf_set_int(CONF_BIO_IMAGE_HEIGHT, ah);
#endif
    }
    gtkui_plugin = NULL;
}

/* Updates "Lyrics" tab with the new lyrics. */
void update_lyrics_view(const char *lyr_txt, DB_playItem_t *track) {
    
    char *artist = NULL, *title = NULL;
    
    if (lyr_buffer && get_artist_and_title_info(track, &artist, &title) == 0) {
        
        GtkTextIter begin = {0}, end = {0};
        
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
        
        const char *txt = lyr_txt ? lyr_txt : "Lyrics not found.";
        gtk_text_buffer_insert(lyr_buffer, &begin, txt, strlen(txt));

        free(artist);
        free(title);
    }
}

/* Updates "Biography" tab with the new artist's image and biography text. */
void update_bio_view(const char *bio_txt, const char *img_file) {

    /* Drawing artist's image. */
    if (bio_image) {
        /* Previous image has to be disposed (if exists). */
        free_bio_pixbuf();
        
        if (img_file) {
            bio_pixbuf = gdk_pixbuf_new_from_file(img_file, NULL);
        }
        gtk_widget_queue_draw(bio_image);
    }
    
    /* Updating biography text. */
    if (bio_buffer) {
        
        GtkTextIter begin = {0}, end = {0};
        
        gtk_text_buffer_get_iter_at_line (bio_buffer, &begin, 0);
        gtk_text_buffer_get_end_iter (bio_buffer, &end);
        gtk_text_buffer_delete (bio_buffer, &begin, &end);
        
        const char *txt = bio_txt ? bio_txt : "Biography not found.";
        gtk_text_buffer_insert(bio_buffer, &begin, txt, strlen(txt));
    }
}

/* Updates "Similar" tab with the new list of similar artists. */
void update_similar_view(SimilarInfo *similar, size_t size) {
    
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(sim_list));
    GtkListStore *store = GTK_LIST_STORE(model);
    if (store) {
        /* Removing previous list from model. */
        gtk_list_store_clear(store);
        GtkTreeIter it = {0};
        
        if (similar) {
            for (size_t i = 0; i < size; ++i) {
                gtk_list_store_append(store, &it);
                
                if (similar[i].name) {
                    gtk_list_store_set(store, &it, NAME, similar[i].name, -1);
                }
                if (similar[i].match) {
                    /* Converting match value to percentage representation. */
                    char perc[10] = {0};
                    if (string_to_perc(similar[i].match, perc) != -1) {
                        gtk_list_store_set(store, &it, MATCH, perc, -1);
                    }
                }
                if (similar[i].url) {
                    gtk_list_store_set(store, &it, URL, similar[i].url, -1);
                }
            }
        } else {
            gtk_list_store_append(store, &it);
            gtk_list_store_set(store, &it, NAME, "Similar artists not found.", -1);
        }
    }
}

/* This function should be invoked, when some changes to the plug-in's 
 * configuration were made. It updates infobar view according to the 
 * new changes. */
void infobar_config_changed(void) {
    
    gboolean state = FALSE;
    
    /* Showing/hiding "Lyrics" tab. */
    state = deadbeef->conf_get_int(CONF_LYRICS_ENABLED, 1);
    if (lyr_toggle && lyr_tab) {
        set_tab_visible(lyr_toggle, lyr_tab, state);
    }
    
    /* Showing/hiding "Biography" tab. */
    state = deadbeef->conf_get_int(CONF_BIO_ENABLED, 1);
    if (bio_toggle && bio_tab) {
        set_tab_visible(bio_toggle, bio_tab, state);
    }
    
    /* Showing/hiding "Similar" tab. */
    state = deadbeef->conf_get_int(CONF_SIM_ENABLED, 1);
    if (sim_toggle && sim_tab) {
        set_tab_visible(sim_toggle, sim_tab, state);
    }
    
    /* Updating lyrics alignment. */
    int type = get_align_type();
    gtk_text_view_set_justification(GTK_TEXT_VIEW(lyr_view), type);
}
