// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012  Instituto Nokia de Tecnologia - INdT
 *  Copyright (C) 2014  Google Inc.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>

#include "lib/bluetooth.h"
#include "lib/hci.h"
#include "lib/sdp.h"
#include "lib/uuid.h"

#include "src/shared/util.h"
#include "src/shared/att.h"
#include "src/shared/queue.h"
#include "src/shared/gatt-db.h"
#include "src/shared/gatt-client.h"
#include "src/plugin.h"
#include "src/adapter.h"
#include "src/device.h"
#include "src/profile.h"
#include "src/service.h"
#include "src/log.h"

#define GAP_UUID16 0x1800

/* Generic Attribute/Access Service */
struct gas {
	struct btd_device *device;
	struct gatt_db *db;
	struct bt_gatt_client *client;
	struct gatt_db_attribute *attr;
};

static void gas_reset(struct gas *gas)
{
	gas->attr = NULL;
	gatt_db_unref(gas->db);
	gas->db = NULL;
	bt_gatt_client_unref(gas->client);
	gas->client = NULL;
}

static void gas_free(struct gas *gas)
{
	gas_reset(gas);
	btd_device_unref(gas->device);
	g_free(gas);
}

static char *name2utf8(const uint8_t *name, uint16_t len)
{
	char utf8_name[HCI_MAX_NAME_LENGTH + 2];

	len = MIN(len, sizeof(utf8_name) - 1);

	memset(utf8_name, 0, sizeof(utf8_name));
	strncpy(utf8_name, (char *) name, len);
	strtoutf8(utf8_name, len);

	/* Remove leading and trailing whitespace characters */
	g_strstrip(utf8_name);

	return g_strdup(utf8_name);
}

static void read_device_name_cb(bool success, uint8_t att_ecode,
					const uint8_t *value, uint16_t length,
					void *user_data)
{
	struct gas *gas = user_data;
	char *name;

	if (!success) {
		DBG("Reading device name failed with ATT error: %u",
								att_ecode);
		return;
	}

	if (!length)
		return;

	name = name2utf8(value, length);

	DBG("GAP Device Name: %s", name);

	btd_device_device_set_name(gas->device, name);

	g_free(name);
}

static void handle_device_name(struct gas *gas, uint16_t value_handle)
{
	if (!bt_gatt_client_read_long_value(gas->client, value_handle, 0,
						read_device_name_cb, gas, NULL))
		DBG("Failed to send request to read device name");
}

static void read_appearance_cb(bool success, uint8_t att_ecode,
					const uint8_t *value, uint16_t length,
					void *user_data)
{
	struct gas *gas = user_data;
	uint16_t appearance;

	if (!success) {
		DBG("Reading appearance failed with ATT error: %u", att_ecode);
		return;
	}

	/* The appearance value is a 16-bit unsigned integer */
	if (length != 2) {
		DBG("Malformed appearance value");
		return;
	}

	appearance = get_le16(value);

	DBG("GAP Appearance: 0x%04x", appearance);

	device_set_appearance(gas->device, appearance);
}

static void handle_appearance(struct gas *gas, uint16_t value_handle)
{
	uint16_t value;

	if (!device_get_appearance(gas->device, &value))
		return;

	if (!bt_gatt_client_read_value(gas->client, value_handle,
						read_appearance_cb, gas, NULL))
		DBG("Failed to send request to read appearance");
}

static void read_ppcp_cb(bool success, uint8_t att_ecode,
			const uint8_t *value, uint16_t length,
			void *user_data)
{
	struct gas *gas = user_data;
	uint16_t min_interval, max_interval, latency, timeout, max_latency;

	if (!success) {
		DBG("Reading PPCP failed with ATT error: %u", att_ecode);
		return;
	}

	if (length != 8) {
		DBG("Malformed PPCP value");
		return;
	}

	min_interval = get_le16(&value[0]);
	max_interval = get_le16(&value[2]);
	latency = get_le16(&value[4]);
	timeout = get_le16(&value[6]);

	/* 0xffff indicates no specific min/max */
	if (min_interval == 0xffff)
		min_interval = 0x0018; /* 30.0ms */

	if (max_interval == 0xffff)
		max_interval = 0x0028; /* 50.0ms */

	DBG("GAP Peripheral Preferred Connection Parameters:");
	DBG("\tMinimum connection interval: %u", min_interval);
	DBG("\tMaximum connection interval: %u", max_interval);
	DBG("\tSlave latency: %u", latency);
	DBG("\tConnection Supervision timeout multiplier: %u", timeout);

	/* avoid persisting connection parameters that are not valid */
	if (min_interval > max_interval ||
	    min_interval < 6 || max_interval > 3200) {
		warn("GAS PPCP: Invalid Connection Parameters values");
		return;
	}

	if (timeout < 10 || timeout > 3200) {
		warn("GAS PPCP: Invalid Connection Parameters values");
		return;
	}

	if (max_interval >= timeout * 8) {
		warn("GAS PPCP: Invalid Connection Parameters values");
		return;
	}

	max_latency = (timeout * 4 / max_interval) - 1;
	if (latency > 499 || latency > max_latency) {
		warn("GAS PPCP: Invalid Connection Parameters values");
		return;
	}

	btd_device_set_conn_param(gas->device, min_interval, max_interval,
					latency, timeout);
}

static void handle_ppcp(struct gas *gas, uint16_t value_handle)
{
	if (!bt_gatt_client_read_value(gas->client, value_handle,
						read_ppcp_cb, gas, NULL))
		DBG("Failed to send request to read PPCP");
}

static inline bool uuid_cmp(uint16_t u16, const bt_uuid_t *uuid)
{
	bt_uuid_t lhs;

	bt_uuid16_create(&lhs, u16);

	return bt_uuid_cmp(&lhs, uuid) == 0;
}

static void handle_characteristic(struct gatt_db_attribute *attr,
								void *user_data)
{
	struct gas *gas = user_data;
	uint16_t value_handle;
	bt_uuid_t uuid;

	if (!gatt_db_attribute_get_char_data(attr, NULL, &value_handle, NULL,
								NULL, &uuid)) {
		error("Failed to obtain characteristic data");
		return;
	}

	if (uuid_cmp(GATT_CHARAC_DEVICE_NAME, &uuid))
		handle_device_name(gas, value_handle);
	else if (uuid_cmp(GATT_CHARAC_APPEARANCE, &uuid))
		handle_appearance(gas, value_handle);
	else if (uuid_cmp(GATT_CHARAC_PERIPHERAL_PREF_CONN, &uuid))
		handle_ppcp(gas, value_handle);
	else {
		char uuid_str[MAX_LEN_UUID_STR];

		/* TODO: Support peripheral privacy feature */

		bt_uuid_to_string(&uuid, uuid_str, sizeof(uuid_str));
		DBG("Unsupported characteristic: %s", uuid_str);
	}
}

static int gap_probe(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);
	struct gas *gas = btd_service_get_user_data(service);
	char addr[18];

	ba2str(device_get_address(device), addr);
	DBG("GAP profile probe (%s)", addr);

	/* Ignore, if we were probed for this device already */
	if (gas) {
		error("Profile probed twice for the same device!");
		return -1;
	}

	gas = g_new0(struct gas, 1);
	if (!gas)
		return -1;

	gas->device = btd_device_ref(device);
	btd_service_set_user_data(service, gas);

	return 0;
}

static void gap_remove(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);
	struct gas *gas;
	char addr[18];

	ba2str(device_get_address(device), addr);
	DBG("GAP profile remove (%s)", addr);

	gas = btd_service_get_user_data(service);
	if (!gas) {
		error("GAP service not handled by profile");
		return;
	}

	gas_free(gas);
}

static void foreach_gap_service(struct gatt_db_attribute *attr, void *user_data)
{
	struct gas *gas = user_data;

	if (gas->attr) {
		error("More than one GAP service exists for this device");
		return;
	}

	gas->attr = attr;
	gatt_db_service_foreach_char(gas->attr, handle_characteristic, gas);
}

static int gap_accept(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);
	struct gatt_db *db = btd_device_get_gatt_db(device);
	struct bt_gatt_client *client = btd_device_get_gatt_client(device);
	struct gas *gas = btd_service_get_user_data(service);
	char addr[18];
	bt_uuid_t gap_uuid;
	int err = 0;

	ba2str(device_get_address(device), addr);
	DBG("GAP profile accept (%s)", addr);

	if (!gas) {
		error("GAP service not handled by profile");
		err = -1;
		goto _finish;
	}

	/* Check if attribute already has been discovered */
	if (gas->attr)
		goto _finish;

	gas->db = gatt_db_ref(db);
	gas->client = bt_gatt_client_clone(client);

	/* Handle the GAP services */
	bt_uuid16_create(&gap_uuid, GAP_UUID16);
	gatt_db_foreach_service(db, &gap_uuid, foreach_gap_service, gas);

	if (!gas->attr) {
		error("GAP attribute not found");
		gas_reset(gas);
		err = -1;
	}

_finish:

	btd_service_connecting_complete(service, err);

	return err;
}

static int gap_disconnect(struct btd_service *service)
{
	btd_service_disconnecting_complete(service, 0);

	return 0;
}

static struct btd_profile gap_profile = {
	.name		= "gap-profile",
	.remote_uuid	= GAP_UUID,
	.device_probe	= gap_probe,
	.device_remove	= gap_remove,
	.accept		= gap_accept,
	.disconnect	= gap_disconnect,
};

static int gap_init(void)
{
	return btd_profile_register(&gap_profile);
}

static void gap_exit(void)
{
	btd_profile_unregister(&gap_profile);
}

BLUETOOTH_PLUGIN_DEFINE(gap, VERSION, BLUETOOTH_PLUGIN_PRIORITY_DEFAULT,
							gap_init, gap_exit)
