#include <gnome.h>
#include "../common/xchat.h"
#include "../common/xchatc.h"
#include "../common/servlist.h"
#include "../common/fe.h"
#include "gui.h"
#include "navigation_tree.h"
#include "textgui.h"

int fe_args(int argc, char *argv[]) {
	if(argc > 1) {
		if(!strcasecmp(argv[1], "--version") || !strcasecmp(argv[1], "-v")) {
			puts(VERSION);
		return 0;
		}
	}
	gnome_program_init("xchat test", "0.1", LIBGNOMEUI_MODULE, argc, argv, NULL);
	return 1;
}

void fe_init(void) {
	strcpy(prefs.nick1, "flobidob");
	servlist_init();
	initialize_gui();
}

void fe_main(void) {
	gtk_main();
	/* sleep for 3 seconds so any QUIT messages are not lost. The  */
	/* GUI is closed at this point, so the user doesn't even know! */
	/* FIXME: pref this? */
	sleep(3);
}

void fe_cleanup(void) {
	/* FIXME: implement */
}

void fe_exit(void) {
	gtk_main_quit();
}

int fe_timeout_add(int interval, void *callback, void *userdata) {
	return g_timeout_add(interval, (GSourceFunc) callback, userdata);
}

void fe_timeout_remove(int tag) {
	g_source_remove(tag);
}

void fe_new_window(struct session *sess) {
	navigation_tree_create_new_network_entry(sess);
	text_gui_add_text_buffer(sess);
}

void fe_new_server(struct server *serv) {
	/* FIXME: implement */
}

void fe_add_rawlog(struct server *serv, char *text, int len, int outbound) {
	/* FIXME: implement */
}

void fe_message(char *msg, int wait) {
	g_print("fe_message()\n");
	/* FIXME: implement */
}

int fe_input_add(int sok, int flags, void *func, void *data) {
	int tag, type = 0;
	GIOChannel *channel;

	channel = g_io_channel_unix_new(sok);

	if(flags & FIA_READ)
		type |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if(flags & FIA_WRITE)
		type |= G_IO_OUT | G_IO_ERR;
	if(flags & FIA_EX)
		type |= G_IO_PRI;

	tag = g_io_add_watch(channel, type, (GIOFunc) func, data);
	g_io_channel_unref(channel);

	return tag;
}

void fe_input_remove(int tag) {
	g_source_remove(tag);
}

void fe_idle_add(void *func, void *data) {
	g_idle_add(func, data);
}

void fe_set_topic(struct session *sess, char *topic) {
	g_print("fe_set_topic()\n");
	/* FIXME: implement */
}

void fe_set_hilight(struct session *sess) {
	/* FIXME: implement */
}

void fe_set_tab_color(struct session *sess, int col, int flash) {
	/* FIXME: implement */
}

void fe_update_mode_buttons(struct session *sess, char mode, char sign) {
	/* FIXME: implement */
}

void fe_update_channel_key(struct session *sess) {
	/* FIXME: implement */
}

void fe_update_channel_limit(struct session *sess) {
	/* FIXME: implement */
}

int fe_is_chanwindow(struct server *serv) {
	/* FIXME: implement */
	return 0;
}

void fe_add_chan_list(struct server *serv, char *chan, char *users, char *topic) {
	/* FIXME: implement */
}

void fe_chan_list_end(struct server *serv) {
	/* FIXME: implement */
}

int fe_is_banwindow(struct session *sess) {
	/* FIXME: implement */
	return 0;
}

void fe_add_ban_list(struct session *sess, char *mask, char *who, char *when) {
	/* FIXME: implement */
}

void fe_ban_list_end(struct session *sess) {
	/* FIXME: implement */
}

void fe_notify_update(char *name) {
	/* FIXME: implement */
}

void fe_text_clear(struct session *sess) {
	/* FIXME: implement */
}

void fe_close_window(struct session *sess) {
	/* FIXME: implement */
}

void fe_progressbar_start(struct session *sess) {
	/* FIXME: implement */
}

void fe_progressbar_end(struct server *serv) {
	/* FIXME: implement */
}

void fe_print_text(struct session *sess, char *text) {
	session_gui *tgui = sess->gui;
	text_gui_print(tgui->buffer, text, TRUE);
	/* FIXME: implement */
}

void fe_userlist_insert(struct session *sess, struct User *newuser, int row, int sel) {
	/* FIXME: implement */
}

int fe_userlist_remove(struct session *sess, struct User *user) {
	/* FIXME: implement */
	return 0;
}

void fe_userlist_rehash(struct session *sess, struct User *user) {
	/* FIXME: implement */
}

void fe_userlist_move(struct session *sess, struct User *user, int new_row) {
	/* FIXME: implement */
}

void fe_userlist_numbers(struct session *sess) {
	/* FIXME: implement */
}

void fe_userlist_clear(struct session *sess) {
	/* FIXME: implement */
}

void fe_dcc_add(struct DCC *dcc) {
	/* FIXME: implement */
}

void fe_dcc_update(struct DCC *dcc) {
	/* FIXME: implement */
}

void fe_dcc_remove(struct DCC *dcc) {
	/* FIXME: implement */
}

int fe_dcc_open_recv_win(int passive) {
	/* FIXME: implement */
	return 0;
}

int fe_dcc_open_send_win(int passive) {
	/* FIXME: implement */
	return 0;
}

int fe_dcc_open_chat_win(int passive) {
	/* FIXME: implement */
	return 0;
}

void fe_clear_channel(struct session *sess) {
	/* FIXME: implement */
}

void fe_session_callback(struct session *sess) {
	g_print("fe_session_callback()\n");
	/* FIXME: implement */
}

void fe_server_callback(struct server *serv) {
	g_print("fe_server_callback()\n");
	/* FIXME: implement */
}

void fe_url_add(const char *text) {
	/* FIXME: implement */
}

void fe_pluginlist_update(void) {
	/* FIXME: implement */
}

void fe_buttons_update(struct session *sess) {
	/* FIXME: implement */
}

void fe_dlgbuttons_update(struct session *sess) {
	/* FIXME: implement */
}

void fe_dcc_send_filereq(struct session *sess, char *nick, int maxcps, int passive) {
	/* FIXME: implement */
}

void fe_set_channel(struct session *sess) {
	navigation_tree_set_channel_name(sess);
	/* FIXME: implement */
}

void fe_set_title(struct session *sess) {
	/* FIXME: implement */
}

void fe_set_nonchannel(struct session *sess, int state) {
	g_print("fe_set_nonchannel()\n");
	/* FIXME: implement */
}

void fe_set_nick(struct server *serv, char *newnick) {
	/* FIXME: implement */
}

void fe_ignore_update(int level) {
	/* FIXME: implement */
}

void fe_beep(void) {
	gdk_beep();
}

void fe_lastlog(session *sess, session *lastlog_sess, char *sstr) {
	/* FIXME: implement */
}

void fe_set_lag(server *serv, int lag) {
	/* FIXME: implement */
}

void fe_set_throttle (server *serv) {
	/* FIXME: implement */
}

void fe_set_away(server *serv) {
	/* FIXME: implement */
}

void fe_serverlist_open (session *sess) {
	/* FIXME: implement */
}

void fe_get_str(char *prompt, char *def, void *callback, void *ud) {
	/* FIXME: implement */
}

void fe_get_int(char *prompt, int def, void *callback, void *ud) {
	/* FIXME: implement */
}

void fe_ctrl_gui(session *sess, int action, int arg) {
	/* FIXME: implement */
}
