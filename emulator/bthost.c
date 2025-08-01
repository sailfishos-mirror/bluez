// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011-2012  Intel Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright 2023-2024 NXP
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <stdbool.h>

#include "lib/bluetooth.h"

#include "src/shared/util.h"
#include "src/shared/tester.h"
#include "src/shared/queue.h"
#include "src/shared/ad.h"
#include "monitor/bt.h"
#include "monitor/rfcomm.h"
#include "bthost.h"

#define lmp_bredr_capable(bthost)     (!((bthost)->features[4] & 0x20))

/* ACL handle and flags pack/unpack */
#define acl_handle_pack(h, f)	(uint16_t)((h & 0x0fff)|(f << 12))
#define acl_handle(h)		(h & 0x0fff)
#define acl_flags(h)		(h >> 12)

#define sco_flags_status(f)	(f & 0x03)
#define sco_flags_pack(status)	(status & 0x03)

#define iso_flags_pb(f)		(f & 0x0003)
#define iso_flags_ts(f)		((f >> 2) & 0x0001)
#define iso_flags_pack(pb, ts)	(((pb) & 0x03) | (((ts) & 0x01) << 2))
#define iso_data_len_pack(h, f)	((uint16_t) ((h) | ((f) << 14)))

#define L2CAP_FEAT_FIXED_CHAN	0x00000080
#define L2CAP_FC_SIG_BREDR	0x02
#define L2CAP_FC_SMP_BREDR	0x80
#define L2CAP_IT_FEAT_MASK	0x0002
#define L2CAP_IT_FIXED_CHAN	0x0003

/* RFCOMM setters */
#define RFCOMM_ADDR(cr, dlci)	(((dlci & 0x3f) << 2) | (cr << 1) | 0x01)
#define RFCOMM_CTRL(type, pf)	(((type & 0xef) | (pf << 4)))
#define RFCOMM_LEN8(len)	(((len) << 1) | 1)
#define RFCOMM_LEN16(len)	((len) << 1)
#define RFCOMM_MCC_TYPE(cr, type)	(((type << 2) | (cr << 1) | 0x01))

/* RFCOMM FCS calculation */
#define CRC(data) (rfcomm_crc_table[rfcomm_crc_table[0xff ^ data[0]] ^ data[1]])

static const unsigned char rfcomm_crc_table[256] = {
	0x00, 0x91, 0xe3, 0x72, 0x07, 0x96, 0xe4, 0x75,
	0x0e, 0x9f, 0xed, 0x7c, 0x09, 0x98, 0xea, 0x7b,
	0x1c, 0x8d, 0xff, 0x6e, 0x1b, 0x8a, 0xf8, 0x69,
	0x12, 0x83, 0xf1, 0x60, 0x15, 0x84, 0xf6, 0x67,

	0x38, 0xa9, 0xdb, 0x4a, 0x3f, 0xae, 0xdc, 0x4d,
	0x36, 0xa7, 0xd5, 0x44, 0x31, 0xa0, 0xd2, 0x43,
	0x24, 0xb5, 0xc7, 0x56, 0x23, 0xb2, 0xc0, 0x51,
	0x2a, 0xbb, 0xc9, 0x58, 0x2d, 0xbc, 0xce, 0x5f,

	0x70, 0xe1, 0x93, 0x02, 0x77, 0xe6, 0x94, 0x05,
	0x7e, 0xef, 0x9d, 0x0c, 0x79, 0xe8, 0x9a, 0x0b,
	0x6c, 0xfd, 0x8f, 0x1e, 0x6b, 0xfa, 0x88, 0x19,
	0x62, 0xf3, 0x81, 0x10, 0x65, 0xf4, 0x86, 0x17,

	0x48, 0xd9, 0xab, 0x3a, 0x4f, 0xde, 0xac, 0x3d,
	0x46, 0xd7, 0xa5, 0x34, 0x41, 0xd0, 0xa2, 0x33,
	0x54, 0xc5, 0xb7, 0x26, 0x53, 0xc2, 0xb0, 0x21,
	0x5a, 0xcb, 0xb9, 0x28, 0x5d, 0xcc, 0xbe, 0x2f,

	0xe0, 0x71, 0x03, 0x92, 0xe7, 0x76, 0x04, 0x95,
	0xee, 0x7f, 0x0d, 0x9c, 0xe9, 0x78, 0x0a, 0x9b,
	0xfc, 0x6d, 0x1f, 0x8e, 0xfb, 0x6a, 0x18, 0x89,
	0xf2, 0x63, 0x11, 0x80, 0xf5, 0x64, 0x16, 0x87,

	0xd8, 0x49, 0x3b, 0xaa, 0xdf, 0x4e, 0x3c, 0xad,
	0xd6, 0x47, 0x35, 0xa4, 0xd1, 0x40, 0x32, 0xa3,
	0xc4, 0x55, 0x27, 0xb6, 0xc3, 0x52, 0x20, 0xb1,
	0xca, 0x5b, 0x29, 0xb8, 0xcd, 0x5c, 0x2e, 0xbf,

	0x90, 0x01, 0x73, 0xe2, 0x97, 0x06, 0x74, 0xe5,
	0x9e, 0x0f, 0x7d, 0xec, 0x99, 0x08, 0x7a, 0xeb,
	0x8c, 0x1d, 0x6f, 0xfe, 0x8b, 0x1a, 0x68, 0xf9,
	0x82, 0x13, 0x61, 0xf0, 0x85, 0x14, 0x66, 0xf7,

	0xa8, 0x39, 0x4b, 0xda, 0xaf, 0x3e, 0x4c, 0xdd,
	0xa6, 0x37, 0x45, 0xd4, 0xa1, 0x30, 0x42, 0xd3,
	0xb4, 0x25, 0x57, 0xc6, 0xb3, 0x22, 0x50, 0xc1,
	0xba, 0x2b, 0x59, 0xc8, 0xbd, 0x2c, 0x5e, 0xcf
};

static uint8_t rfcomm_fcs2(uint8_t *data)
{
	return 0xff - rfcomm_crc_table[CRC(data) ^ data[2]];
}

static uint8_t rfcomm_fcs(uint8_t *data)
{
	return 0xff - CRC(data);
}

struct cmd {
	struct cmd *next;
	struct cmd *prev;
	uint8_t data[256 + sizeof(struct bt_hci_cmd_hdr)];
	uint16_t len;
};

struct cmd_queue {
	struct cmd *head;
	struct cmd *tail;
};

struct cid_hook {
	uint16_t cid;
	bthost_cid_hook_func_t func;
	void *user_data;
	struct cid_hook *next;
};

struct rfcomm_chan_hook {
	uint8_t channel;
	bthost_rfcomm_chan_hook_func_t func;
	void *user_data;
	struct rfcomm_chan_hook *next;
};

struct sco_hook {
	bthost_sco_hook_func_t func;
	void *user_data;
	bthost_destroy_func_t destroy;
};

struct iso_hook {
	bthost_iso_hook_func_t func;
	void *user_data;
	bthost_destroy_func_t destroy;
};

struct btconn {
	uint16_t handle;
	uint8_t bdaddr[6];
	uint8_t addr_type;
	uint8_t encr_mode;
	uint16_t next_cid;
	uint64_t fixed_chan;
	struct l2conn *l2conns;
	struct rcconn *rcconns;
	struct cid_hook *cid_hooks;
	struct rfcomm_chan_hook *rfcomm_chan_hooks;
	struct sco_hook *sco_hook;
	struct iso_hook *iso_hook;
	struct btconn *next;
	void *smp_data;
	uint16_t recv_len;
	uint16_t data_len;
	void *recv_data;
};

enum l2cap_mode {
	L2CAP_MODE_OTHER,
	L2CAP_MODE_LE_CRED,
	L2CAP_MODE_LE_ENH_CRED,
};

struct l2conn {
	struct l2conn *next;
	uint16_t scid;
	uint16_t dcid;
	uint16_t psm;
	enum l2cap_mode mode;
	uint16_t data_len;
	uint16_t recv_len;
	void *recv_data;
};

struct rcconn {
	uint8_t channel;
	uint16_t scid;
	struct rcconn *next;
};

struct l2cap_pending_req {
	uint8_t ident;
	bthost_l2cap_rsp_cb cb;
	void *user_data;
	struct l2cap_pending_req *next;
};

struct l2cap_conn_cb_data {
	uint16_t psm;
	uint16_t mtu;
	uint16_t mps;
	uint16_t credits;
	bthost_l2cap_connect_cb func;
	bthost_l2cap_disconnect_cb disconn_func;
	void *user_data;
	struct l2cap_conn_cb_data *next;
};

struct rfcomm_conn_cb_data {
	uint8_t channel;
	bthost_rfcomm_connect_cb func;
	void *user_data;
	struct rfcomm_conn_cb_data *next;
};

struct rfcomm_connection_data {
	uint8_t channel;
	struct btconn *conn;
	bthost_rfcomm_connect_cb cb;
	void *user_data;
};

struct le_ext_adv {
	struct bthost *bthost;
	uint16_t event_type;
	uint8_t  addr_type;
	uint8_t  addr[6];
	uint8_t  direct_addr_type;
	uint8_t  direct_addr[6];
};

struct bthost {
	bool ready;
	bthost_ready_cb ready_cb;
	uint8_t bdaddr[6];
	uint8_t features[8];
	bthost_send_func send_handler;
	void *send_data;
	struct cmd_queue cmd_q;
	uint8_t ncmd;
	struct btconn *conns;
	bthost_cmd_complete_cb cmd_complete_cb;
	void *cmd_complete_data;
	bthost_new_conn_cb new_conn_cb;
	void *new_conn_data;
	bthost_new_conn_cb new_sco_cb;
	void *new_sco_data;
	bthost_accept_conn_cb accept_iso_cb;
	bthost_new_conn_cb new_iso_cb;
	void *new_iso_data;
	uint16_t acl_mtu;
	uint16_t iso_mtu;
	struct rfcomm_connection_data *rfcomm_conn_data;
	struct l2cap_conn_cb_data *new_l2cap_conn_data;
	struct rfcomm_conn_cb_data *new_rfcomm_conn_data;
	struct l2cap_pending_req *l2reqs;
	uint8_t pin[16];
	uint8_t pin_len;
	uint8_t io_capability;
	uint8_t auth_req;
	bool reject_user_confirm;
	void *smp_data;
	bool conn_init;
	bool le;
	bool sc;

	struct queue *le_ext_adv;

	bthost_debug_func_t debug_callback;
	bthost_destroy_func_t debug_destroy;
	void *debug_data;
};

struct bthost *bthost_create(void)
{
	struct bthost *bthost;

	bthost = new0(struct bthost, 1);
	if (!bthost)
		return NULL;

	bthost->smp_data = smp_start(bthost);
	if (!bthost->smp_data) {
		free(bthost);
		return NULL;
	}

	bthost->le_ext_adv = queue_new();

	/* Set defaults */
	bthost->io_capability = 0x03;
	bthost->acl_mtu = UINT16_MAX;
	bthost->iso_mtu = UINT16_MAX;

	return bthost;
}

static void l2conn_free(struct l2conn *conn)
{
	free(conn->recv_data);
	free(conn);
}

static void btconn_free(struct btconn *conn)
{
	if (conn->smp_data)
		smp_conn_del(conn->smp_data);

	while (conn->l2conns) {
		struct l2conn *l2conn = conn->l2conns;

		conn->l2conns = l2conn->next;
		l2conn_free(l2conn);
	}

	while (conn->cid_hooks) {
		struct cid_hook *hook = conn->cid_hooks;

		conn->cid_hooks = hook->next;
		free(hook);
	}

	while (conn->rcconns) {
		struct rcconn *rcconn = conn->rcconns;

		conn->rcconns = rcconn->next;
		free(rcconn);
	}

	while (conn->rfcomm_chan_hooks) {
		struct rfcomm_chan_hook *hook = conn->rfcomm_chan_hooks;

		conn->rfcomm_chan_hooks = hook->next;
		free(hook);
	}

	if (conn->sco_hook && conn->sco_hook->destroy)
		conn->sco_hook->destroy(conn->sco_hook->user_data);

	if (conn->iso_hook && conn->iso_hook->destroy)
		conn->iso_hook->destroy(conn->iso_hook->user_data);

	free(conn->sco_hook);
	free(conn->iso_hook);
	free(conn->recv_data);
	free(conn);
}

static struct btconn *bthost_find_conn(struct bthost *bthost, uint16_t handle)
{
	struct btconn *conn;

	for (conn = bthost->conns; conn != NULL; conn = conn->next) {
		if (conn->handle == handle)
			return conn;
	}

	return NULL;
}

static struct btconn *bthost_find_conn_by_bdaddr(struct bthost *bthost,
							const uint8_t *bdaddr)
{
	struct btconn *conn;

	for (conn = bthost->conns; conn != NULL; conn = conn->next) {
		if (!memcmp(conn->bdaddr, bdaddr, 6))
			return conn;
	}

	return NULL;
}

static struct l2conn *bthost_add_l2cap_conn(struct bthost *bthost,
						struct btconn *conn,
						uint16_t scid, uint16_t dcid,
						uint16_t psm)
{
	struct l2conn *l2conn;

	l2conn = malloc(sizeof(*l2conn));
	if (!l2conn)
		return NULL;

	memset(l2conn, 0, sizeof(*l2conn));

	l2conn->psm = psm;
	l2conn->scid = scid;
	l2conn->dcid = dcid;
	l2conn->mode = L2CAP_MODE_OTHER;

	l2conn->next = conn->l2conns;
	conn->l2conns = l2conn;

	return l2conn;
}

static struct rcconn *bthost_add_rfcomm_conn(struct bthost *bthost,
						struct btconn *conn,
						struct l2conn *l2conn,
						uint8_t channel)
{
	struct rcconn *rcconn;

	rcconn = malloc(sizeof(*rcconn));
	if (!rcconn)
		return NULL;

	memset(rcconn, 0, sizeof(*rcconn));

	rcconn->channel = channel;
	rcconn->scid = l2conn->scid;

	rcconn->next = conn->rcconns;
	conn->rcconns = rcconn;

	return rcconn;
}

static struct rcconn *btconn_find_rfcomm_conn_by_channel(struct btconn *conn,
								uint8_t chan)
{
	struct rcconn *rcconn;

	for (rcconn = conn->rcconns; rcconn != NULL; rcconn = rcconn->next) {
		if (rcconn->channel == chan)
			return rcconn;
	}

	return NULL;
}

static struct l2conn *btconn_find_l2cap_conn_by_scid(struct btconn *conn,
								uint16_t scid)
{
	struct l2conn *l2conn;

	for (l2conn = conn->l2conns; l2conn != NULL; l2conn = l2conn->next) {
		if (l2conn->scid == scid)
			return l2conn;
	}

	return NULL;
}

static struct l2conn *btconn_find_l2cap_conn_by_dcid(struct btconn *conn,
								uint16_t dcid)
{
	struct l2conn *l2conn;

	for (l2conn = conn->l2conns; l2conn != NULL; l2conn = l2conn->next) {
		if (l2conn->dcid == dcid)
			return l2conn;
	}

	return NULL;
}

static struct l2cap_conn_cb_data *bthost_find_l2cap_cb_by_psm(
					struct bthost *bthost, uint16_t psm)
{
	struct l2cap_conn_cb_data *cb;

	for (cb = bthost->new_l2cap_conn_data; cb != NULL; cb = cb->next) {
		if (cb->psm == psm)
			return cb;
	}

	return NULL;
}

static struct rfcomm_conn_cb_data *bthost_find_rfcomm_cb_by_channel(
					struct bthost *bthost, uint8_t channel)
{
	struct rfcomm_conn_cb_data *cb;

	for (cb = bthost->new_rfcomm_conn_data; cb != NULL; cb = cb->next) {
		if (cb->channel == channel)
			return cb;
	}

	return NULL;
}

static struct le_ext_adv *le_ext_adv_new(struct bthost *bthost)
{
	struct le_ext_adv *ext_adv;

	ext_adv = new0(struct le_ext_adv, 1);
	ext_adv->bthost = bthost;

	/* Add to queue */
	if (!queue_push_tail(bthost->le_ext_adv, ext_adv)) {
		free(ext_adv);
		return NULL;
	}

	return ext_adv;
}

static void le_ext_adv_free(void *data)
{
	struct le_ext_adv *ext_adv = data;

	/* Remove from queue */
	queue_remove(ext_adv->bthost->le_ext_adv, ext_adv);

	free(ext_adv);
}

void bthost_destroy(struct bthost *bthost)
{
	if (!bthost)
		return;

	while (bthost->cmd_q.tail) {
		struct cmd *cmd = bthost->cmd_q.tail;

		bthost->cmd_q.tail = cmd->prev;
		free(cmd);
	}

	while (bthost->conns) {
		struct btconn *conn = bthost->conns;

		bthost->conns = conn->next;
		btconn_free(conn);
	}

	while (bthost->l2reqs) {
		struct l2cap_pending_req *req = bthost->l2reqs;

		bthost->l2reqs = req->next;
		req->cb(0, NULL, 0, req->user_data);
		free(req);
	}

	while (bthost->new_l2cap_conn_data) {
		struct l2cap_conn_cb_data *cb = bthost->new_l2cap_conn_data;

		bthost->new_l2cap_conn_data = cb->next;
		free(cb);
	}

	while (bthost->new_rfcomm_conn_data) {
		struct rfcomm_conn_cb_data *cb = bthost->new_rfcomm_conn_data;

		bthost->new_rfcomm_conn_data = cb->next;
		free(cb);
	}

	if (bthost->rfcomm_conn_data)
		free(bthost->rfcomm_conn_data);

	smp_stop(bthost->smp_data);

	queue_destroy(bthost->le_ext_adv, le_ext_adv_free);

	free(bthost);
}

void bthost_set_send_handler(struct bthost *bthost, bthost_send_func handler,
							void *user_data)
{
	if (!bthost)
		return;

	bthost->send_handler = handler;
	bthost->send_data = user_data;
}

void bthost_set_acl_mtu(struct bthost *bthost, uint16_t mtu)
{
	if (!bthost)
		return;

	bthost->acl_mtu = mtu;
}

void bthost_set_iso_mtu(struct bthost *bthost, uint16_t mtu)
{
	if (!bthost)
		return;

	bthost->iso_mtu = mtu;
}

static void queue_command(struct bthost *bthost, const struct iovec *iov,
								int iovlen)
{
	struct cmd_queue *cmd_q = &bthost->cmd_q;
	struct cmd *cmd;
	int i;

	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		return;

	memset(cmd, 0, sizeof(*cmd));

	for (i = 0; i < iovlen; i++) {
		memcpy(cmd->data + cmd->len, iov[i].iov_base, iov[i].iov_len);
		cmd->len += iov[i].iov_len;
	}

	if (cmd_q->tail)
		cmd_q->tail->next = cmd;
	else
		cmd_q->head = cmd;

	cmd->prev = cmd_q->tail;
	cmd_q->tail = cmd;
}

static void send_packet(struct bthost *bthost, const struct iovec *iov,
								int iovlen)
{
	int i;

	if (!bthost->send_handler)
		return;

	for (i = 0; i < iovlen; i++) {
		if (!i)
			util_hexdump('<', iov[i].iov_base, iov[i].iov_len,
				bthost->debug_callback, bthost->debug_data);
		else
			util_hexdump(' ', iov[i].iov_base, iov[i].iov_len,
				bthost->debug_callback, bthost->debug_data);
	}

	bthost->send_handler(iov, iovlen, bthost->send_data);
}

static void iov_pull_n(struct iovec *src, unsigned int *src_cnt,
		struct iovec *dst, unsigned int *dst_cnt, unsigned int max_dst,
		size_t len)
{
	unsigned int i;
	size_t count;

	*dst_cnt = 0;

	while (len && *dst_cnt < max_dst && *src_cnt) {
		count = len;
		if (count > src[0].iov_len)
			count = src[0].iov_len;

		dst[*dst_cnt].iov_base = src[0].iov_base;
		dst[*dst_cnt].iov_len = count;
		*dst_cnt += 1;

		util_iov_pull(&src[0], count);
		len -= count;

		if (!src[0].iov_len) {
			for (i = 1; i < *src_cnt; ++i)
				src[i - 1] = src[i];
			*src_cnt -= 1;
		}
	}
}

static void send_iov(struct bthost *bthost, uint16_t handle, uint16_t cid,
				const struct iovec *iov, unsigned int iovcnt)
{
	struct bt_hci_acl_hdr acl_hdr;
	struct bt_l2cap_hdr l2_hdr;
	uint8_t pkt = BT_H4_ACL_PKT;
	struct iovec pdu[3 + iovcnt];
	struct iovec payload[1 + iovcnt];
	size_t payload_mtu, len;
	int flag;
	unsigned int i;

	len = 0;
	for (i = 0; i < iovcnt; i++) {
		payload[1 + i].iov_base = iov[i].iov_base;
		payload[1 + i].iov_len = iov[i].iov_len;
		len += iov[i].iov_len;
	}

	l2_hdr.cid = cpu_to_le16(cid);
	l2_hdr.len = cpu_to_le16(len);
	payload[0].iov_base = &l2_hdr;
	payload[0].iov_len = sizeof(l2_hdr);

	len += sizeof(l2_hdr);
	iovcnt++;

	/* Fragment to ACL MTU */

	payload_mtu = bthost->acl_mtu - sizeof(pkt) - sizeof(acl_hdr);

	flag = 0x00;
	do {
		size_t count = (len > payload_mtu) ? payload_mtu : len;
		unsigned int pdu_iovcnt;

		pdu[0].iov_base = &pkt;
		pdu[0].iov_len = sizeof(pkt);

		acl_hdr.dlen = cpu_to_le16(count);
		acl_hdr.handle = acl_handle_pack(handle, flag);

		pdu[1].iov_base = &acl_hdr;
		pdu[1].iov_len = sizeof(acl_hdr);

		iov_pull_n(payload, &iovcnt, &pdu[2], &pdu_iovcnt,
						ARRAY_SIZE(pdu) - 2, count);
		pdu_iovcnt += 2;

		send_packet(bthost, pdu, pdu_iovcnt);

		len -= count;
		flag = 0x01;
	} while (len);
}

static void send_acl(struct bthost *bthost, uint16_t handle, uint16_t cid,
				bool sdu_hdr, const void *data, uint16_t len)
{
	struct iovec iov[2];
	uint16_t sdu;
	int num = 0;

	if (sdu_hdr) {
		sdu = cpu_to_le16(len);
		iov[num].iov_base = &sdu;
		iov[num].iov_len = sizeof(sdu);
		num++;
	}

	iov[num].iov_base = (void *) data;
	iov[num].iov_len = len;
	num++;

	send_iov(bthost, handle, cid, iov, num);
}

static uint8_t l2cap_sig_send(struct bthost *bthost, struct btconn *conn,
					uint8_t code, uint8_t ident,
					const void *data, uint16_t len)
{
	static uint8_t next_ident = 1;
	struct bt_l2cap_hdr_sig hdr;
	uint16_t cid;
	struct iovec iov[2];

	if (!ident) {
		ident = next_ident++;
		if (!ident)
			ident = next_ident++;
	}

	hdr.code  = code;
	hdr.ident = ident;
	hdr.len   = cpu_to_le16(len);

	iov[0].iov_base = &hdr;
	iov[0].iov_len = sizeof(hdr);

	if (conn->addr_type == BDADDR_BREDR)
		cid = 0x0001;
	else
		cid = 0x0005;

	if (len == 0) {
		send_iov(bthost, conn->handle, cid, iov, 1);
		return ident;
	}

	iov[1].iov_base = (void *) data;
	iov[1].iov_len = len;

	send_iov(bthost, conn->handle, cid, iov, 2);

	return ident;
}

void bthost_add_cid_hook(struct bthost *bthost, uint16_t handle, uint16_t cid,
				bthost_cid_hook_func_t func, void *user_data)
{
	struct cid_hook *hook;
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	hook = malloc(sizeof(*hook));
	if (!hook)
		return;

	memset(hook, 0, sizeof(*hook));

	hook->cid = cid;
	hook->func = func;
	hook->user_data = user_data;

	hook->next = conn->cid_hooks;
	conn->cid_hooks = hook;
}

void bthost_add_sco_hook(struct bthost *bthost, uint16_t handle,
				bthost_sco_hook_func_t func, void *user_data,
				bthost_destroy_func_t destroy)
{
	struct sco_hook *hook;
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn || conn->sco_hook)
		return;

	hook = malloc(sizeof(*hook));
	if (!hook)
		return;

	memset(hook, 0, sizeof(*hook));

	hook->func = func;
	hook->user_data = user_data;
	hook->destroy = destroy;

	conn->sco_hook = hook;
}

void bthost_add_iso_hook(struct bthost *bthost, uint16_t handle,
				bthost_iso_hook_func_t func, void *user_data,
				bthost_destroy_func_t destroy)
{
	struct iso_hook *hook;
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn || conn->iso_hook)
		return;

	hook = malloc(sizeof(*hook));
	if (!hook)
		return;

	memset(hook, 0, sizeof(*hook));

	hook->func = func;
	hook->user_data = user_data;
	hook->destroy = destroy;

	conn->iso_hook = hook;
}

void bthost_send_cid(struct bthost *bthost, uint16_t handle, uint16_t cid,
					const void *data, uint16_t len)
{
	struct btconn *conn;
	struct l2conn *l2conn;
	bool sdu_hdr = false;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	l2conn = btconn_find_l2cap_conn_by_dcid(conn, cid);
	if (l2conn && (l2conn->mode == L2CAP_MODE_LE_CRED ||
			l2conn->mode == L2CAP_MODE_LE_ENH_CRED))
		sdu_hdr = true;

	send_acl(bthost, handle, cid, sdu_hdr, data, len);
}

void bthost_send_cid_v(struct bthost *bthost, uint16_t handle, uint16_t cid,
					const struct iovec *iov, int iovcnt)
{
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	send_iov(bthost, handle, cid, iov, iovcnt);
}

void bthost_send_sco(struct bthost *bthost, uint16_t handle, uint8_t pkt_status,
					const struct iovec *iov, int iovcnt)
{
	uint8_t pkt = BT_H4_SCO_PKT;
	struct iovec pdu[2 + iovcnt];
	struct bt_hci_sco_hdr sco_hdr;
	struct btconn *conn;
	int i, len = 0;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	for (i = 0; i < iovcnt; i++) {
		pdu[2 + i].iov_base = iov[i].iov_base;
		pdu[2 + i].iov_len = iov[i].iov_len;
		len += iov[i].iov_len;
	}

	pdu[0].iov_base = &pkt;
	pdu[0].iov_len = sizeof(pkt);

	sco_hdr.handle = acl_handle_pack(handle, sco_flags_pack(pkt_status));
	sco_hdr.dlen = len;

	pdu[1].iov_base = &sco_hdr;
	pdu[1].iov_len = sizeof(sco_hdr);

	send_packet(bthost, pdu, 2 + iovcnt);
}

static void send_iso(struct bthost *bthost, uint16_t handle, bool ts,
			uint16_t sn, uint32_t timestamp, uint8_t pkt_status,
			const struct iovec *iov, unsigned int iovcnt)
{
	struct bt_hci_iso_hdr iso_hdr;
	struct bt_hci_iso_data_start data_hdr;
	uint8_t pkt = BT_H4_ISO_PKT;
	struct iovec pdu[4 + iovcnt];
	struct iovec payload[2 + iovcnt];
	uint16_t payload_mtu = bthost->iso_mtu - sizeof(iso_hdr);
	int len = 0, packet = 0;
	unsigned int i;

	for (i = 0; i < iovcnt; i++) {
		payload[2 + i].iov_base = iov[i].iov_base;
		payload[2 + i].iov_len = iov[i].iov_len;
		len += iov[i].iov_len;
	}

	data_hdr.sn = cpu_to_le16(sn);
	data_hdr.slen = cpu_to_le16(iso_data_len_pack(len, pkt_status));

	if (ts) {
		timestamp = cpu_to_le32(timestamp);

		payload[0].iov_base = &timestamp;
		payload[0].iov_len = sizeof(timestamp);
		len += sizeof(timestamp);
	} else {
		payload[0].iov_base = NULL;
		payload[0].iov_len = 0;
	}
	iovcnt++;

	payload[1].iov_base = &data_hdr;
	payload[1].iov_len = sizeof(data_hdr);
	len += sizeof(data_hdr);
	iovcnt++;

	/* ISO fragmentation */

	do {
		unsigned int pdu_iovcnt;
		uint16_t iso_len, pb, flags;

		if (packet == 0 && len <= payload_mtu)
			pb = 0x02;
		else if (packet == 0)
			pb = 0x00;
		else if (len <= payload_mtu)
			pb = 0x03;
		else
			pb = 0x01;

		flags = iso_flags_pack(pb, ts);
		iso_len = len <= payload_mtu ? len : payload_mtu;

		iso_hdr.handle = acl_handle_pack(handle, flags);
		iso_hdr.dlen = cpu_to_le16(iso_len);

		pdu[0].iov_base = &pkt;
		pdu[0].iov_len = sizeof(pkt);

		pdu[1].iov_base = &iso_hdr;
		pdu[1].iov_len = sizeof(iso_hdr);

		iov_pull_n(payload, &iovcnt, &pdu[2], &pdu_iovcnt,
						ARRAY_SIZE(pdu) - 2, iso_len);
		pdu_iovcnt += 2;

		send_packet(bthost, pdu, pdu_iovcnt);

		packet++;
		ts = false;
		len -= iso_len;
	} while (len);
}

void bthost_send_iso(struct bthost *bthost, uint16_t handle, bool ts,
			uint16_t sn, uint32_t timestamp, uint8_t pkt_status,
			const struct iovec *iov, int iovcnt)
{
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	send_iso(bthost, handle, ts, sn, timestamp, pkt_status, iov, iovcnt);
}

bool bthost_l2cap_req(struct bthost *bthost, uint16_t handle, uint8_t code,
				const void *data, uint16_t len,
				bthost_l2cap_rsp_cb cb, void *user_data)
{
	struct l2cap_pending_req *req;
	struct btconn *conn;
	uint8_t ident;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return false;

	if (code == BT_L2CAP_PDU_CONN_REQ &&
			len == sizeof(struct bt_l2cap_pdu_conn_req)) {
		const struct bt_l2cap_pdu_conn_req *req = data;

		bthost_add_l2cap_conn(bthost, conn, le16_to_cpu(req->scid),
							le16_to_cpu(req->scid),
							le16_to_cpu(req->psm));
	}

	ident = l2cap_sig_send(bthost, conn, code, 0, data, len);
	if (!ident)
		return false;

	if (!cb)
		return true;

	req = malloc(sizeof(*req));
	if (!req)
		return false;

	memset(req, 0, sizeof(*req));
	req->ident = ident;
	req->cb = cb;
	req->user_data = user_data;

	req->next = bthost->l2reqs;
	bthost->l2reqs = req;

	return true;
}

static void send_command(struct bthost *bthost, uint16_t opcode,
						const void *data, uint8_t len)
{
	struct bt_hci_cmd_hdr hdr;
	uint8_t pkt = BT_H4_CMD_PKT;
	struct iovec iov[3];

	bthost_debug(bthost, "command 0x%02x", opcode);

	iov[0].iov_base = &pkt;
	iov[0].iov_len = sizeof(pkt);

	hdr.opcode = cpu_to_le16(opcode);
	hdr.plen = len;

	iov[1].iov_base = &hdr;
	iov[1].iov_len = sizeof(hdr);

	if (len > 0) {
		iov[2].iov_base = (void *) data;
		iov[2].iov_len = len;
	}

	if (bthost->ncmd) {
		send_packet(bthost, iov, len > 0 ? 3 : 2);
		bthost->ncmd--;
	} else {
		queue_command(bthost, iov, len > 0 ? 3 : 2);
	}
}

static void next_cmd(struct bthost *bthost)
{
	struct cmd_queue *cmd_q = &bthost->cmd_q;
	struct cmd *cmd = cmd_q->head;
	struct cmd *next;
	struct iovec iov;

	if (!cmd)
		return;

	next = cmd->next;

	if (!bthost->ncmd)
		return;

	iov.iov_base = cmd->data;
	iov.iov_len = cmd->len;

	send_packet(bthost, &iov, 1);
	bthost->ncmd--;

	if (next)
		next->prev = NULL;
	else
		cmd_q->tail = NULL;

	cmd_q->head = next;

	free(cmd);
}

static void read_bd_addr_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_rsp_read_bd_addr *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	memcpy(bthost->bdaddr, ev->bdaddr, 6);

	bthost->ready = true;

	if (bthost->ready_cb) {
		bthost->ready_cb();
		bthost->ready_cb = NULL;
	}
}

void bthost_notify_ready(struct bthost *bthost, bthost_ready_cb cb)
{
	if (bthost->ready) {
		cb();
		return;
	}

	bthost->ready_cb = cb;
}

bool bthost_set_debug(struct bthost *bthost, bthost_debug_func_t callback,
			void *user_data, bthost_destroy_func_t destroy)
{
	if (!bthost)
		return false;

	if (bthost->debug_destroy)
		bthost->debug_destroy(bthost->debug_data);

	bthost->debug_callback = callback;
	bthost->debug_destroy = destroy;
	bthost->debug_data = user_data;

	return true;
}

void bthost_debug(struct bthost *host, const char *format, ...)
{
	va_list ap;

	if (!host || !format || !host->debug_callback)
		return;

	va_start(ap, format);
	util_debug_va(host->debug_callback, host->debug_data, format, ap);
	va_end(ap);
}

static void read_local_features_complete(struct bthost *bthost,
						const void *data, uint8_t len)
{
	const struct bt_hci_rsp_read_local_features *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	memcpy(bthost->features, ev->features, 8);
}

static void evt_cmd_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_cmd_complete *ev = data;
	const void *param;
	uint16_t opcode;

	if (len < sizeof(*ev))
		return;

	param = data + sizeof(*ev);

	bthost->ncmd = ev->ncmd;

	opcode = le16toh(ev->opcode);

	switch (opcode) {
	case BT_HCI_CMD_RESET:
		break;
	case BT_HCI_CMD_READ_LOCAL_FEATURES:
		read_local_features_complete(bthost, param, len - sizeof(*ev));
		break;
	case BT_HCI_CMD_READ_BD_ADDR:
		read_bd_addr_complete(bthost, param, len - sizeof(*ev));
		break;
	case BT_HCI_CMD_WRITE_SCAN_ENABLE:
		break;
	case BT_HCI_CMD_LE_SET_ADV_ENABLE:
		break;
	case BT_HCI_CMD_LE_SET_ADV_PARAMETERS:
		break;
	case BT_HCI_CMD_PIN_CODE_REQUEST_REPLY:
		break;
	case BT_HCI_CMD_PIN_CODE_REQUEST_NEG_REPLY:
		break;
	case BT_HCI_CMD_LINK_KEY_REQUEST_NEG_REPLY:
		break;
	case BT_HCI_CMD_WRITE_SIMPLE_PAIRING_MODE:
		break;
	case BT_HCI_CMD_WRITE_LE_HOST_SUPPORTED:
		break;
	case BT_HCI_CMD_WRITE_SECURE_CONN_SUPPORT:
		break;
	case BT_HCI_CMD_IO_CAPABILITY_REQUEST_REPLY:
		break;
	case BT_HCI_CMD_USER_CONFIRM_REQUEST_REPLY:
		break;
	case BT_HCI_CMD_USER_CONFIRM_REQUEST_NEG_REPLY:
		break;
	case BT_HCI_CMD_LE_LTK_REQ_REPLY:
		break;
	case BT_HCI_CMD_LE_LTK_REQ_NEG_REPLY:
		break;
	case BT_HCI_CMD_LE_SET_ADV_DATA:
		break;
	case BT_HCI_CMD_LE_SET_EXT_ADV_PARAMS:
		break;
	case BT_HCI_CMD_LE_SET_EXT_ADV_DATA:
		break;
	case BT_HCI_CMD_LE_SET_EXT_ADV_ENABLE:
		break;
	case BT_HCI_CMD_LE_SET_PA_PARAMS:
		break;
	case BT_HCI_CMD_LE_SET_PA_ENABLE:
		break;
	default:
		bthost_debug(bthost, "Unhandled cmd_complete opcode 0x%04x",
								opcode);
		break;
	}

	if (bthost->cmd_complete_cb)
		bthost->cmd_complete_cb(opcode, 0, param, len - sizeof(*ev),
						bthost->cmd_complete_data);

	next_cmd(bthost);
}

static void evt_cmd_status(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_cmd_status *ev = data;
	uint16_t opcode;

	if (len < sizeof(*ev))
		return;

	bthost->ncmd = ev->ncmd;

	opcode = le16toh(ev->opcode);

	if (ev->status && bthost->cmd_complete_cb)
		bthost->cmd_complete_cb(opcode, ev->status, NULL, 0,
						bthost->cmd_complete_data);

	next_cmd(bthost);
}

static void evt_conn_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_conn_request *ev = data;
	struct bt_hci_cmd_accept_conn_request cmd;

	if (len < sizeof(*ev))
		return;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(cmd.bdaddr, ev->bdaddr, sizeof(ev->bdaddr));

	send_command(bthost, BT_HCI_CMD_ACCEPT_CONN_REQUEST, &cmd,
								sizeof(cmd));
}

static void init_conn(struct bthost *bthost, uint16_t handle,
				const uint8_t *bdaddr, uint8_t addr_type)
{
	struct btconn *conn;
	const uint8_t *ia, *ra;
	uint8_t ia_type, ra_type;

	conn = malloc(sizeof(*conn));
	if (!conn)
		return;

	memset(conn, 0, sizeof(*conn));
	conn->handle = handle;
	memcpy(conn->bdaddr, bdaddr, 6);
	conn->addr_type = addr_type;
	conn->next_cid = 0x0040;

	conn->next = bthost->conns;
	bthost->conns = conn;

	if (bthost->conn_init) {
		ia = bthost->bdaddr;
		if (addr_type == BDADDR_BREDR)
			ia_type = addr_type;
		else
			ia_type = BDADDR_LE_PUBLIC;
		ra = bdaddr;
		ra_type = addr_type;
	} else {
		ia = bdaddr;
		ia_type = addr_type;
		ra = bthost->bdaddr;
		if (addr_type == BDADDR_BREDR)
			ra_type = addr_type;
		else
			ra_type = BDADDR_LE_PUBLIC;
	}

	conn->smp_data = smp_conn_add(bthost->smp_data, handle, ia, ia_type,
					ra, ra_type, bthost->conn_init);

	if (bthost->new_conn_cb)
		bthost->new_conn_cb(conn->handle, bthost->new_conn_data);

	if (addr_type == BDADDR_BREDR) {
		struct bt_l2cap_pdu_info_req req;
		req.type = L2CAP_IT_FIXED_CHAN;
		l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_INFO_REQ, 1,
							&req, sizeof(req));
	}
}

static void init_sco(struct bthost *bthost, uint16_t handle,
				const uint8_t *bdaddr, uint8_t addr_type)
{
	struct btconn *conn;

	bthost_debug(bthost, "SCO handle 0x%4.4x", handle);

	conn = malloc(sizeof(*conn));
	if (!conn)
		return;

	memset(conn, 0, sizeof(*conn));
	conn->handle = handle;
	memcpy(conn->bdaddr, bdaddr, 6);
	conn->addr_type = addr_type;

	conn->next = bthost->conns;
	bthost->conns = conn;

	if (bthost->new_sco_cb)
		bthost->new_sco_cb(handle, bthost->new_sco_data);
}

static void evt_conn_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_conn_complete *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	if (ev->link_type == 0x00) {
		init_sco(bthost, le16_to_cpu(ev->handle), ev->bdaddr,
								BDADDR_BREDR);
	} else if (ev->link_type == 0x01) {
		init_conn(bthost, le16_to_cpu(ev->handle), ev->bdaddr,
								BDADDR_BREDR);
	}
}

static void evt_disconn_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_disconnect_complete *ev = data;
	struct btconn **curr;
	uint16_t handle;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	handle = le16_to_cpu(ev->handle);

	for (curr = &bthost->conns; *curr;) {
		struct btconn *conn = *curr;

		if (conn->handle == handle) {
			*curr = conn->next;
			btconn_free(conn);
		} else {
			curr = &conn->next;
		}
	}
}

static void evt_num_completed_packets(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_num_completed_packets *ev = data;

	if (len < sizeof(*ev))
		return;
}

static void evt_auth_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_auth_complete *ev = data;
	struct bt_hci_cmd_set_conn_encrypt cp;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	cp.handle = ev->handle;
	cp.encr_mode = 0x01;

	send_command(bthost, BT_HCI_CMD_SET_CONN_ENCRYPT, &cp, sizeof(cp));
}

static void evt_pin_code_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_pin_code_request *ev = data;

	if (len < sizeof(*ev))
		return;

	if (bthost->pin_len > 0) {
		struct bt_hci_cmd_pin_code_request_reply cp;

		memset(&cp, 0, sizeof(cp));
		memcpy(cp.bdaddr, ev->bdaddr, 6);
		cp.pin_len = bthost->pin_len;
		memcpy(cp.pin_code, bthost->pin, bthost->pin_len);

		send_command(bthost, BT_HCI_CMD_PIN_CODE_REQUEST_REPLY,
							&cp, sizeof(cp));
	} else {
		struct bt_hci_cmd_pin_code_request_neg_reply cp;

		memcpy(cp.bdaddr, ev->bdaddr, 6);
		send_command(bthost, BT_HCI_CMD_PIN_CODE_REQUEST_NEG_REPLY,
							&cp, sizeof(cp));
	}
}

static void evt_link_key_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_link_key_request *ev = data;
	struct bt_hci_cmd_link_key_request_neg_reply cp;

	if (len < sizeof(*ev))
		return;

	memset(&cp, 0, sizeof(cp));
	memcpy(cp.bdaddr, ev->bdaddr, 6);

	send_command(bthost, BT_HCI_CMD_LINK_KEY_REQUEST_NEG_REPLY,
							&cp, sizeof(cp));
}

static void evt_link_key_notify(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_link_key_notify *ev = data;

	if (len < sizeof(*ev))
		return;
}

static void evt_encrypt_change(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_encrypt_change *ev = data;
	struct btconn *conn;
	uint16_t handle;

	if (len < sizeof(*ev))
		return;

	handle = acl_handle(ev->handle);
	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	if (ev->status)
		return;

	conn->encr_mode = ev->encr_mode;

	if (conn->smp_data)
		smp_conn_encrypted(conn->smp_data, conn->encr_mode);
}

static void evt_io_cap_response(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_io_capability_response *ev = data;
	struct btconn *conn;

	if (len < sizeof(*ev))
		return;

	conn = bthost_find_conn_by_bdaddr(bthost, ev->bdaddr);
	if (!conn)
		return;
}

static void evt_io_cap_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_io_capability_request *ev = data;
	struct bt_hci_cmd_io_capability_request_reply cp;
	struct btconn *conn;

	if (len < sizeof(*ev))
		return;

	conn = bthost_find_conn_by_bdaddr(bthost, ev->bdaddr);
	if (!conn)
		return;

	memcpy(cp.bdaddr, ev->bdaddr, 6);
	cp.capability = bthost->io_capability;
	cp.oob_data = 0x00;
	cp.authentication = bthost->auth_req;

	send_command(bthost, BT_HCI_CMD_IO_CAPABILITY_REQUEST_REPLY,
							&cp, sizeof(cp));
}

static void evt_user_confirm_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_user_confirm_request *ev = data;
	struct btconn *conn;

	if (len < sizeof(*ev))
		return;

	conn = bthost_find_conn_by_bdaddr(bthost, ev->bdaddr);
	if (!conn)
		return;

	if (bthost->reject_user_confirm) {
		send_command(bthost, BT_HCI_CMD_USER_CONFIRM_REQUEST_NEG_REPLY,
								ev->bdaddr, 6);
		return;
	}

	send_command(bthost, BT_HCI_CMD_USER_CONFIRM_REQUEST_REPLY,
								ev->bdaddr, 6);
}

static void evt_simple_pairing_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_simple_pairing_complete *ev = data;

	if (len < sizeof(*ev))
		return;
}

static void evt_sync_conn_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_sync_conn_complete *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	init_sco(bthost, le16_to_cpu(ev->handle), ev->bdaddr, BDADDR_BREDR);
}

static void evt_le_conn_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_le_conn_complete *ev = data;
	uint8_t addr_type;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	if (ev->peer_addr_type == 0x00)
		addr_type = BDADDR_LE_PUBLIC;
	else
		addr_type = BDADDR_LE_RANDOM;

	init_conn(bthost, le16_to_cpu(ev->handle), ev->peer_addr, addr_type);
}

static void evt_le_ext_conn_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_le_enhanced_conn_complete *ev = data;
	uint8_t addr_type;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;

	if (ev->peer_addr_type == 0x00)
		addr_type = BDADDR_LE_PUBLIC;
	else
		addr_type = BDADDR_LE_RANDOM;

	init_conn(bthost, le16_to_cpu(ev->handle), ev->peer_addr, addr_type);
}

static void evt_le_conn_update_complete(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_le_conn_update_complete *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;
}

static void evt_le_remote_features_complete(struct bthost *bthost,
						const void *data, uint8_t len)
{
	const struct bt_hci_evt_le_remote_features_complete *ev = data;

	if (len < sizeof(*ev))
		return;

	if (ev->status)
		return;
}

static void evt_le_ltk_request(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_le_long_term_key_request *ev = data;
	struct bt_hci_cmd_le_ltk_req_reply cp;
	struct bt_hci_cmd_le_ltk_req_neg_reply *neg_cp = (void *) &cp;
	uint16_t handle, ediv;
	uint64_t rand;
	struct btconn *conn;
	int err;

	if (len < sizeof(*ev))
		return;

	handle = acl_handle(ev->handle);
	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	rand = le64_to_cpu(ev->rand);
	ediv = le16_to_cpu(ev->ediv);

	cp.handle = ev->handle;

	err = smp_get_ltk(conn->smp_data, rand, ediv, cp.ltk);
	if (err < 0)
		send_command(bthost, BT_HCI_CMD_LE_LTK_REQ_NEG_REPLY,
						neg_cp, sizeof(*neg_cp));
	else
		send_command(bthost, BT_HCI_CMD_LE_LTK_REQ_REPLY, &cp,
								sizeof(cp));
}

static void init_iso(struct bthost *bthost, uint16_t handle,
				const uint8_t *bdaddr, uint8_t addr_type)
{
	struct btconn *conn;

	bthost_debug(bthost, "ISO handle 0x%4.4x", handle);

	conn = malloc(sizeof(*conn));
	if (!conn)
		return;

	memset(conn, 0, sizeof(*conn));
	conn->handle = handle;
	memcpy(conn->bdaddr, bdaddr, 6);
	conn->addr_type = addr_type;

	conn->next = bthost->conns;
	bthost->conns = conn;

	if (bthost->new_iso_cb)
		bthost->new_iso_cb(handle, bthost->new_iso_data);
}

static void evt_le_cis_established(struct bthost *bthost, const void *data,
							uint8_t size)
{
	const struct bt_hci_evt_le_cis_established *ev = data;

	if (ev->status)
		return;

	init_iso(bthost, ev->conn_handle, BDADDR_ANY->b, BDADDR_LE_PUBLIC);
}

static void evt_le_cis_req(struct bthost *bthost, const void *data, uint8_t len)
{
	const struct bt_hci_evt_le_cis_req *ev = data;
	struct bt_hci_cmd_le_accept_cis cmd;
	struct bt_hci_cmd_le_reject_cis rej;

	if (len < sizeof(*ev))
		return;

	if (bthost->accept_iso_cb) {
		memset(&rej, 0, sizeof(rej));

		rej.reason = bthost->accept_iso_cb(le16_to_cpu(ev->cis_handle),
							bthost->new_iso_data);
		if (rej.reason) {
			rej.handle = ev->cis_handle;
			send_command(bthost, BT_HCI_CMD_LE_REJECT_CIS,
						     &rej, sizeof(rej));
			return;
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.handle = ev->cis_handle;

	send_command(bthost, BT_HCI_CMD_LE_ACCEPT_CIS, &cmd, sizeof(cmd));
}

static void evt_le_ext_adv_report(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const struct bt_hci_evt_le_ext_adv_report *ev = data;
	const struct bt_hci_le_ext_adv_report *report;
	struct le_ext_adv *le_ext_adv;
	int i;

	data += sizeof(ev->num_reports);

	for (i = 0; i < ev->num_reports; i++) {
		char addr_str[18];

		report = data;
		ba2str((bdaddr_t *) report->addr, addr_str);

		bthost_debug(bthost, "le ext adv report: %s (0x%02x)",
						addr_str, report->addr_type);

		/* Add ext event to the queue */
		le_ext_adv = le_ext_adv_new(bthost);
		if (le_ext_adv) {
			le_ext_adv->addr_type = report->addr_type;
			memcpy(le_ext_adv->addr, report->addr, 6);
			le_ext_adv->direct_addr_type = report->direct_addr_type;
			memcpy(le_ext_adv->direct_addr, report->direct_addr, 6);
		}

		data += (sizeof(*report) + report->data_len);
	}
}

static void evt_le_big_complete(struct bthost *bthost, const void *data,
							uint8_t size)
{
	const struct bt_hci_evt_le_big_complete *ev = data;
	int i;

	if (ev->status)
		return;

	for (i = 0; i < ev->num_bis; i++) {
		uint16_t handle = le16_to_cpu(ev->bis_handle[i]);

		init_iso(bthost, handle, BDADDR_ANY->b, BDADDR_LE_PUBLIC);
	}
}

static void evt_le_big_sync_established(struct bthost *bthost,
						const void *data, uint8_t size)
{
	const struct bt_hci_evt_le_big_sync_estabilished *ev = data;
	int i;

	if (ev->status)
		return;

	for (i = 0; i < ev->num_bis; i++) {
		uint16_t handle = le16_to_cpu(ev->bis[i]);

		init_iso(bthost, handle, BDADDR_ANY->b, BDADDR_LE_PUBLIC);
	}
}

static void evt_le_meta_event(struct bthost *bthost, const void *data,
								uint8_t len)
{
	const uint8_t *event = data;
	const void *evt_data = data + 1;

	if (len < 1)
		return;

	bthost_debug(bthost, "meta event 0x%02x", *event);

	switch (*event) {
	case BT_HCI_EVT_LE_CONN_COMPLETE:
		evt_le_conn_complete(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_CONN_UPDATE_COMPLETE:
		evt_le_conn_update_complete(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_REMOTE_FEATURES_COMPLETE:
		evt_le_remote_features_complete(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_LONG_TERM_KEY_REQUEST:
		evt_le_ltk_request(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_ENHANCED_CONN_COMPLETE:
		evt_le_ext_conn_complete(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_EXT_ADV_REPORT:
		evt_le_ext_adv_report(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_CIS_ESTABLISHED:
		evt_le_cis_established(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_CIS_REQ:
		evt_le_cis_req(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_BIG_COMPLETE:
		evt_le_big_complete(bthost, evt_data, len - 1);
		break;
	case BT_HCI_EVT_LE_BIG_SYNC_ESTABILISHED:
		evt_le_big_sync_established(bthost, evt_data, len - 1);
		break;
	default:
		bthost_debug(bthost, "Unsupported LE Meta event 0x%2.2x",
								*event);
		break;
	}
}

static void process_evt(struct bthost *bthost, const void *data, uint16_t len)
{
	const struct bt_hci_evt_hdr *hdr = data;
	const void *param;

	if (len < sizeof(*hdr))
		return;

	if (sizeof(*hdr) + hdr->plen != len)
		return;

	param = data + sizeof(*hdr);

	bthost_debug(bthost, "event 0x%02x", hdr->evt);

	switch (hdr->evt) {
	case BT_HCI_EVT_CMD_COMPLETE:
		evt_cmd_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_CMD_STATUS:
		evt_cmd_status(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_CONN_REQUEST:
		evt_conn_request(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_CONN_COMPLETE:
		evt_conn_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_DISCONNECT_COMPLETE:
		evt_disconn_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_SYNC_CONN_COMPLETE:
		evt_sync_conn_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_NUM_COMPLETED_PACKETS:
		evt_num_completed_packets(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_AUTH_COMPLETE:
		evt_auth_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_PIN_CODE_REQUEST:
		evt_pin_code_request(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_LINK_KEY_REQUEST:
		evt_link_key_request(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_LINK_KEY_NOTIFY:
		evt_link_key_notify(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_ENCRYPT_CHANGE:
		evt_encrypt_change(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_IO_CAPABILITY_RESPONSE:
		evt_io_cap_response(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_IO_CAPABILITY_REQUEST:
		evt_io_cap_request(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_USER_CONFIRM_REQUEST:
		evt_user_confirm_request(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_SIMPLE_PAIRING_COMPLETE:
		evt_simple_pairing_complete(bthost, param, hdr->plen);
		break;

	case BT_HCI_EVT_LE_META_EVENT:
		evt_le_meta_event(bthost, param, hdr->plen);
		break;

	default:
		bthost_debug(bthost, "Unsupported event 0x%2.2x", hdr->evt);
		break;
	}
}

static bool l2cap_cmd_rej(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_cmd_reject *rsp = data;

	if (len < sizeof(*rsp))
		return false;

	return true;
}

static bool l2cap_conn_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_conn_req *req = data;
	struct l2cap_conn_cb_data *cb_data;
	struct bt_l2cap_pdu_conn_rsp rsp;
	uint16_t psm;

	if (len < sizeof(*req))
		return false;

	psm = le16_to_cpu(req->psm);

	memset(&rsp, 0, sizeof(rsp));
	rsp.scid = req->scid;

	cb_data = bthost_find_l2cap_cb_by_psm(bthost, psm);
	if (cb_data)
		rsp.dcid = rsp.scid;
	else
		rsp.result = cpu_to_le16(0x0002); /* PSM Not Supported */

	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CONN_RSP, ident, &rsp,
								sizeof(rsp));

	if (!rsp.result) {
		struct bt_l2cap_pdu_config_req conf_req;
		struct l2conn *l2conn;

		l2conn = bthost_add_l2cap_conn(bthost, conn,
							le16_to_cpu(rsp.dcid),
							le16_to_cpu(rsp.scid),
							le16_to_cpu(psm));

		memset(&conf_req, 0, sizeof(conf_req));
		conf_req.dcid = rsp.scid;

		l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CONFIG_REQ, 0,
						&conf_req, sizeof(conf_req));

		if (cb_data && l2conn->psm == cb_data->psm && cb_data->func)
			cb_data->func(conn->handle, l2conn->dcid,
							cb_data->user_data);
	}

	return true;
}

static void rfcomm_sabm_send(struct bthost *bthost, struct btconn *conn,
			struct l2conn *l2conn, uint8_t cr, uint8_t dlci)
{
	struct rfcomm_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.address = RFCOMM_ADDR(cr, dlci);
	cmd.control = RFCOMM_CTRL(RFCOMM_SABM, 1);
	cmd.length = RFCOMM_LEN8(0);
	cmd.fcs = rfcomm_fcs2((uint8_t *)&cmd);

	send_acl(bthost, conn->handle, l2conn->dcid, false, &cmd, sizeof(cmd));
}

static bool l2cap_conn_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_conn_rsp *rsp = data;
	struct bt_l2cap_pdu_config_req req;
	struct l2conn *l2conn;

	if (len < sizeof(*rsp))
		return false;

	l2conn = btconn_find_l2cap_conn_by_scid(conn, le16_to_cpu(rsp->scid));
	if (l2conn)
		l2conn->dcid = le16_to_cpu(rsp->dcid);
	else
		return false;

	if (rsp->result)
		return true;

	memset(&req, 0, sizeof(req));
	req.dcid = rsp->dcid;

	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CONFIG_REQ, 0,
							&req, sizeof(req));

	return true;
}

static bool l2cap_config_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_config_req *req = data;
	struct bt_l2cap_pdu_config_rsp rsp;
	struct l2conn *l2conn;
	uint16_t dcid;

	if (len < sizeof(*req))
		return false;

	dcid = le16_to_cpu(req->dcid);

	l2conn = btconn_find_l2cap_conn_by_scid(conn, dcid);
	if (!l2conn)
		return false;

	memset(&rsp, 0, sizeof(rsp));
	rsp.scid  = cpu_to_le16(l2conn->dcid);
	rsp.flags = req->flags;

	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CONFIG_RSP, ident, &rsp,
								sizeof(rsp));

	return true;
}

static bool l2cap_config_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_config_rsp *rsp = data;
	struct l2conn *l2conn;

	if (len < sizeof(*rsp))
		return false;

	l2conn = btconn_find_l2cap_conn_by_scid(conn, rsp->scid);
	if (!l2conn)
		return false;

	if (l2conn->psm == 0x0003 && !rsp->result && bthost->rfcomm_conn_data)
		rfcomm_sabm_send(bthost, conn, l2conn, 1, 0);

	return true;
}

static bool l2cap_disconn_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_disconn_req *req = data;
	struct l2cap_conn_cb_data *cb_data;
	struct bt_l2cap_pdu_disconn_rsp rsp;
	struct l2conn *l2conn;

	if (len < sizeof(*req))
		return false;

	memset(&rsp, 0, sizeof(rsp));
	rsp.dcid = req->dcid;
	rsp.scid = req->scid;

	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_DISCONN_RSP, ident, &rsp,
								sizeof(rsp));

	l2conn = btconn_find_l2cap_conn_by_scid(conn, rsp.scid);
	if (!l2conn)
		return true;

	cb_data = bthost_find_l2cap_cb_by_psm(bthost, l2conn->psm);

	if (cb_data && cb_data->disconn_func)
		cb_data->disconn_func(cb_data->user_data);

	return true;
}

static bool l2cap_info_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_info_req *req = data;
	uint64_t fixed_chan;
	uint16_t type;
	uint8_t buf[12];
	struct bt_l2cap_pdu_info_rsp *rsp = (void *) buf;

	if (len < sizeof(*req))
		return false;

	memset(buf, 0, sizeof(buf));
	rsp->type = req->type;

	type = le16_to_cpu(req->type);

	switch (type) {
	case L2CAP_IT_FEAT_MASK:
		rsp->result = 0x0000;
		put_le32(L2CAP_FEAT_FIXED_CHAN, rsp->data);
		l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_INFO_RSP, ident,
							rsp, sizeof(*rsp) + 4);
		break;
	case L2CAP_IT_FIXED_CHAN:
		rsp->result = 0x0000;
		fixed_chan = L2CAP_FC_SIG_BREDR;
		if (bthost->sc && bthost->le)
			fixed_chan |= L2CAP_FC_SMP_BREDR;
		put_le64(fixed_chan, rsp->data);
		l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_INFO_RSP, ident,
				rsp, sizeof(*rsp) + sizeof(fixed_chan));
		break;
	default:
		rsp->result = cpu_to_le16(0x0001); /* Not Supported */
		l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_INFO_RSP, ident,
							rsp, sizeof(*rsp));
		break;
	}

	return true;
}

static bool l2cap_info_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_info_rsp *rsp = data;
	uint16_t type;

	if (len < sizeof(*rsp))
		return false;

	if (rsp->result)
		return true;

	type = le16_to_cpu(rsp->type);

	switch (type) {
	case L2CAP_IT_FIXED_CHAN:
		if (len < sizeof(*rsp) + 8)
			return false;
		conn->fixed_chan = get_le64(rsp->data);
		if (conn->smp_data && conn->encr_mode)
			smp_conn_encrypted(conn->smp_data, conn->encr_mode);
		break;
	default:
		break;
	}

	return true;
}

static void handle_pending_l2reqs(struct bthost *bthost, struct btconn *conn,
						uint8_t ident, uint8_t code,
						const void *data, uint16_t len)
{
	struct l2cap_pending_req **curr;

	for (curr = &bthost->l2reqs; *curr != NULL;) {
		struct l2cap_pending_req *req = *curr;

		if (req->ident != ident) {
			curr = &req->next;
			continue;
		}

		*curr = req->next;
		req->cb(code, data, len, req->user_data);
		free(req);
	}
}

static bool l2cap_rsp(uint8_t code)
{
	switch (code) {
	case BT_L2CAP_PDU_CMD_REJECT:
	case BT_L2CAP_PDU_CONN_RSP:
	case BT_L2CAP_PDU_CONFIG_RSP:
	case BT_L2CAP_PDU_INFO_RSP:
		return true;
	}

	return false;
}

static void l2cap_sig(struct bthost *bthost, struct btconn *conn,
						const void *data, uint16_t len)
{
	const struct bt_l2cap_hdr_sig *hdr = data;
	struct bt_l2cap_pdu_cmd_reject rej;
	uint16_t hdr_len;
	bool ret;

	if (len < sizeof(*hdr))
		goto reject;

	hdr_len = le16_to_cpu(hdr->len);

	if (sizeof(*hdr) + hdr_len != len)
		goto reject;

	switch (hdr->code) {
	case BT_L2CAP_PDU_CMD_REJECT:
		ret = l2cap_cmd_rej(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONN_REQ:
		ret = l2cap_conn_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONN_RSP:
		ret = l2cap_conn_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONFIG_REQ:
		ret = l2cap_config_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONFIG_RSP:
		ret = l2cap_config_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_DISCONN_REQ:
		ret = l2cap_disconn_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_INFO_REQ:
		ret = l2cap_info_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_INFO_RSP:
		ret = l2cap_info_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	default:
		bthost_debug(bthost, "Unknown L2CAP code 0x%02x", hdr->code);
		ret = false;
	}

	if (l2cap_rsp(hdr->code))
		handle_pending_l2reqs(bthost, conn, hdr->ident, hdr->code,
						data + sizeof(*hdr), hdr_len);

	if (ret)
		return;

reject:
	memset(&rej, 0, sizeof(rej));
	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CMD_REJECT, 0,
							&rej, sizeof(rej));
}

static bool l2cap_conn_param_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_conn_param_req *req = data;
	struct bt_l2cap_pdu_conn_param_rsp rsp;
	struct bt_hci_cmd_le_conn_update hci_cmd;

	if (len < sizeof(*req))
		return false;

	memset(&hci_cmd, 0, sizeof(hci_cmd));
	hci_cmd.handle = cpu_to_le16(conn->handle);
	hci_cmd.min_interval = req->min_interval;
	hci_cmd.max_interval = req->max_interval;
	hci_cmd.latency = req->latency;
	hci_cmd.supv_timeout = req->timeout;
	hci_cmd.min_length = cpu_to_le16(0x0001);
	hci_cmd.max_length = cpu_to_le16(0x0001);

	send_command(bthost, BT_HCI_CMD_LE_CONN_UPDATE,
						&hci_cmd, sizeof(hci_cmd));

	memset(&rsp, 0, sizeof(rsp));
	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CONN_PARAM_RSP, ident,
							&rsp, sizeof(rsp));

	return true;
}

static bool l2cap_conn_param_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_conn_param_req *rsp = data;

	if (len < sizeof(*rsp))
		return false;

	return true;
}

static bool l2cap_le_conn_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_le_conn_req *req = data;
	struct l2cap_conn_cb_data *cb_data;
	struct bt_l2cap_pdu_le_conn_rsp rsp;
	uint16_t psm;

	if (len < sizeof(*req))
		return false;

	psm = le16_to_cpu(req->psm);

	memset(&rsp, 0, sizeof(rsp));

	cb_data = bthost_find_l2cap_cb_by_psm(bthost, psm);
	if (cb_data) {
		rsp.dcid = cpu_to_le16(conn->next_cid++);
		rsp.mtu = cpu_to_le16(cb_data->mtu) ? : cpu_to_le16(23);
		rsp.mps = cpu_to_le16(cb_data->mps) ? : cpu_to_le16(23);
		rsp.credits = cpu_to_le16(cb_data->credits) ? : cpu_to_le16(1);
	} else
		rsp.result = cpu_to_le16(0x0002); /* PSM Not Supported */

	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_LE_CONN_RSP, ident, &rsp,
								sizeof(rsp));

	if (!rsp.result) {
		struct l2conn *l2conn;

		l2conn = bthost_add_l2cap_conn(bthost, conn,
							le16_to_cpu(rsp.dcid),
							le16_to_cpu(req->scid),
							le16_to_cpu(psm));
		l2conn->mode = L2CAP_MODE_LE_CRED;

		if (cb_data && l2conn->psm == cb_data->psm && cb_data->func)
			cb_data->func(conn->handle, l2conn->dcid,
							cb_data->user_data);
	}

	return true;
}

static bool l2cap_le_conn_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_le_conn_rsp *rsp = data;
	struct l2conn *l2conn;

	if (len < sizeof(*rsp))
		return false;
	/* TODO add L2CAP connection before with proper PSM */
	l2conn = bthost_add_l2cap_conn(bthost, conn, 0,
						le16_to_cpu(rsp->dcid), 0);
	l2conn->mode = L2CAP_MODE_LE_CRED;

	return true;
}

static bool l2cap_ecred_conn_req(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct bt_l2cap_pdu_ecred_conn_req *req = data;
	struct {
		struct bt_l2cap_pdu_ecred_conn_rsp pdu;
		uint16_t dcid[5];
	} __attribute__ ((packed)) rsp;
	uint16_t psm;
	int num_scid, i = 0;

	if (len < sizeof(*req))
		return false;

	psm = le16_to_cpu(req->psm);

	memset(&rsp, 0, sizeof(rsp));

	rsp.pdu.mtu = 64;
	rsp.pdu.mps = 64;
	rsp.pdu.credits = 1;

	if (!bthost_find_l2cap_cb_by_psm(bthost, psm)) {
		rsp.pdu.result = cpu_to_le16(0x0002); /* PSM Not Supported */
		goto respond;
	}

	len -= sizeof(rsp.pdu);
	num_scid = len / sizeof(*req->scid);

	for (; i < num_scid; i++)
		rsp.dcid[i] = cpu_to_le16(conn->next_cid++);

respond:
	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_ECRED_CONN_RSP, ident, &rsp,
			sizeof(rsp.pdu) + i * sizeof(*rsp.dcid));

	return true;
}

static bool l2cap_ecred_conn_rsp(struct bthost *bthost, struct btconn *conn,
				uint8_t ident, const void *data, uint16_t len)
{
	const struct  {
		const struct bt_l2cap_pdu_ecred_conn_rsp *pdu;
		uint16_t scid[5];
	} __attribute__ ((packed)) *rsp = data;
	int num_scid, i;
	struct l2conn *l2conn;

	if (len < sizeof(*rsp))
		return false;

	num_scid = len / sizeof(*rsp->scid);

	for (i = 0; i < num_scid; i++) {
		/* TODO add L2CAP connection before with proper PSM */
		l2conn = bthost_add_l2cap_conn(bthost, conn, 0,
				      le16_to_cpu(rsp->scid[i]), 0);
		l2conn->mode = L2CAP_MODE_LE_ENH_CRED;
	}


	return true;
}

static bool l2cap_le_rsp(uint8_t code)
{
	switch (code) {
	case BT_L2CAP_PDU_CMD_REJECT:
	case BT_L2CAP_PDU_CONN_PARAM_RSP:
	case BT_L2CAP_PDU_LE_CONN_RSP:
	case BT_L2CAP_PDU_ECRED_CONN_RSP:
		return true;
	}

	return false;
}

static void l2cap_le_sig(struct bthost *bthost, struct btconn *conn,
						const void *data, uint16_t len)
{
	const struct bt_l2cap_hdr_sig *hdr = data;
	struct bt_l2cap_pdu_cmd_reject rej;
	uint16_t hdr_len;
	bool ret;

	if (len < sizeof(*hdr))
		goto reject;

	hdr_len = le16_to_cpu(hdr->len);

	if (sizeof(*hdr) + hdr_len != len)
		goto reject;

	switch (hdr->code) {
	case BT_L2CAP_PDU_CMD_REJECT:
		ret = l2cap_cmd_rej(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_DISCONN_REQ:
		ret = l2cap_disconn_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONN_PARAM_REQ:
		ret = l2cap_conn_param_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_CONN_PARAM_RSP:
		ret = l2cap_conn_param_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_LE_CONN_REQ:
		ret = l2cap_le_conn_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_LE_CONN_RSP:
		ret = l2cap_le_conn_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;
	case BT_L2CAP_PDU_ECRED_CONN_REQ:
		ret = l2cap_ecred_conn_req(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	case BT_L2CAP_PDU_ECRED_CONN_RSP:
		ret = l2cap_ecred_conn_rsp(bthost, conn, hdr->ident,
						data + sizeof(*hdr), hdr_len);
		break;

	default:
		bthost_debug(bthost, "Unknown L2CAP code 0x%02x", hdr->code);
		ret = false;
	}

	if (l2cap_le_rsp(hdr->code))
		handle_pending_l2reqs(bthost, conn, hdr->ident, hdr->code,
						data + sizeof(*hdr), hdr_len);

	if (ret)
		return;

reject:
	memset(&rej, 0, sizeof(rej));
	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_CMD_REJECT, 0,
							&rej, sizeof(rej));
}

static struct cid_hook *find_cid_hook(struct btconn *conn, uint16_t cid)
{
	struct cid_hook *hook;

	for (hook = conn->cid_hooks; hook != NULL; hook = hook->next) {
		if (hook->cid == cid)
			return hook;
	}

	return NULL;
}

static struct rfcomm_chan_hook *find_rfcomm_chan_hook(struct btconn *conn,
							uint8_t channel)
{
	struct rfcomm_chan_hook *hook;

	for (hook = conn->rfcomm_chan_hooks; hook != NULL; hook = hook->next)
		if (hook->channel == channel)
			return hook;

	return NULL;
}

static void rfcomm_ua_send(struct bthost *bthost, struct btconn *conn,
			struct l2conn *l2conn, uint8_t cr, uint8_t dlci)
{
	struct rfcomm_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.address = RFCOMM_ADDR(cr, dlci);
	cmd.control = RFCOMM_CTRL(RFCOMM_UA, 1);
	cmd.length = RFCOMM_LEN8(0);
	cmd.fcs = rfcomm_fcs2((uint8_t *)&cmd);

	send_acl(bthost, conn->handle, l2conn->dcid, false, &cmd, sizeof(cmd));
}

static void rfcomm_dm_send(struct bthost *bthost, struct btconn *conn,
			struct l2conn *l2conn, uint8_t cr, uint8_t dlci)
{
	struct rfcomm_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.address = RFCOMM_ADDR(cr, dlci);
	cmd.control = RFCOMM_CTRL(RFCOMM_DM, 1);
	cmd.length = RFCOMM_LEN8(0);
	cmd.fcs = rfcomm_fcs2((uint8_t *)&cmd);

	send_acl(bthost, conn->handle, l2conn->dcid, false, &cmd, sizeof(cmd));
}

static void rfcomm_sabm_recv(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_cmd *hdr = data;
	uint8_t dlci;
	struct rfcomm_conn_cb_data *cb;
	uint8_t chan;

	if (len < sizeof(*hdr))
		return;

	chan = RFCOMM_GET_CHANNEL(hdr->address);
	dlci = RFCOMM_GET_DLCI(hdr->address);

	cb = bthost_find_rfcomm_cb_by_channel(bthost, chan);
	if (!dlci || cb) {
		bthost_add_rfcomm_conn(bthost, conn, l2conn, chan);
		rfcomm_ua_send(bthost, conn, l2conn, 1, dlci);
		if (cb && cb->func)
			cb->func(conn->handle, l2conn->scid, cb->user_data,
									true);
	} else {
		rfcomm_dm_send(bthost, conn, l2conn, 1, dlci);
	}
}

static void rfcomm_disc_recv(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_cmd *hdr = data;
	uint8_t dlci;

	if (len < sizeof(*hdr))
		return;

	dlci = RFCOMM_GET_DLCI(hdr->address);

	rfcomm_ua_send(bthost, conn, l2conn, 0, dlci);
}

static void rfcomm_uih_send(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, uint8_t address,
				uint8_t type, const void *data, uint16_t len)
{
	struct rfcomm_hdr hdr;
	struct rfcomm_mcc mcc;
	uint8_t fcs;
	struct iovec iov[4];

	hdr.address = address;
	hdr.control = RFCOMM_CTRL(RFCOMM_UIH, 0);
	hdr.length  = RFCOMM_LEN8(sizeof(mcc) + len);

	iov[0].iov_base = &hdr;
	iov[0].iov_len = sizeof(hdr);

	mcc.type = type;
	mcc.length = RFCOMM_LEN8(len);

	iov[1].iov_base = &mcc;
	iov[1].iov_len = sizeof(mcc);

	iov[2].iov_base = (void *) data;
	iov[2].iov_len = len;

	fcs = rfcomm_fcs((uint8_t *) &hdr);

	iov[3].iov_base = &fcs;
	iov[3].iov_len = sizeof(fcs);

	send_iov(bthost, conn->handle, l2conn->dcid, iov, 4);
}

static void rfcomm_ua_recv(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_cmd *ua_hdr = data;
	uint8_t channel;
	struct rfcomm_connection_data *conn_data = bthost->rfcomm_conn_data;
	uint8_t type;
	struct rfcomm_pn pn_cmd;

	if (len < sizeof(*ua_hdr))
		return;

	channel = RFCOMM_GET_CHANNEL(ua_hdr->address);
	type = RFCOMM_GET_TYPE(ua_hdr->control);

	if (channel && conn_data && conn_data->channel == channel) {
		bthost_add_rfcomm_conn(bthost, conn, l2conn, channel);
		if (conn_data->cb)
			conn_data->cb(conn->handle, l2conn->scid,
						conn_data->user_data, true);
		free(bthost->rfcomm_conn_data);
		bthost->rfcomm_conn_data = NULL;
		return;
	}

	if (!conn_data || !RFCOMM_TEST_CR(type))
		return;

	bthost_add_rfcomm_conn(bthost, conn, l2conn, channel);

	pn_cmd.dlci = conn_data->channel * 2;
	pn_cmd.priority = 7;
	pn_cmd.ack_timer = 0;
	pn_cmd.max_retrans = 0;
	pn_cmd.mtu = 667;
	pn_cmd.credits = 7;

	rfcomm_uih_send(bthost, conn, l2conn, RFCOMM_ADDR(1, 0),
			RFCOMM_MCC_TYPE(1, RFCOMM_PN), &pn_cmd, sizeof(pn_cmd));
}

static void rfcomm_dm_recv(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_cmd *hdr = data;
	uint8_t channel;
	struct rfcomm_connection_data *conn_data = bthost->rfcomm_conn_data;

	if (len < sizeof(*hdr))
		return;

	channel = RFCOMM_GET_CHANNEL(hdr->address);

	if (conn_data && conn_data->channel == channel) {
		if (conn_data->cb)
			conn_data->cb(conn->handle, l2conn->scid,
						conn_data->user_data, false);
		free(bthost->rfcomm_conn_data);
		bthost->rfcomm_conn_data = NULL;
	}
}

static void rfcomm_msc_recv(struct bthost *bthost, struct btconn *conn,
					struct l2conn *l2conn, uint8_t cr,
					const struct rfcomm_msc *msc)
{
	struct rfcomm_msc msc_cmd;

	msc_cmd.dlci = msc->dlci;
	msc_cmd.v24_sig = msc->v24_sig;

	rfcomm_uih_send(bthost, conn, l2conn, RFCOMM_ADDR(0, 0),
				RFCOMM_MCC_TYPE(cr, RFCOMM_MSC), &msc_cmd,
				sizeof(msc_cmd));
}

static void rfcomm_pn_recv(struct bthost *bthost, struct btconn *conn,
					struct l2conn *l2conn, uint8_t cr,
					const struct rfcomm_pn *pn)
{
	struct rfcomm_pn pn_cmd;

	if (!cr) {
		rfcomm_sabm_send(bthost, conn, l2conn, 1,
					bthost->rfcomm_conn_data->channel * 2);
		return;
	}

	pn_cmd.dlci = pn->dlci;
	pn_cmd.flow_ctrl = pn->flow_ctrl;
	pn_cmd.priority = pn->priority;
	pn_cmd.ack_timer = pn->ack_timer;
	pn_cmd.max_retrans = pn->max_retrans;
	pn_cmd.mtu = pn->mtu;
	pn_cmd.credits = 255;

	rfcomm_uih_send(bthost, conn, l2conn, RFCOMM_ADDR(1, 0),
			RFCOMM_MCC_TYPE(0, RFCOMM_PN), &pn_cmd, sizeof(pn_cmd));
}

static void rfcomm_mcc_recv(struct bthost *bthost, struct btconn *conn,
			struct l2conn *l2conn, const void *data, uint16_t len)
{
	const struct rfcomm_mcc *mcc = data;
	const struct rfcomm_msc *msc;
	const struct rfcomm_pn *pn;

	if (len < sizeof(*mcc))
		return;

	switch (RFCOMM_GET_MCC_TYPE(mcc->type)) {
	case RFCOMM_MSC:
		if (len - sizeof(*mcc) < sizeof(*msc))
			break;

		msc = data + sizeof(*mcc);

		rfcomm_msc_recv(bthost, conn, l2conn,
				RFCOMM_TEST_CR(mcc->type) / 2, msc);
		break;
	case RFCOMM_PN:
		if (len - sizeof(*mcc) < sizeof(*pn))
			break;

		pn = data + sizeof(*mcc);

		rfcomm_pn_recv(bthost, conn, l2conn,
				RFCOMM_TEST_CR(mcc->type) / 2, pn);
		break;
	default:
		break;
	}
}

#define GET_LEN8(length)	((length & 0xfe) >> 1)
#define GET_LEN16(length)	((length & 0xfffe) >> 1)

static void rfcomm_uih_recv(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_hdr *hdr = data;
	uint16_t hdr_len, data_len;
	const void *p;

	if (len < sizeof(*hdr)) {
		bthost_debug(bthost, "RFCOMM UIH: too short");
		return;
	}

	if (RFCOMM_TEST_EA(hdr->length)) {
		data_len = (uint16_t) GET_LEN8(hdr->length);
		hdr_len = sizeof(*hdr);
	} else {
		uint8_t ex_len = *((uint8_t *)(data + sizeof(*hdr)));
		data_len = GET_LEN16((((uint16_t) ex_len << 8) | hdr->length));
		hdr_len = sizeof(*hdr) + sizeof(uint8_t);
	}

	if (len < hdr_len + data_len) {
		bthost_debug(bthost, "RFCOMM UIH: %u != %u", len,
						hdr_len + data_len);
		return;
	}

	p = data + hdr_len;

	if (RFCOMM_GET_DLCI(hdr->address)) {
		struct rfcomm_chan_hook *hook;

		hook = find_rfcomm_chan_hook(conn,
					RFCOMM_GET_CHANNEL(hdr->address));
		if (hook && data_len)
			hook->func(p, data_len, hook->user_data);
	} else {
		rfcomm_mcc_recv(bthost, conn, l2conn, p, data_len);
	}
}

static void process_rfcomm(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, const void *data,
				uint16_t len)
{
	const struct rfcomm_hdr *hdr = data;

	bthost_debug(bthost, "RFCOMM data: %u bytes", len);

	switch (RFCOMM_GET_TYPE(hdr->control)) {
	case RFCOMM_SABM:
		rfcomm_sabm_recv(bthost, conn, l2conn, data, len);
		break;
	case RFCOMM_DISC:
		rfcomm_disc_recv(bthost, conn, l2conn, data, len);
		break;
	case RFCOMM_UA:
		rfcomm_ua_recv(bthost, conn, l2conn, data, len);
		break;
	case RFCOMM_DM:
		rfcomm_dm_recv(bthost, conn, l2conn, data, len);
		break;
	case RFCOMM_UIH:
		rfcomm_uih_recv(bthost, conn, l2conn, data, len);
		break;
	default:
		bthost_debug(bthost, "Unknown frame type");
		break;
	}
}

static void append_l2conn_data(struct bthost *bthost, struct l2conn *conn,
				const void *data, uint16_t len)
{
	if (!conn->recv_data) {
		bthost_debug(bthost, "Unexpected L2CAP SDU data: sCID 0x%4.4x ",
								conn->scid);
		return;
	}

	if (conn->recv_len + len > conn->data_len) {
		bthost_debug(bthost, "Unexpected L2CAP SDU data: sCID 0x%4.4x ",
								conn->scid);
		return;
	}

	memcpy(conn->recv_data + conn->recv_len, data, len);
	conn->recv_len += len;

	bthost_debug(bthost, "L2CAP SDU data: %u/%u bytes", conn->recv_len,
							conn->data_len);
}

static void free_l2conn_data(struct l2conn *conn)
{
	free(conn->recv_data);
	conn->recv_data = NULL;
	conn->recv_len = 0;
	conn->data_len = 0;
}

static void new_l2conn_data(struct bthost *bthost, struct l2conn *conn,
								uint16_t len)
{
	free(conn->recv_data);
	conn->recv_data = malloc(len);
	conn->recv_len = 0;
	conn->data_len = len;
}

static bool process_l2cap_conn(struct bthost *bthost, struct btconn *conn,
				struct l2conn *l2conn, struct iovec *data)
{
	struct bt_l2cap_pdu_le_flowctl_creds creds;
	uint16_t sdu;

	if (!l2conn)
		return true;

	switch (l2conn->mode) {
	case L2CAP_MODE_LE_CRED:
	case L2CAP_MODE_LE_ENH_CRED:
		break;
	case L2CAP_MODE_OTHER:
		return true;
	}

	/* Credit-based flow control */

	creds.cid = cpu_to_le16(l2conn->scid);
	creds.credits = cpu_to_le16(1);
	l2cap_sig_send(bthost, conn, BT_L2CAP_PDU_LE_FLOWCTL_CREDS, 0,
							&creds, sizeof(creds));

	if (!l2conn->data_len) {
		if (!util_iov_pull_le16(data, &sdu)) {
			free_l2conn_data(l2conn);
			bthost_debug(bthost, "L2CAP invalid SDU");
			return false;
		}
		new_l2conn_data(bthost, l2conn, sdu);
	}

	append_l2conn_data(bthost, l2conn, data->iov_base, data->iov_len);

	if (l2conn->recv_len < l2conn->data_len)
		return false;  /* SDU incomplete */

	l2conn->data_len = 0;
	data->iov_base = l2conn->recv_data;
	data->iov_len = l2conn->recv_len;

	return true;
}

static void process_l2cap(struct bthost *bthost, struct btconn *conn,
					const void *buf, uint16_t len)
{
	const struct bt_l2cap_hdr *l2_hdr = buf;
	struct cid_hook *hook;
	struct l2conn *l2conn;
	struct iovec data;
	uint16_t cid, l2_len;

	l2_len = le16_to_cpu(l2_hdr->len);
	if (len != sizeof(*l2_hdr) + l2_len) {
		bthost_debug(bthost, "L2CAP invalid length: %u != %zu",
					len, sizeof(*l2_hdr) + l2_len);
		return;
	}

	bthost_debug(bthost, "L2CAP data: %u bytes", l2_len);

	cid = le16_to_cpu(l2_hdr->cid);
	l2conn = btconn_find_l2cap_conn_by_scid(conn, cid);

	data.iov_base = (void *)l2_hdr->data;
	data.iov_len = l2_len;

	if (!process_l2cap_conn(bthost, conn, l2conn, &data))
		return;

	hook = find_cid_hook(conn, cid);
	if (hook) {
		hook->func(data.iov_base, data.iov_len, hook->user_data);
		return;
	}

	switch (cid) {
	case 0x0001:
		l2cap_sig(bthost, conn, data.iov_base, data.iov_len);
		break;
	case 0x0005:
		l2cap_le_sig(bthost, conn, data.iov_base, data.iov_len);
		break;
	case 0x0006:
		smp_data(conn->smp_data, data.iov_base, data.iov_len);
		break;
	case 0x0007:
		smp_bredr_data(conn->smp_data, data.iov_base, data.iov_len);
		break;
	default:
		if (l2conn && l2conn->psm == 0x0003)
			process_rfcomm(bthost, conn, l2conn, data.iov_base,
								data.iov_len);
		else
			bthost_debug(bthost,
					"Packet for unknown CID 0x%04x (%u)",
					cid, cid);
		break;
	}
}

static void append_recv_data(struct bthost *bthost, struct btconn *conn,
				const char *type, uint8_t flags,
				const void *data, uint16_t len)
{
	if (!conn->recv_data) {
		bthost_debug(bthost, "Unexpected %s frame: handle 0x%4.4x "
				"flags 0x%2.2x", type, conn->handle, flags);
		return;
	}

	if (conn->recv_len + len > conn->data_len) {
		bthost_debug(bthost, "Unexpected %s frame: handle 0x%4.4x "
				"flags 0x%2.2x", type, conn->handle, flags);
		return;
	}

	memcpy(conn->recv_data + conn->recv_len, data, len);
	conn->recv_len += len;

	bthost_debug(bthost, "%s data: %u/%u bytes", type, conn->recv_len,
							conn->data_len);
}

static void free_recv_data(struct btconn *conn)
{
	free(conn->recv_data);
	conn->recv_data = NULL;
	conn->recv_len = 0;
	conn->data_len = 0;
}

static void append_acl_data(struct bthost *bthost, struct btconn *conn,
				uint8_t flags, const void *data, uint16_t len)
{
	append_recv_data(bthost, conn, "ACL", flags, data, len);

	if (conn->recv_len < conn->data_len)
		return;

	process_l2cap(bthost, conn, conn->recv_data, conn->recv_len);

	free_recv_data(conn);
}

static void new_recv_data(struct btconn *conn, uint16_t len)
{
	conn->recv_data = malloc(len);
	conn->recv_len = 0;
	conn->data_len = len;
}

static void process_acl(struct bthost *bthost, const void *data, uint16_t len)
{
	const struct bt_hci_acl_hdr *acl_hdr = data;
	const struct bt_l2cap_hdr *l2_hdr = (void *) acl_hdr->data;
	uint16_t handle, acl_len, l2_len;
	uint8_t flags;
	struct btconn *conn;

	acl_len = le16_to_cpu(acl_hdr->dlen);
	if (len != sizeof(*acl_hdr) + acl_len)
		return;

	handle = acl_handle(acl_hdr->handle);
	flags = acl_flags(acl_hdr->handle);

	conn = bthost_find_conn(bthost, handle);
	if (!conn) {
		bthost_debug(bthost, "Unknown handle: 0x%4.4x", handle);
		return;
	}

	switch (flags) {
	case 0x00:	/* start of a non-automatically-flushable PDU */
	case 0x02:	/* start of an automatically-flushable PDU */
		if (conn->recv_data) {
			bthost_debug(bthost, "Unexpected ACL start frame");
			free_recv_data(conn);
		}

		l2_len = le16_to_cpu(l2_hdr->len) + sizeof(*l2_hdr);

		bthost_debug(bthost, "acl_len %u l2_len %u", acl_len, l2_len);

		if (acl_len == l2_len) {
			process_l2cap(bthost, conn, acl_hdr->data, acl_len);
			break;
		}

		new_recv_data(conn, l2_len);
		/* fall through */
	case 0x01:	/* continuing fragment */
		append_acl_data(bthost, conn, flags, acl_hdr->data, acl_len);
		break;
	case 0x03:	/* complete automatically-flushable PDU */
		process_l2cap(bthost, conn, acl_hdr->data, acl_len);
		break;
	default:
		bthost_debug(bthost, "Invalid ACL frame flags 0x%2.2x", flags);
	}
}

static void process_sco(struct bthost *bthost, const void *data, uint16_t len)
{
	const struct bt_hci_sco_hdr *sco_hdr = data;
	uint16_t handle, sco_len;
	uint8_t status;
	struct btconn *conn;
	struct sco_hook *hook;

	sco_len = le16_to_cpu(sco_hdr->dlen);
	if (len != sizeof(*sco_hdr) + sco_len)
		return;

	handle = acl_handle(sco_hdr->handle);
	status = sco_flags_status(acl_flags(sco_hdr->handle));

	conn = bthost_find_conn(bthost, handle);
	if (!conn) {
		bthost_debug(bthost, "Unknown handle: 0x%4.4x", handle);
		return;
	}

	bthost_debug(bthost, "SCO data: %u bytes", sco_len);

	hook = conn->sco_hook;
	if (!hook)
		return;

	hook->func(sco_hdr->data, sco_len, status, hook->user_data);
}

static void process_iso_data(struct bthost *bthost, struct btconn *conn,
					const void *data, uint16_t len)
{
	const struct bt_hci_iso_data_start *data_hdr = data;
	uint16_t data_len;
	struct iso_hook *hook;

	data_len = le16_to_cpu(data_hdr->slen);
	if (len != sizeof(*data_hdr) + data_len) {
		bthost_debug(bthost, "ISO invalid length: %u != %zu",
					len, sizeof(*data_hdr) + data_len);
		return;
	}

	bthost_debug(bthost, "ISO data: %u bytes (%u)", data_len, data_hdr->sn);

	hook = conn->iso_hook;
	if (!hook)
		return;

	hook->func(data_hdr->data, data_len, hook->user_data);
}

static void append_iso_data(struct bthost *bthost, struct btconn *conn,
				uint8_t flags, const void *data, uint16_t len)
{
	append_recv_data(bthost, conn, "ISO", flags, data, len);

	if (conn->recv_len < conn->data_len) {
		if (flags == 0x03) {
			bthost_debug(bthost, "Unexpected ISO end frame");
			free_recv_data(conn);
		}
		return;
	}

	process_iso_data(bthost, conn, conn->recv_data, conn->recv_len);

	free_recv_data(conn);
}

static void process_iso(struct bthost *bthost, const void *data, uint16_t len)
{
	const struct bt_hci_iso_hdr *iso_hdr = data;
	const struct bt_hci_iso_data_start *data_hdr;
	uint16_t handle, iso_len, data_len;
	uint8_t flags;
	struct btconn *conn;

	iso_len = le16_to_cpu(iso_hdr->dlen);
	if (len != sizeof(*iso_hdr) + iso_len)
		return;

	handle = acl_handle(iso_hdr->handle);
	flags = iso_flags_pb(acl_flags(iso_hdr->handle));

	conn = bthost_find_conn(bthost, handle);
	if (!conn) {
		bthost_debug(bthost, "Unknown handle: 0x%4.4x", handle);
		return;
	}

	data_hdr = (void *) data + sizeof(*iso_hdr);

	switch (flags) {
	case 0x00:
	case 0x02:
		if (conn->recv_data) {
			bthost_debug(bthost, "Unexpected ISO start frame");
			free_recv_data(conn);
		}

		data_len = le16_to_cpu(data_hdr->slen) + sizeof(*data_hdr);

		bthost_debug(bthost, "iso_len %u data_len %u", iso_len,
								data_len);

		if (iso_len == data_len) {
			process_iso_data(bthost, conn, iso_hdr->data, iso_len);
			break;
		}

		new_recv_data(conn, data_len);
		/* fall through */
	case 0x01:
	case 0x03:
		append_iso_data(bthost, conn, flags, iso_hdr->data, iso_len);
		break;
	default:
		bthost_debug(bthost, "Invalid ISO frame flags 0x%2.2x", flags);
	}
}

void bthost_receive_h4(struct bthost *bthost, const void *data, uint16_t len)
{
	uint8_t pkt_type;

	if (!bthost)
		return;

	if (len < 1)
		return;

	util_hexdump('>', data, len, bthost->debug_callback,
					bthost->debug_data);

	pkt_type = ((const uint8_t *) data)[0];

	switch (pkt_type) {
	case BT_H4_EVT_PKT:
		process_evt(bthost, data + 1, len - 1);
		break;
	case BT_H4_ACL_PKT:
		process_acl(bthost, data + 1, len - 1);
		break;
	case BT_H4_SCO_PKT:
		process_sco(bthost, data + 1, len - 1);
		break;
	case BT_H4_ISO_PKT:
		process_iso(bthost, data + 1, len - 1);
		break;
	default:
		bthost_debug(bthost, "Unsupported packet 0x%2.2x", pkt_type);
		break;
	}
}

void bthost_set_cmd_complete_cb(struct bthost *bthost,
				bthost_cmd_complete_cb cb, void *user_data)
{
	bthost->cmd_complete_cb = cb;
	bthost->cmd_complete_data = user_data;
}

void bthost_set_connect_cb(struct bthost *bthost, bthost_new_conn_cb cb,
							void *user_data)
{
	bthost->new_conn_cb = cb;
	bthost->new_conn_data = user_data;
}

void bthost_set_sco_cb(struct bthost *bthost, bthost_new_conn_cb cb,
							void *user_data)
{
	bthost->new_sco_cb = cb;
	bthost->new_sco_data = user_data;
}

void bthost_set_iso_cb(struct bthost *bthost, bthost_accept_conn_cb accept,
				bthost_new_conn_cb cb, void *user_data)
{
	bthost->accept_iso_cb = accept;
	bthost->new_iso_cb = cb;
	bthost->new_iso_data = user_data;
}

void bthost_hci_connect(struct bthost *bthost, const uint8_t *bdaddr,
							uint8_t addr_type)
{
	bthost->conn_init = true;

	if (addr_type == BDADDR_BREDR) {
		struct bt_hci_cmd_create_conn cc;

		memset(&cc, 0, sizeof(cc));
		memcpy(cc.bdaddr, bdaddr, sizeof(cc.bdaddr));

		send_command(bthost, BT_HCI_CMD_CREATE_CONN, &cc, sizeof(cc));
	} else {
		struct bt_hci_cmd_le_create_conn cc;

		memset(&cc, 0, sizeof(cc));
		memcpy(cc.peer_addr, bdaddr, sizeof(cc.peer_addr));

		if (addr_type == BDADDR_LE_RANDOM)
			cc.peer_addr_type = 0x01;

		cc.scan_interval = cpu_to_le16(0x0060);
		cc.scan_window = cpu_to_le16(0x0030);
		cc.min_interval = cpu_to_le16(0x0028);
		cc.max_interval = cpu_to_le16(0x0038);
		cc.supv_timeout = cpu_to_le16(0x002a);

		send_command(bthost, BT_HCI_CMD_LE_CREATE_CONN,
							&cc, sizeof(cc));
	}
}

void bthost_hci_ext_connect(struct bthost *bthost, const uint8_t *bdaddr,
							uint8_t addr_type)
{
	struct bt_hci_cmd_le_ext_create_conn *cc;
	struct bt_hci_le_ext_create_conn *cp;
	uint8_t buf[sizeof(*cc) + sizeof(*cp)];

	cc = (void *)buf;
	cp = (void *)cc->data;

	bthost->conn_init = true;

	memset(cc, 0, sizeof(*cc));
	memset(cp, 0, sizeof(*cp));

	memcpy(cc->peer_addr, bdaddr, sizeof(cc->peer_addr));

	if (addr_type == BDADDR_LE_RANDOM)
		cc->peer_addr_type = 0x01;

	cc->phys = 0x01;

	cp->scan_interval = cpu_to_le16(0x0060);
	cp->scan_window = cpu_to_le16(0x0030);
	cp->min_interval = cpu_to_le16(0x0028);
	cp->max_interval = cpu_to_le16(0x0038);
	cp->supv_timeout = cpu_to_le16(0x002a);

	send_command(bthost, BT_HCI_CMD_LE_EXT_CREATE_CONN,
						buf, sizeof(buf));
}

void bthost_hci_disconnect(struct bthost *bthost, uint16_t handle,
								uint8_t reason)
{
	struct bt_hci_cmd_disconnect disc;

	disc.handle = cpu_to_le16(handle);
	disc.reason = reason;

	send_command(bthost, BT_HCI_CMD_DISCONNECT, &disc, sizeof(disc));
}

void bthost_write_scan_enable(struct bthost *bthost, uint8_t scan)
{
	send_command(bthost, BT_HCI_CMD_WRITE_SCAN_ENABLE, &scan, 1);
}

void bthost_set_adv_data(struct bthost *bthost, const uint8_t *data,
								uint8_t len)
{
	struct bt_hci_cmd_le_set_adv_data adv_cp;

	memset(adv_cp.data, 0, 31);

	if (len) {
		adv_cp.len = len;
		memcpy(adv_cp.data, data, len);
	}

	send_command(bthost, BT_HCI_CMD_LE_SET_ADV_DATA, &adv_cp,
							sizeof(adv_cp));
}

void bthost_set_ext_adv_data(struct bthost *bthost, const uint8_t *data,
								uint8_t len)
{
	struct bt_hci_cmd_le_set_ext_adv_data *adv_cp;
	uint8_t buf[sizeof(*adv_cp) + 31];

	adv_cp = (void *)buf;

	memset(adv_cp, 0, sizeof(*adv_cp));
	memset(adv_cp->data, 0, 31);

	adv_cp->handle = 1;
	adv_cp->operation = 0x03;
	adv_cp->fragment_preference = 0x01;

	if (len) {
		adv_cp->data_len = len;
		memcpy(adv_cp->data, data, len);
	}

	send_command(bthost, BT_HCI_CMD_LE_SET_EXT_ADV_DATA, buf,
							sizeof(buf));
}

void bthost_set_adv_enable(struct bthost *bthost, uint8_t enable)
{
	struct bt_hci_cmd_le_set_adv_parameters cp;

	memset(&cp, 0, sizeof(cp));
	send_command(bthost, BT_HCI_CMD_LE_SET_ADV_PARAMETERS,
							&cp, sizeof(cp));

	send_command(bthost, BT_HCI_CMD_LE_SET_ADV_ENABLE, &enable, 1);
}

void bthost_set_scan_params(struct bthost *bthost, uint8_t scan_type,
				uint8_t addr_type, uint8_t filter_policy)
{
	struct bt_hci_cmd_le_set_scan_parameters cp;

	memset(&cp, 0, sizeof(cp));
	cp.type = scan_type;
	cp.own_addr_type = addr_type;
	cp.filter_policy = filter_policy;
	send_command(bthost, BT_HCI_CMD_LE_SET_SCAN_PARAMETERS,
							&cp, sizeof(cp));
}

void bthost_set_scan_enable(struct bthost *bthost, uint8_t enable)
{
	struct bt_hci_cmd_le_set_scan_enable cp;

	memset(&cp, 0, sizeof(cp));
	cp.enable = enable;
	send_command(bthost, BT_HCI_CMD_LE_SET_SCAN_ENABLE,
							&cp, sizeof(cp));
}

void bthost_set_ext_adv_params(struct bthost *bthost, uint8_t sid)
{
	const uint8_t interval_20ms[] = { 0x20, 0x00, 0x00 };
	struct bt_hci_cmd_le_set_ext_adv_params cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = 0x01;
	cp.evt_properties = cpu_to_le16(0x0013);
	memcpy(cp.min_interval, interval_20ms, sizeof(cp.min_interval));
	memcpy(cp.max_interval, interval_20ms, sizeof(cp.max_interval));
	cp.sid = sid;
	send_command(bthost, BT_HCI_CMD_LE_SET_EXT_ADV_PARAMS,
							&cp, sizeof(cp));
}

void bthost_set_ext_adv_enable(struct bthost *bthost, uint8_t enable)
{
	struct bt_hci_cmd_le_set_ext_adv_enable *cp_enable;
	struct bt_hci_cmd_ext_adv_set *cp_set;
	uint8_t cp[6];

	memset(cp, 0, 6);

	cp_enable = (struct bt_hci_cmd_le_set_ext_adv_enable *)cp;
	cp_set = (struct bt_hci_cmd_ext_adv_set *)(cp + sizeof(*cp_enable));

	cp_enable->enable = enable;
	cp_enable->num_of_sets = 1;
	cp_set->handle = 1;

	send_command(bthost, BT_HCI_CMD_LE_SET_EXT_ADV_ENABLE, cp, 6);
}

void bthost_set_pa_params(struct bthost *bthost)
{
	struct bt_hci_cmd_le_set_pa_params cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = 0x01;
	send_command(bthost, BT_HCI_CMD_LE_SET_PA_PARAMS, &cp, sizeof(cp));
}

static void set_pa_data(struct bthost *bthost, const uint8_t *data,
				uint8_t len, uint8_t offset)
{
	struct bt_hci_cmd_le_set_pa_data *cp;
	uint8_t buf[sizeof(*cp) + BT_PA_MAX_DATA_LEN];
	size_t data_len;

	cp = (void *)buf;

	memset(cp, 0, sizeof(*cp));
	memset(cp->data, 0, BT_PA_MAX_DATA_LEN);

	cp->handle = 1;

	if (len - offset > BT_PA_MAX_DATA_LEN) {
		data_len = BT_PA_MAX_DATA_LEN;

		if (!offset)
			cp->operation = 0x01;
		else
			cp->operation = 0x00;
	} else {
		data_len = len - offset;

		if (!offset)
			cp->operation = 0x03;
		else
			cp->operation = 0x02;
	}

	memcpy(cp->data, data + offset, data_len);
	cp->data_len = data_len;

	send_command(bthost, BT_HCI_CMD_LE_SET_PA_DATA, buf,
					sizeof(*cp) + cp->data_len);

	if (cp->operation == 0x01 || cp->operation == 0x00) {
		offset += cp->data_len;
		set_pa_data(bthost, data, len, offset);
	}
}

void bthost_set_pa_data(struct bthost *bthost, const uint8_t *data,
							uint8_t len)
{
	set_pa_data(bthost, data, len, 0);
}

void bthost_set_base(struct bthost *bthost, const uint8_t *data, uint8_t len)
{
	struct bt_ad *ad;
	bt_uuid_t uuid;
	uint8_t *pa_data;
	size_t pa_len;

	bt_uuid16_create(&uuid, BAA_SERVICE);

	ad = bt_ad_new();
	bt_ad_set_max_len(ad, BT_PA_MAX_DATA_LEN);
	bt_ad_add_service_data(ad, &uuid, (void *)data, len);

	pa_data = bt_ad_generate(ad, &pa_len);
	bthost_set_pa_data(bthost, pa_data, pa_len);

	bt_ad_unref(ad);
}

void bthost_set_pa_enable(struct bthost *bthost, uint8_t enable)
{
	struct bt_hci_cmd_le_set_pa_enable cp;

	memset(&cp, 0, sizeof(cp));
	cp.enable = enable;
	cp.handle = 0x01;
	send_command(bthost, BT_HCI_CMD_LE_SET_PA_ENABLE, &cp, sizeof(cp));
}

void bthost_create_big(struct bthost *bthost, uint8_t num_bis,
				uint8_t enc, const uint8_t *bcode)
{
	struct bt_hci_cmd_le_create_big cp;

	memset(&cp, 0, sizeof(cp));
	cp.handle = 0x01;
	cp.adv_handle = 0x01;
	cp.num_bis = num_bis;
	put_le24(10000, cp.bis.sdu_interval);
	cp.bis.sdu = 40;
	cp.bis.latency = cpu_to_le16(10);
	cp.bis.rtn = 0x02;
	cp.bis.phy = 0x02;
	cp.bis.encryption = enc;
	memcpy(cp.bis.bcode, bcode, sizeof(cp.bis.bcode));
	send_command(bthost, BT_HCI_CMD_LE_CREATE_BIG, &cp, sizeof(cp));
}

bool bthost_search_ext_adv_addr(struct bthost *bthost, const uint8_t *addr)
{
	const struct queue_entry *entry;

	if (queue_isempty(bthost->le_ext_adv))
		return false;

	for (entry = queue_get_entries(bthost->le_ext_adv); entry;
							entry = entry->next) {
		struct le_ext_adv *le_ext_adv = entry->data;

		if (!memcmp(le_ext_adv->addr, addr, 6))
			return true;
	}

	return false;
}

void bthost_set_cig_params(struct bthost *bthost, uint8_t cig_id,
				uint8_t cis_id, const struct bt_iso_qos *qos)
{
	struct bt_hci_cmd_le_set_cig_params *cp;

	cp = malloc(sizeof(*cp) + sizeof(*cp->cis));
	memset(cp, 0, sizeof(*cp) + sizeof(*cp->cis));
	cp->cig_id = cig_id;
	put_le24(qos->ucast.in.interval ? qos->ucast.in.interval :
				qos->ucast.out.interval, cp->c_interval);
	put_le24(qos->ucast.out.interval ? qos->ucast.out.interval :
				qos->ucast.in.interval, cp->p_interval);
	cp->c_latency = cpu_to_le16(qos->ucast.in.latency ?
				qos->ucast.in.latency : qos->ucast.out.latency);
	cp->p_latency = cpu_to_le16(qos->ucast.out.latency ?
				qos->ucast.out.latency : qos->ucast.in.latency);
	cp->num_cis = 0x01;
	cp->cis[0].cis_id = cis_id;
	cp->cis[0].c_sdu = qos->ucast.in.sdu;
	cp->cis[0].p_sdu = qos->ucast.out.sdu;
	cp->cis[0].c_phy = qos->ucast.in.phy ? qos->ucast.in.phy :
							qos->ucast.out.phy;
	cp->cis[0].p_phy = qos->ucast.out.phy ? qos->ucast.out.phy :
							qos->ucast.in.phy;
	cp->cis[0].c_rtn = qos->ucast.in.rtn;
	cp->cis[0].p_rtn = qos->ucast.out.rtn;

	send_command(bthost, BT_HCI_CMD_LE_SET_CIG_PARAMS, cp,
				sizeof(*cp) + sizeof(*cp->cis));
	free(cp);
}

void bthost_create_cis(struct bthost *bthost, uint16_t cis_handle,
						uint16_t acl_handle)
{
	struct bt_hci_cmd_le_create_cis *cp;

	cp = malloc(sizeof(*cp) + sizeof(*cp->cis));
	memset(cp, 0, sizeof(*cp) + sizeof(*cp->cis));
	cp->num_cis = 0x01;
	cp->cis[0].cis_handle = cpu_to_le16(cis_handle);
	cp->cis[0].acl_handle = cpu_to_le16(acl_handle);

	send_command(bthost, BT_HCI_CMD_LE_CREATE_CIS, cp,
				sizeof(*cp) + sizeof(*cp->cis));
	free(cp);
}

void bthost_write_ssp_mode(struct bthost *bthost, uint8_t mode)
{
	send_command(bthost, BT_HCI_CMD_WRITE_SIMPLE_PAIRING_MODE, &mode, 1);
}

void bthost_write_le_host_supported(struct bthost *bthost, uint8_t mode)
{
	struct bt_hci_cmd_write_le_host_supported cmd;

	bthost->le = mode;

	memset(&cmd, 0, sizeof(cmd));
	cmd.supported = mode;
	send_command(bthost, BT_HCI_CMD_WRITE_LE_HOST_SUPPORTED,
							&cmd, sizeof(cmd));
}

bool bthost_bredr_capable(struct bthost *bthost)
{
	return lmp_bredr_capable(bthost);
}

void bthost_request_auth(struct bthost *bthost, uint16_t handle)
{
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	if (conn->addr_type == BDADDR_BREDR) {
		struct bt_hci_cmd_auth_requested cp;

		cp.handle = cpu_to_le16(handle);
		send_command(bthost, BT_HCI_CMD_AUTH_REQUESTED, &cp, sizeof(cp));
	} else {
		uint8_t auth_req = bthost->auth_req;

		if (bthost->sc)
			auth_req |= 0x08;

		smp_pair(conn->smp_data, bthost->io_capability, auth_req);
	}
}

void bthost_le_start_encrypt(struct bthost *bthost, uint16_t handle,
							const uint8_t ltk[16])
{
	struct bt_hci_cmd_le_start_encrypt cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.handle = htobs(handle);
	memcpy(cmd.ltk, ltk, 16);

	send_command(bthost, BT_HCI_CMD_LE_START_ENCRYPT, &cmd, sizeof(cmd));
}

uint64_t bthost_conn_get_fixed_chan(struct bthost *bthost, uint16_t handle)
{
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return 0;

	return conn->fixed_chan;
}

void bthost_add_l2cap_server_custom(struct bthost *bthost, uint16_t psm,
				uint16_t mtu, uint16_t mps, uint16_t credits,
				bthost_l2cap_connect_cb func,
				bthost_l2cap_disconnect_cb disconn_func,
				void *user_data)
{
	struct l2cap_conn_cb_data *data;

	data = malloc(sizeof(struct l2cap_conn_cb_data));
	if (!data)
		return;

	data->psm = psm;
	data->mtu = mtu;
	data->mps = mps;
	data->credits = credits;
	data->user_data = user_data;
	data->func = func;
	data->disconn_func = disconn_func;
	data->next = bthost->new_l2cap_conn_data;

	bthost->new_l2cap_conn_data = data;
}

void bthost_add_l2cap_server(struct bthost *bthost, uint16_t psm,
				bthost_l2cap_connect_cb func,
				bthost_l2cap_disconnect_cb disconn_func,
				void *user_data)
{
	bthost_add_l2cap_server_custom(bthost, psm, 0, 0, 0, func,
					disconn_func, user_data);
}

void bthost_set_sc_support(struct bthost *bthost, bool enable)
{
	struct bt_hci_cmd_write_secure_conn_support cmd;

	bthost->sc = enable;

	if (!lmp_bredr_capable(bthost))
		return;

	cmd.support = enable;
	send_command(bthost, BT_HCI_CMD_WRITE_SECURE_CONN_SUPPORT,
							&cmd, sizeof(cmd));
}

void bthost_set_pin_code(struct bthost *bthost, const uint8_t *pin,
							uint8_t pin_len)
{
	memcpy(bthost->pin, pin, pin_len);
	bthost->pin_len = pin_len;
}

void bthost_set_io_capability(struct bthost *bthost, uint8_t io_capability)
{
	bthost->io_capability = io_capability;
}

uint8_t bthost_get_io_capability(struct bthost *bthost)
{
	return bthost->io_capability;
}

void bthost_set_auth_req(struct bthost *bthost, uint8_t auth_req)
{
	bthost->auth_req = auth_req;
}

uint8_t bthost_get_auth_req(struct bthost *bthost)
{
	uint8_t auth_req = bthost->auth_req;

	if (bthost->sc)
		auth_req |= 0x08;

	return auth_req;
}

void bthost_set_reject_user_confirm(struct bthost *bthost, bool reject)
{
	bthost->reject_user_confirm = reject;
}

bool bthost_get_reject_user_confirm(struct bthost *bthost)
{
	return bthost->reject_user_confirm;
}

void bthost_add_rfcomm_server(struct bthost *bthost, uint8_t channel,
				bthost_rfcomm_connect_cb func, void *user_data)
{
	struct rfcomm_conn_cb_data *data;

	data = malloc(sizeof(struct rfcomm_conn_cb_data));
	if (!data)
		return;

	data->channel = channel;
	data->user_data = user_data;
	data->func = func;
	data->next = bthost->new_rfcomm_conn_data;

	bthost->new_rfcomm_conn_data = data;
}

void bthost_start(struct bthost *bthost)
{
	if (!bthost)
		return;

	bthost->ncmd = 1;

	send_command(bthost, BT_HCI_CMD_RESET, NULL, 0);

	send_command(bthost, BT_HCI_CMD_READ_LOCAL_FEATURES, NULL, 0);
	send_command(bthost, BT_HCI_CMD_READ_BD_ADDR, NULL, 0);
}

bool bthost_connect_rfcomm(struct bthost *bthost, uint16_t handle,
				uint8_t channel, bthost_rfcomm_connect_cb func,
				void *user_data)
{
	struct rfcomm_connection_data *data;
	struct bt_l2cap_pdu_conn_req req;
	struct btconn *conn;

	if (bthost->rfcomm_conn_data)
		return false;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return false;

	data = malloc(sizeof(struct rfcomm_connection_data));
	if (!data)
		return false;

	data->channel = channel;
	data->conn = conn;
	data->cb = func;
	data->user_data = user_data;

	bthost->rfcomm_conn_data = data;

	req.psm = cpu_to_le16(0x0003);
	req.scid = cpu_to_le16(conn->next_cid++);

	return bthost_l2cap_req(bthost, handle, BT_L2CAP_PDU_CONN_REQ,
					&req, sizeof(req), NULL, NULL);
}

void bthost_add_rfcomm_chan_hook(struct bthost *bthost, uint16_t handle,
					uint8_t channel,
					bthost_rfcomm_chan_hook_func_t func,
					void *user_data)
{
	struct rfcomm_chan_hook *hook;
	struct btconn *conn;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	hook = malloc(sizeof(*hook));
	if (!hook)
		return;

	memset(hook, 0, sizeof(*hook));

	hook->channel = channel;
	hook->func = func;
	hook->user_data = user_data;

	hook->next = conn->rfcomm_chan_hooks;
	conn->rfcomm_chan_hooks = hook;
}

void bthost_send_rfcomm_data(struct bthost *bthost, uint16_t handle,
					uint8_t channel, const void *data,
					uint16_t len)
{
	struct btconn *conn;
	struct rcconn *rcconn;
	struct rfcomm_hdr *hdr;
	uint8_t *uih_frame;
	uint16_t uih_len;

	conn = bthost_find_conn(bthost, handle);
	if (!conn)
		return;

	rcconn = btconn_find_rfcomm_conn_by_channel(conn, channel);
	if (!rcconn)
		return;

	if (len > 127)
		uih_len = len + sizeof(struct rfcomm_cmd) + sizeof(uint8_t);
	else
		uih_len = len + sizeof(struct rfcomm_cmd);

	uih_frame = malloc(uih_len);
	if (!uih_frame)
		return;

	hdr = (struct rfcomm_hdr *) uih_frame;
	hdr->address = RFCOMM_ADDR(1, channel * 2);
	hdr->control = RFCOMM_CTRL(RFCOMM_UIH, 0);
	if (len > 127) {
		hdr->length  = RFCOMM_LEN16(cpu_to_le16(sizeof(*hdr) + len));
		memcpy(uih_frame + sizeof(*hdr) + 1, data, len);
	} else {
		hdr->length  = RFCOMM_LEN8(sizeof(*hdr) + len);
		memcpy(uih_frame + sizeof(*hdr), data, len);
	}

	uih_frame[uih_len - 1] = rfcomm_fcs((void *)hdr);
	send_acl(bthost, handle, rcconn->scid, false, uih_frame, uih_len);

	free(uih_frame);
}
