/**
 * @file webapp.c Webserver UI module
 *
 * Copyright (C) 2013-2018 studio-link.de Sebastian Reimers
 */
#include <re.h>
#include <baresip.h>
#include <string.h>
#include <stdlib.h>
#include "assets/index_html.h"
#include "assets/js.h"
#include "assets/css.h"
#include "assets/fonts.h"
#include "assets/logo.h"
#include "webapp.h"

#define SLVERSION "SLVERSION_T"

static struct tmr tmr;

static struct http_sock *httpsock = NULL;
enum webapp_call_state webapp_call_status = WS_CALL_OFF;
static char webapp_call_json[150] = {0};
struct odict *webapp_calls = NULL;
//static char command[100] = {0};

static struct aufilt vumeter = {
	LE_INIT, "webapp_vumeter",
	webapp_vu_encode_update, webapp_vu_encode,
	webapp_vu_decode_update, webapp_vu_decode
};

static int http_sreply(struct http_conn *conn, uint16_t scode,
		const char *reason, const char *ctype,
		const char *fmt, size_t size)
{
	struct mbuf *mb;
	int err = 0;
	
	(void)scode;
	(void)reason;
	(void)ctype;
	
	mb = mbuf_alloc(size);
	if (!mb)
		return ENOMEM;

	err = mbuf_write_mem(mb, (const uint8_t *)fmt, size);

	if (err)
		goto out;

	http_reply(conn, 200, "OK",
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%b",
			ctype,
			mb->end,
			mb->buf, mb->end);
out:
	mem_deref(mb);
	return err;
}


static void http_req_handler(struct http_conn *conn,
			     const struct http_msg *msg, void *arg)
{
	const struct network *net = baresip_network();
	(void)arg;
	/*
	 * Dynamic Requests
	 */

	if (0 == pl_strcasecmp(&msg->path, "/ws_baresip")) {
		info("webapp:ws_baresip\n");
		webapp_ws_handler(conn, WS_BARESIP, msg, webapp_ws_baresip);
		ws_send_json(WS_BARESIP, webapp_accounts_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_contacts")) {
		info("webapp:ws_contacts\n");
		webapp_ws_handler(conn, WS_CONTACT, msg, webapp_ws_contacts);
		ws_send_json(WS_CONTACT, webapp_contacts_get());
		return;
	}
	
	if (0 == pl_strcasecmp(&msg->path, "/ws_videosrc")) {	//by aphero 2018-3-19
		info("webapp:ws_videosrc\n");
		webapp_ws_handler(conn, WS_VIDEOSRC, msg, webapp_ws_videosrc);
		ws_send_json(WS_VIDEOSRC, webapp_videosrc_get());
		return;
	}
	
	if (0 == pl_strcasecmp(&msg->path, "/ws_chat")) {
		info("webapp:ws_chat\n");
		webapp_ws_handler(conn, WS_CHAT, msg, webapp_ws_chat);
		ws_send_json(WS_CHAT, webapp_messages_get());
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_meter")) {
		info("webapp:ws_meter\n");
		webapp_ws_handler(conn, WS_METER, msg, webapp_ws_meter);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/ws_calls")) {
		info("webapp:ws_calls\n");
		webapp_ws_handler(conn, WS_CALLS, msg, webapp_ws_calls);//webapp_ws_calls接收处理
		ws_send_json(WS_CALLS, webapp_calls);
		webapp_ws_call_mute_send();
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/status.json")) {
		char ipv4[15];
		char json[255];
		info("webapp:status.json\n");
		sa_ntop(net_laddr_af(net, AF_INET), ipv4, sizeof(ipv4));

		re_snprintf(json, sizeof(json), "[{ \"name\": \"IPv4\", \
				\"value\": \"%s\", \"label\": \"default\"}]",
				ipv4);

		http_sreply(conn, 200, "OK",
				"application/json",
				(const char*)json,
				strlen(json));
		return;
	}

	/*
	 * Static Requests
	 */

	if (0 == pl_strcasecmp(&msg->path, "/")) {
		http_sreply(conn, 200, "OK",
				"text/html",
				(const char*)index_min_html,
				index_min_html_len);

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/version")) {
		http_sreply(conn, 200, "OK",
				"text/html",
				(const char*)SLVERSION,
				sizeof(SLVERSION));

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/swvariant")) {
#ifdef SLPLUGIN
		const char* value = "plugin";
#else
		const char* value = "standalone";
#endif
		http_sreply(conn, 200, "OK",
				"text/html",
				value,
				strlen(value));
		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/js/all.js")) {
		http_sreply(conn, 200, "OK",
				"application/javascript",
				(const char*)js_all_js,
				js_all_js_len);

		return;
	}
	/*
	if (0 == pl_strcasecmp(&msg->path, "/images/logo.svg")) {
		http_sreply(conn, 200, "OK",
				"image/svg+xml",
				(const char*)images_logo_svg,
				images_logo_svg_len);

		return;
	}
	*/ 
	if (0 == pl_strcasecmp(&msg->path, "/images/logo.png")) {
		http_sreply(conn, 200, "OK",
				"image/png",
				(const char*)images_logo_png,
				images_logo_png_len);

		return;
	}
	if (0 == pl_strcasecmp(&msg->path, "/css/all.css")) {
		http_sreply(conn, 200, "OK",
				"text/css",
				(const char*)css_all_css,
				css_all_css_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.eot")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_eot,
			    fonts_glyphicons_halflings_regular_eot_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.svg")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_svg,
			    fonts_glyphicons_halflings_regular_svg_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.ttf")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_ttf,
			    fonts_glyphicons_halflings_regular_ttf_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.woff")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_woff,
			    fonts_glyphicons_halflings_regular_woff_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/glyphicons-halflings-regular.woff2")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_glyphicons_halflings_regular_woff2,
			    fonts_glyphicons_halflings_regular_woff2_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/FontAwesome.otf")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_FontAwesome_otf,
			    fonts_FontAwesome_otf_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/fontawesome-webfont.eot")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_fontawesome_webfont_eot,
			    fonts_fontawesome_webfont_eot_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/fontawesome-webfont.svg")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_fontawesome_webfont_svg,
			    fonts_fontawesome_webfont_svg_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/fontawesome-webfont.ttf")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_fontawesome_webfont_ttf,
			    fonts_fontawesome_webfont_ttf_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/fontawesome-webfont.woff")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_fontawesome_webfont_woff,
			    fonts_fontawesome_webfont_woff_len);
		return;
	}
	if (0 == pl_strcasecmp(&msg->path,
				"/fonts/fontawesome-webfont.woff2")) {
		http_sreply(conn, 200, "OK",
			    "application/octet-stream",
			    (const char*)
			    fonts_fontawesome_webfont_woff2,
			    fonts_fontawesome_webfont_woff2_len);
		return;
	}


	http_ereply(conn, 404, "Not found");
}


static void ua_event_current_set(struct ua *ua)
{
	uag_current_set(ua);
	webapp_account_current();
	ws_send_json(WS_BARESIP, webapp_accounts_get());
}


int webapp_call_delete(struct call *call)
{
	char id[64] = {0};
	int err = 0;

	if (!call)
		return EINVAL;

	re_snprintf(id, sizeof(id), "%x", call);
	odict_entry_del(webapp_calls, id);

	return err;
}

/*
static int json_print_handler(const char *p, size_t size, void *arg)                                
{      
	//info("webapp:json: %s %s\n", p,arg);
	(void)size;
	return re_snprintf(arg, sizeof(arg), "%s%s", p, arg);
}    


static void chat_send_calls(void)
{
	struct re_printf pf;                   
	char msg[1024] = {0};
	pf.vph = json_print_handler;                
	pf.arg = msg;

	json_encode_odict(&pf, webapp_calls);
	webapp_chat_send(msg, NULL);
}
*/

int webapp_call_update(struct call *call, char *state)
{
	struct odict *o;
	char id[64] = {0};
	int err = 0;

	if (!call || !state)
		return EINVAL;


	err = odict_alloc(&o, DICT_BSIZE);
	if (err)
		return ENOMEM;

	re_snprintf(id, sizeof(id), "%x", call);

	odict_entry_del(webapp_calls, id);
	odict_entry_add(o, "peer", ODICT_STRING, call_peeruri(call));
	odict_entry_add(o, "state", ODICT_STRING, state);
	odict_entry_add(webapp_calls, id, ODICT_OBJECT, o);

	//chat_send_calls();//刚呼叫上就发送message，"{{"
	ws_send_json(WS_CALLS, webapp_calls);
	mem_deref(o);
	return err;
}


static void ua_event_handler(struct ua *ua, enum ua_event ev,
		struct call *call, const char *prm, void *arg)
{
	(void)arg;
	switch (ev) {
		case UA_EVENT_CALL_INCOMING:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"INCOMING\",\
					\"peeruri\": \"%s\",\
					\"key\": \"%x\" }",
					call_peeruri(call), call);
					
			webapp_call_update(call, "Incoming");
			//ws_send_all(WS_CALLS, webapp_call_json);
			webapp_call_status = WS_CALL_RINGING;
			ua_answer(uag_current(), call);//自动应答
			break;

		case UA_EVENT_CALL_ESTABLISHED:
			ua_event_current_set(ua);
			webapp_call_update(call, "Established");
			webapp_call_status = WS_CALL_ON;
			break;

		case UA_EVENT_CALL_CLOSED:
			ua_event_current_set(ua);
			re_snprintf(webapp_call_json, sizeof(webapp_call_json),
					"{ \"callback\": \"CLOSED\",\
					\"message\": \"%s\" }", prm);
			webapp_call_delete(call);
			ws_send_all(WS_CALLS, webapp_call_json);
			ws_send_json(WS_CALLS, webapp_calls);
			webapp_call_status = WS_CALL_OFF;
			break;

		case UA_EVENT_REGISTER_OK:
			info("Register OK: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), true);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_UNREGISTERING:
			info("UNREGISTERING: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		case UA_EVENT_REGISTER_FAIL:
			info("Register Fail: %s\n", ua_aor(ua));
			webapp_account_status(ua_aor(ua), false);
			ws_send_json(WS_BARESIP, webapp_accounts_get());
			break;

		default:
			break;
	}
}


static int http_port(void)
{
	char path[256] = "";
	char filename[256] = "";
	struct mbuf *mb;
	struct sa srv;
	//struct sa listen;
	//int port = 0;
	char port_string[10];
	int err = 0;

	mb = mbuf_alloc(20);
	if (!mb)
		return ENOMEM;

	err = conf_path_get(path, sizeof(path));
	if (err)
		goto out;

	if (re_snprintf(filename, sizeof(filename),
				"%s/http_port", path) < 0)
		return ENOMEM;

	err = webapp_load_file(mb, filename);
	//if (err) {
	//	port = 0;
	//}
	//else {
	//	port = atoi((char *)mb->buf);
	//}

	//err = sa_set_str(&srv, "0.0.0.0", port);
	err = sa_set_str(&srv, "0.0.0.0", 9000);
	err |= http_listen(&httpsock, &srv, http_req_handler, NULL);
	if (err)
		goto out;

	info("webapp: http listening on %J\n", &srv);
	webapp_write_file(port_string, filename);

out:
	mem_deref(mb);
	return err;
}

/* //by aphero
static void syscmd(void *arg)
{
#ifndef SLBOX
	system(command);
#endif
}
*/
static int module_init(void)
{
	int err = 0;

	err = odict_alloc(&webapp_calls, DICT_BSIZE);
	if (err)
		goto out;

	err = http_port();
	if (err)
		goto out;
	aufilt_register(baresip_aufiltl(), &vumeter);//音量表

	uag_event_register(ua_event_handler, NULL);
	webapp_ws_init();
	webapp_accounts_init();
	webapp_videosrc_init();//by aphero
	webapp_contacts_init();
	webapp_chat_init();
	webapp_ws_meter_init();
/* //by aphero
	tmr_init(&tmr);
#if defined (SLPLUGIN)
	tmr_start(&tmr, 800, syscmd, NULL);
#endif
*/
out:
	if (err) {
		mem_deref(httpsock);
	}
	return err;
}

static int module_close(void)
{
	tmr_cancel(&tmr);
	uag_event_unregister(ua_event_handler);

	webapp_ws_meter_close();
	webapp_accounts_close();
	webapp_contacts_close();
	webapp_videosrc_close();//by aphero
	webapp_chat_close();
	
	aufilt_unregister(&vumeter);
	
	webapp_ws_close();
	mem_deref(httpsock);
	mem_deref(webapp_calls);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(webapp) = {
	"webapp",
	"application",
	module_init,
	module_close,
};
