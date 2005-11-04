/*
 * preferences-page-effects.c - helpers for the effects preferences page
 *
 * Copyright (C) 2005 David Trowbridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <gconf/gconf-client.h>

#include "preferences-page-effects.h"
#include "preferences-dialog.h"

static void
type_changed (GtkToggleButton *button, PreferencesEffectsPage *page)
{
	GConfClient *client;
	gint type;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->background_none))) {
		type = 0;
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->background_image))) {
		type = 1;
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else {
		type = 2;
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), TRUE);
	}

	client = gconf_client_get_default ();
	gconf_client_set_int (client, "/apps/xchat/main_window/background_type", type, NULL);
	g_object_unref (client);
}

static void
image_changed (GtkFileChooser *chooser, PreferencesEffectsPage *page)
{
	GConfClient *client;
	gchar *filename;

	filename = gtk_file_chooser_get_filename (chooser);

	if (filename) {
		client = gconf_client_get_default ();
		gconf_client_set_string (client, "/apps/xchat/main_window/background_image", filename, NULL);
		g_object_unref (client);
		g_free (filename);
	}
}

static void
transparency_changed (GtkRange *range, PreferencesEffectsPage *page)
{
	GConfClient *client;
	gdouble value;

	value = gtk_range_get_value (range);

	client = gconf_client_get_default ();
	gconf_client_set_float (client, "/apps/xchat/main_window/background_transparency", (float) value, NULL);
	g_object_unref (client);
}

static void
gconf_type_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, PreferencesEffectsPage *page)
{
	gint type;

	g_signal_handlers_block_by_func (G_OBJECT (page->background_none), "toggled", type_changed);
	g_signal_handlers_block_by_func (G_OBJECT (page->background_none), "toggled", type_changed);
	g_signal_handlers_block_by_func (G_OBJECT (page->background_none), "toggled", type_changed);
	type = gconf_client_get_int (client, entry->key, NULL);
	if (type == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_none), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else if (type == 1) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_image), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_transparent), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), TRUE);
	}
	g_signal_handlers_unblock_by_func (G_OBJECT (page->background_none), "toggled", type_changed);
	g_signal_handlers_unblock_by_func (G_OBJECT (page->background_image), "toggled", type_changed);
	g_signal_handlers_unblock_by_func (G_OBJECT (page->background_transparent), "toggled", type_changed);
}

static void
gconf_image_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, PreferencesEffectsPage *page)
{
}

static void
gconf_transparency_changed (GConfClient *client, guint cnxn_id, GConfEntry *entry, PreferencesEffectsPage *page)
{
}

PreferencesEffectsPage *
preferences_page_effects_new (gpointer prefs_dialog, GladeXML *xml)
{
	PreferencesEffectsPage *page = g_new0 (PreferencesEffectsPage, 1);
	PreferencesDialog *p = (PreferencesDialog *) prefs_dialog;
	GtkTreeIter iter;
	int type;
	char *filename;
	float transparency;

#define GW(name) ((page->name) = glade_xml_get_widget (xml, #name))
	GW(background_none);
	GW(background_image);
	GW(background_transparent);
	GW(background_image_file);
	GW(background_transparency);
#undef GW

	page->icon = gdk_pixbuf_new_from_file (XCHATSHAREDIR "/effects.png", NULL);
	gtk_list_store_append (p->page_store, &iter);
	gtk_list_store_set (p->page_store, &iter, 0, page->icon, 1, "Effects", 2, 2, -1);

	g_signal_connect (G_OBJECT (page->background_none),         "toggled",           G_CALLBACK (type_changed),         page);
	g_signal_connect (G_OBJECT (page->background_image),        "toggled",           G_CALLBACK (type_changed),         page);
	g_signal_connect (G_OBJECT (page->background_transparent),  "toggled",           G_CALLBACK (type_changed),         page);
	g_signal_connect (G_OBJECT (page->background_image_file),   "selection-changed", G_CALLBACK (image_changed),        page);
	g_signal_connect (G_OBJECT (page->background_transparency), "value-changed",     G_CALLBACK (transparency_changed), page);

	page->nh[0] = gconf_client_notify_add (p->gconf, "/apps/xchat/main_window/background_type",         (GConfClientNotifyFunc) gconf_type_changed,         page, NULL, NULL);
	page->nh[1] = gconf_client_notify_add (p->gconf, "/apps/xchat/main_window/background_image",        (GConfClientNotifyFunc) gconf_image_changed,        page, NULL, NULL);
	page->nh[2] = gconf_client_notify_add (p->gconf, "/apps/xchat/main_window/background_transparency", (GConfClientNotifyFunc) gconf_transparency_changed, page, NULL, NULL);

	type = gconf_client_get_int (p->gconf, "/apps/xchat/main_window/background_type", NULL);
	if (type == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_none), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else if (type == 1) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_image), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), FALSE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->background_transparent), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_image_file),   FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (page->background_transparency), TRUE);
	}

	filename = gconf_client_get_string (p->gconf, "/apps/xchat/main_window/background_image", NULL);
	if (filename) {
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (page->background_image_file), filename);
		g_free (filename);
	}

	transparency = gconf_client_get_float (p->gconf, "/apps/xchat/main_window/background_transparency", NULL);
	gtk_range_set_value (GTK_RANGE (page->background_transparency), (gdouble) transparency);

	return page;
}

void
preferences_page_effects_free (PreferencesEffectsPage *page)
{
	GConfClient *client;
	gint i;

	client = gconf_client_get_default ();
	for (i = 0; i < 3; i++)
		gconf_client_notify_remove (client, page->nh[i]);
	g_object_unref (client);

	gdk_pixbuf_unref (page->icon);
	g_free (page);
}
