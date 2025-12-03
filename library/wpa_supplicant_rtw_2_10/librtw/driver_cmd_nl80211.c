/*
 * Driver interaction with extended Linux CFG8021
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <fcntl.h>

#include "utils/common.h"
#include "driver_nl80211.h"
#include "android_drv.h"

#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE  0x89F0  /* to 89FF */
#endif

#ifndef __unused
#if __GNUC_PREREQ(2, 95) || defined(__INTEL_COMPILER)
#define __unused __attribute__((__unused__))
#else
#define __unused
#endif
#endif

typedef struct wifi_priv_cmd {
	char *bufaddr;
	int used_len;
	int total_len;
} wifi_priv_cmd;

static int drv_errors = 0;

static void wpa_driver_send_hang_msg(struct wpa_driver_nl80211_data *drv)
{
	drv_errors++;
	if (drv_errors > DRV_NUMBER_SEQUENTIAL_ERRORS) {
		drv_errors = 0;
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	}
}

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
	size_t buf_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ifreq ifr;
	wifi_priv_cmd priv_cmd;
	int ret = 0;

	if (1) { /* Use private command */
		os_memcpy(buf, cmd, strlen(cmd) + 1);
		memset(&ifr, 0, sizeof(ifr));
		memset(&priv_cmd, 0, sizeof(priv_cmd));
		os_strlcpy(ifr.ifr_name, bss->ifname, IFNAMSIZ);

		priv_cmd.bufaddr = buf;
		priv_cmd.used_len = buf_len;
		priv_cmd.total_len = buf_len;
		ifr.ifr_data = (void *)&priv_cmd;

		if ((ret = ioctl(drv->global->ioctl_sock, SIOCDEVPRIVATE + 1, &ifr)) < 0) {
			wpa_printf(MSG_ERROR, "%s: failed to issue private command: %s", __func__, cmd);
			wpa_driver_send_hang_msg(drv);
		} else {
			drv_errors = 0;
			ret = 0;
			wpa_printf(MSG_DEBUG, "%s %s len = %d, %zu", __func__, buf, ret, strlen(buf));
		}
	}
	return ret;
}

int wpa_driver_set_p2p_noa(void *priv, u8 count, int start, int duration)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_NOA %d %d %d", count, start, duration);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf) + 1);
}

int wpa_driver_get_p2p_noa(void *priv __unused, u8 *buf __unused, size_t len __unused)
{
	/* Return 0 till we handle p2p_presence request completely in the driver */
	return 0;
}

int wpa_driver_set_p2p_ps(void *priv, int legacy_ps, int opp_ps, int ctwindow)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_PS %d %d %d", legacy_ps, opp_ps, ctwindow);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf) + 1);
}

int wpa_driver_set_ap_wps_p2p_ie(void *priv, const struct wpabuf *beacon,
	const struct wpabuf *proberesp,
	const struct wpabuf *assocresp)
{
	char *buf;
	const struct wpabuf *ap_wps_p2p_ie = NULL;

	char *_cmd = "SET_AP_WPS_P2P_IE";
	char *pbuf;
	int ret = 0;
	int i, buf_len;
	struct cmd_desc {
		int cmd;
		const struct wpabuf *src;
	} cmd_arr[] = {
		{ 0x1, beacon },
		{ 0x2, proberesp },
		{ 0x4, assocresp },
		{ -1, NULL }
	};

	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	for (i = 0; cmd_arr[i].cmd != -1; i++) {
		ap_wps_p2p_ie = cmd_arr[i].src;
		if (ap_wps_p2p_ie) {
			buf_len = strlen(_cmd) + 3 + wpabuf_len(ap_wps_p2p_ie);
			buf = os_zalloc(buf_len);
			if (NULL == buf) {
				wpa_printf(MSG_ERROR, "%s: Out of memory",
					__func__);
				ret = -1;
				break;
			}
		} else {
			continue;
		}
		pbuf = buf;
		pbuf += snprintf(pbuf, buf_len - wpabuf_len(ap_wps_p2p_ie),
			"%s %d", _cmd, cmd_arr[i].cmd);
		*pbuf++ = '\0';
		os_memcpy(pbuf, wpabuf_head(ap_wps_p2p_ie), wpabuf_len(ap_wps_p2p_ie));
		ret = wpa_driver_nl80211_driver_cmd(priv, buf, buf, buf_len);
		os_free(buf);
		if (ret < 0)
			break;
	}

	return ret;
}
