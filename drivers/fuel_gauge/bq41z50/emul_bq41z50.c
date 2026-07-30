/**
 * Copyright (c) 2026 Open Device Partnership and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Emulator for bq41z50 fuel gauge
 */

#define DT_DRV_COMPAT ti_bq41z50

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(EMUL_BQ41Z50);

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>

#include "bq41z50.h"

/* Fixed readings. Distinct per register so a test can tell them apart. */
#define BQ41Z50_EMUL_TEMPERATURE_DK    2980
#define BQ41Z50_EMUL_VOLTAGE_MV        14400
#define BQ41Z50_EMUL_CURRENT_MA        (-500)
#define BQ41Z50_EMUL_AVG_CURRENT_MA    (-450)
#define BQ41Z50_EMUL_MAX_ERROR_PCT     2
#define BQ41Z50_EMUL_REL_CHARGE_PCT    87
#define BQ41Z50_EMUL_ABS_CHARGE_PCT    85
#define BQ41Z50_EMUL_REMAINING_CAP_MAH 3500
#define BQ41Z50_EMUL_FULL_CHARGE_MAH   4000
#define BQ41Z50_EMUL_RUNTIME_EMPTY_MIN 420
#define BQ41Z50_EMUL_AVG_EMPTY_MIN     430
#define BQ41Z50_EMUL_CHARGING_CURR_MA  2000
#define BQ41Z50_EMUL_CHARGING_VOLT_MV  16800
#define BQ41Z50_EMUL_BATTERY_STATUS    0x00C0
#define BQ41Z50_EMUL_CYCLE_COUNT       67
#define BQ41Z50_EMUL_DESIGN_CAP_MAH    4000
#define BQ41Z50_EMUL_DESIGN_VOLT_MV    14400
#define BQ41Z50_EMUL_SPEC_INFO         0x0031
#define BQ41Z50_EMUL_MANUFACTURE_DATE  0x5BE1
#define BQ41Z50_EMUL_SERIAL_NUMBER     0x1234
#define BQ41Z50_EMUL_STATE_OF_HEALTH   95
#define BQ41Z50_EMUL_TIME_UNKNOWN      0xFFFF
/* PRES | DSG | SEC=2 (sealed); XCHG clear, so charging is not cut off. */
#define BQ41Z50_EMUL_OPERATION_STATUS  0x00000203

/* Power-on defaults for the host-writable registers. */
#define BQ41Z50_EMUL_DFLT_MFR_ACCESS   0x0001
#define BQ41Z50_EMUL_DFLT_CAP_ALARM    300
#define BQ41Z50_EMUL_DFLT_TIME_ALARM   10
#define BQ41Z50_EMUL_DFLT_BATTERY_MODE 0x0000
#define BQ41Z50_EMUL_DFLT_AT_RATE      0

#define BQ41Z50_EMUL_MAC_CMD_LEN 2

struct bq41z50_emul_cfg {
	uint16_t i2c_addr;
};

/** Registers the host is allowed to write, so reads return what was written. */
struct bq41z50_emul_data {
	uint16_t mfr_access;
	uint16_t remaining_capacity_alarm;
	uint16_t remaining_time_alarm;
	uint16_t battery_mode;
	int16_t at_rate;
};

static int emul_bq41z50_buffer_read(int reg, uint8_t *buf, size_t len)
{
	static const char manufacturer_name[] = "Texas Inst.";
	static const char device_name[] = "bq41z50";
	static const char device_chemistry[] = "LION";
	const char *str;

	switch (reg) {
	case BQ41Z50_MANUFACTURERNAME:
		str = manufacturer_name;
		break;
	case BQ41Z50_DEVICENAME:
		str = device_name;
		break;
	case BQ41Z50_DEVICECHEMISTRY:
		str = device_chemistry;
		break;
	default:
		LOG_ERR("Buffer read for reg 0x%x is not supported", reg);
		return -EIO;
	}

	/* SMBus block read: a length byte followed by the payload. */
	if (len < strlen(str) + 1) {
		return -EIO;
	}

	buf[0] = strlen(str);
	memcpy(&buf[1], str, strlen(str));

	return 0;
}

static int emul_bq41z50_reg_read(const struct emul *target, int reg, uint32_t *val)
{
	const struct bq41z50_emul_data *data = target->data;

	switch (reg) {
	case BQ41Z50_MANUFACTURERACCESS:
		*val = data->mfr_access;
		break;
	case BQ41Z50_REMAININGCAPACITYALARM:
		*val = data->remaining_capacity_alarm;
		break;
	case BQ41Z50_REMAININGTIMEALARM:
		*val = data->remaining_time_alarm;
		break;
	case BQ41Z50_BATTERYMODE:
		*val = data->battery_mode;
		break;
	case BQ41Z50_ATRATE:
		*val = (uint16_t)data->at_rate;
		break;
	case BQ41Z50_ATRATETIMETOFULL:
	case BQ41Z50_ATRATETIMETOEMPTY:
	case BQ41Z50_AVERAGETIMETOFULL:
		*val = BQ41Z50_EMUL_TIME_UNKNOWN;
		break;
	case BQ41Z50_ATRATEOK:
		*val = 0;
		break;
	case BQ41Z50_TEMPERATURE:
		*val = BQ41Z50_EMUL_TEMPERATURE_DK;
		break;
	case BQ41Z50_VOLTAGE:
		*val = BQ41Z50_EMUL_VOLTAGE_MV;
		break;
	case BQ41Z50_CURRENT:
		*val = (uint16_t)BQ41Z50_EMUL_CURRENT_MA;
		break;
	case BQ41Z50_AVERAGECURRENT:
		*val = (uint16_t)BQ41Z50_EMUL_AVG_CURRENT_MA;
		break;
	case BQ41Z50_MAXERROR:
		*val = BQ41Z50_EMUL_MAX_ERROR_PCT;
		break;
	case BQ41Z50_RELATIVESTATEOFCHARGE:
		*val = BQ41Z50_EMUL_REL_CHARGE_PCT;
		break;
	case BQ41Z50_ABSOLUTESTATEOFCHARGE:
		*val = BQ41Z50_EMUL_ABS_CHARGE_PCT;
		break;
	case BQ41Z50_REMAININGCAPACITY:
		*val = BQ41Z50_EMUL_REMAINING_CAP_MAH;
		break;
	case BQ41Z50_FULLCHARGECAPACITY:
		*val = BQ41Z50_EMUL_FULL_CHARGE_MAH;
		break;
	case BQ41Z50_RUNTIMETOEMPTY:
		*val = BQ41Z50_EMUL_RUNTIME_EMPTY_MIN;
		break;
	case BQ41Z50_AVERAGETIMETOEMPTY:
		*val = BQ41Z50_EMUL_AVG_EMPTY_MIN;
		break;
	case BQ41Z50_CHARGINGCURRENT:
		*val = BQ41Z50_EMUL_CHARGING_CURR_MA;
		break;
	case BQ41Z50_CHARGINGVOLTAGE:
		*val = BQ41Z50_EMUL_CHARGING_VOLT_MV;
		break;
	case BQ41Z50_BATTERYSTATUS:
		*val = BQ41Z50_EMUL_BATTERY_STATUS;
		break;
	case BQ41Z50_CYCLECOUNT:
		*val = BQ41Z50_EMUL_CYCLE_COUNT;
		break;
	case BQ41Z50_DESIGNCAPACITY:
		*val = BQ41Z50_EMUL_DESIGN_CAP_MAH;
		break;
	case BQ41Z50_DESIGNVOLTAGE:
		*val = BQ41Z50_EMUL_DESIGN_VOLT_MV;
		break;
	case BQ41Z50_SPECIFICATIONINFO:
		*val = BQ41Z50_EMUL_SPEC_INFO;
		break;
	case BQ41Z50_MANUFACTURERDATE:
		*val = BQ41Z50_EMUL_MANUFACTURE_DATE;
		break;
	case BQ41Z50_SERIALNUMBER:
		*val = BQ41Z50_EMUL_SERIAL_NUMBER;
		break;
	case BQ41Z50_STATEOFHEALTHSOH:
		*val = BQ41Z50_EMUL_STATE_OF_HEALTH;
		break;
	case BQ41Z50_OPERATIONSTATUS:
		*val = BQ41Z50_EMUL_OPERATION_STATUS;
		break;
	default:
		LOG_ERR("Unknown register 0x%x read", reg);
		return -EIO;
	}

	return 0;
}

static int emul_bq41z50_reg_write(const struct emul *target, int reg, uint16_t val)
{
	struct bq41z50_emul_data *data = target->data;

	switch (reg) {
	case BQ41Z50_MANUFACTURERACCESS:
		data->mfr_access = val;
		break;
	case BQ41Z50_REMAININGCAPACITYALARM:
		data->remaining_capacity_alarm = val;
		break;
	case BQ41Z50_REMAININGTIMEALARM:
		data->remaining_time_alarm = val;
		break;
	case BQ41Z50_BATTERYMODE:
		data->battery_mode = val;
		break;
	case BQ41Z50_ATRATE:
		data->at_rate = (int16_t)val;
		break;
	default:
		LOG_ERR("Register 0x%x is not writable", reg);
		return -EIO;
	}

	return 0;
}

static int emul_bq41z50_read(const struct emul *target, int reg, uint8_t *buf, size_t len)
{
	uint32_t val;
	int rc;

	switch (reg) {
	case BQ41Z50_MANUFACTURERNAME:
	case BQ41Z50_DEVICENAME:
	case BQ41Z50_DEVICECHEMISTRY:
		return emul_bq41z50_buffer_read(reg, buf, len);
	default:
		break;
	}

	if (len > sizeof(val)) {
		LOG_ERR("Read of %zu bytes from reg 0x%x is too wide", len, reg);
		return -EIO;
	}

	rc = emul_bq41z50_reg_read(target, reg, &val);
	if (rc) {
		return rc;
	}

	/* Return only as many bytes as the host asked for, little endian. */
	for (size_t i = 0; i < len; i++) {
		buf[i] = (uint8_t)(val >> (8U * i));
	}

	return 0;
}

/*
 * ManufacturerBlockAccess writes arrive as an SMBus block write: the 0x44 command, a length byte,
 * and then the subcommand. Only the framing is checked, since none of the subcommands the driver
 * issues have an observable effect on the emulated register file.
 */
static int emul_bq41z50_mac_write(const struct emul *target, struct i2c_msg *msgs)
{
	uint8_t payload_len;
	uint16_t cmd;

	if (msgs[1].len != 1 || msgs[2].len != BQ41Z50_EMUL_MAC_CMD_LEN) {
		LOG_ERR("Malformed manufacturer block access write");
		return -EIO;
	}

	payload_len = msgs[1].buf[0];
	if (payload_len != BQ41Z50_EMUL_MAC_CMD_LEN) {
		LOG_ERR("Unexpected block length %u", payload_len);
		return -EIO;
	}

	cmd = sys_get_le16(msgs[2].buf);
	LOG_DBG("Manufacturer block access command 0x%04x", cmd);

	return 0;
}

static int bq41z50_emul_transfer_i2c(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
				     int addr)
{
	const struct bq41z50_emul_cfg *cfg = target->cfg;
	int reg;

	__ASSERT_NO_MSG(msgs && num_msgs);

	if (addr != cfg->i2c_addr) {
		LOG_ERR("I2C address (0x%2x) is not supported.", addr);
		return -EIO;
	}

	i2c_dump_msgs_rw(target->dev, msgs, num_msgs, addr, false);

	if (msgs[0].flags & I2C_MSG_READ) {
		LOG_ERR("Transfer must start with a register address write");
		return -EIO;
	}
	if (msgs[0].len != 1) {
		LOG_ERR("Unexpected addr length %d", msgs[0].len);
		return -EIO;
	}
	reg = msgs[0].buf[0];

	switch (num_msgs) {
	case 2:
		if (msgs[1].flags & I2C_MSG_READ) {
			return emul_bq41z50_read(target, reg, msgs[1].buf, msgs[1].len);
		}
		if (msgs[1].len != sizeof(uint16_t)) {
			LOG_ERR("Unexpected write length %d for reg 0x%x", msgs[1].len, reg);
			return -EIO;
		}
		return emul_bq41z50_reg_write(target, reg, sys_get_le16(msgs[1].buf));
	case 3:
		if (reg != BQ41Z50_MANUFACTURERBLOCKACCESS) {
			LOG_ERR("Unexpected 3-message transfer to reg 0x%x", reg);
			return -EIO;
		}
		return emul_bq41z50_mac_write(target, msgs);
	default:
		LOG_ERR("Invalid number of messages: %d", num_msgs);
		return -EIO;
	}
}

static const struct i2c_emul_api bq41z50_emul_api_i2c = {
	.transfer = bq41z50_emul_transfer_i2c,
};

/**
 * Set up a new emulator (I2C)
 *
 * @param target Emulation information
 * @param parent Device to emulate
 * @return 0 indicating success (always)
 */
static int emul_bq41z50_init(const struct emul *target, const struct device *parent)
{
	struct bq41z50_emul_data *data = target->data;

	ARG_UNUSED(parent);

	data->mfr_access = BQ41Z50_EMUL_DFLT_MFR_ACCESS;
	data->remaining_capacity_alarm = BQ41Z50_EMUL_DFLT_CAP_ALARM;
	data->remaining_time_alarm = BQ41Z50_EMUL_DFLT_TIME_ALARM;
	data->battery_mode = BQ41Z50_EMUL_DFLT_BATTERY_MODE;
	data->at_rate = BQ41Z50_EMUL_DFLT_AT_RATE;

	return 0;
}

/*
 * Main instantiation macro.
 */
#define BQ41Z50_EMUL(n)                                                                            \
	static const struct bq41z50_emul_cfg bq41z50_emul_cfg_##n = {                              \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
	};                                                                                         \
	static struct bq41z50_emul_data bq41z50_emul_data_##n;                                     \
	EMUL_DT_INST_DEFINE(n, emul_bq41z50_init, &bq41z50_emul_data_##n, &bq41z50_emul_cfg_##n,   \
			    &bq41z50_emul_api_i2c, NULL)

DT_INST_FOREACH_STATUS_OKAY(BQ41Z50_EMUL)
