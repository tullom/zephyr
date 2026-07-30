/**
 * Copyright (c) 2026 Open Device Partnership and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_bq41z50

#include "bq41z50.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <errno.h>

LOG_MODULE_REGISTER(BQ41Z50);

/* ManufacturerBlockAccess (0x44) subcommands */
#define BQ41Z50_MAC_CMD_DEVICE_TYPE  0x0001
#define BQ41Z50_MAC_CMD_FIRMWARE_VER 0x0002
#define BQ41Z50_MAC_CMD_SHUTDOWNMODE 0x0010
#define BQ41Z50_MAC_CMD_SLEEPMODE    0x0011
#define BQ41Z50_MAC_CMD_GAUGING      0x0021

/* BatteryMode (0x03) CAPACITY_MODE: set means capacity registers report 10 mWh instead of mAh */
#define BQ41Z50_BATTERYMODE_CAPM_BIT 15

/* OperationStatus (0x54) XCHG: set means charging is disabled */
#define BQ41Z50_OPERATIONSTATUS_XCHG_BIT 14

struct bq41z50_config {
	struct i2c_dt_spec i2c;
};

struct bq41z50_data {
	/* Mirrors BatteryMode CAPACITY_MODE; see bq41z50_track_capacity_mode(). */
	bool capacity_in_10mwh;
};

static int bq41z50_i2c_read(const struct device *dev, uint8_t reg_addr, uint8_t *value, size_t len)
{
	const struct bq41z50_config *cfg = dev->config;
	int ret = i2c_burst_read_dt(&cfg->i2c, reg_addr, value, len);

	if (ret) {
		LOG_ERR("i2c_burst_read_dt failed for address 0x%02x: %d", reg_addr, ret);
	}
	return ret;
}

static int bq41z50_i2c_write(const struct device *dev, uint8_t reg_addr, uint8_t *value, size_t len)
{
	const struct bq41z50_config *cfg = dev->config;
	int ret = i2c_burst_write_dt(&cfg->i2c, reg_addr, value, len);

	if (ret) {
		LOG_ERR("i2c_burst_write_dt failed for address 0x%02x: %d", reg_addr, ret);
	}
	return ret;
}

static int bq41z50_read_u8(const struct device *dev, uint8_t reg_addr, uint8_t *value)
{
	return bq41z50_i2c_read(dev, reg_addr, value, sizeof(*value));
}

static int bq41z50_read_u16(const struct device *dev, uint8_t reg_addr, uint16_t *value)
{
	uint8_t buf[sizeof(uint16_t)];
	int ret = bq41z50_i2c_read(dev, reg_addr, buf, sizeof(buf));

	if (ret == 0) {
		*value = sys_get_le16(buf);
	}
	return ret;
}

static int bq41z50_read_u32(const struct device *dev, uint8_t reg_addr, uint32_t *value)
{
	uint8_t buf[sizeof(uint32_t)];
	int ret = bq41z50_i2c_read(dev, reg_addr, buf, sizeof(buf));

	if (ret == 0) {
		*value = sys_get_le32(buf);
	}
	return ret;
}

static int bq41z50_write_u16(const struct device *dev, uint8_t reg_addr, uint16_t value)
{
	uint8_t buf[sizeof(uint16_t)];

	sys_put_le16(value, buf);
	return bq41z50_i2c_write(dev, reg_addr, buf, sizeof(buf));
}

/*
 * The gauge reports RemainingCapacity and FullChargeCapacity in either mAh or 10 mWh depending on
 * BatteryMode CAPACITY_MODE, but the API expresses both in uAh. CAPACITY_MODE is only changed by a
 * host write, so mirror it whenever BatteryMode passes through the driver and reject the affected
 * properties while the gauge is in 10 mWh mode rather than reporting a wrong unit.
 */
static void bq41z50_track_capacity_mode(const struct device *dev, uint16_t battery_mode)
{
	struct bq41z50_data *data = dev->data;

	data->capacity_in_10mwh = IS_BIT_SET(battery_mode, BQ41Z50_BATTERYMODE_CAPM_BIT);
}

static int bq41z50_read_capacity_uah(const struct device *dev, uint8_t reg_addr, uint32_t *value)
{
	const struct bq41z50_data *data = dev->data;
	uint16_t tmp_val;
	int ret;

	if (data->capacity_in_10mwh) {
		return -ENOTSUP;
	}

	ret = bq41z50_read_u16(dev, reg_addr, &tmp_val);
	if (ret == 0) {
		/* convert mAh to uAh */
		*value = tmp_val * 1000U;
	}
	return ret;
}

static int bq41z50_i2c_write_mfr_blk_access(const struct device *dev, uint16_t cmd, uint8_t *value,
					    size_t len)
{
	const struct bq41z50_config *cfg = dev->config;
	/* Manufacturer Block Access (0x44) is standard for bq4xzxy family. */
	uint8_t mac_cmd = BQ41Z50_MANUFACTURERBLOCKACCESS;
	uint8_t cmd_le[sizeof(cmd)];
	struct i2c_msg msg[4];
	uint8_t total_len = sizeof(cmd) + len;

	sys_put_le16(cmd, cmd_le);

	/* As per Datasheet, use SMBus block protocol to write/read using
	 * Manufacturer Block Access (0x44).
	 * SMBus block write requires writing command followed by number of
	 * bytes that will follow and then actual bytes to write.
	 */
	msg[0].buf = &mac_cmd;
	msg[0].len = 1U;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = &total_len;
	msg[1].len = 1U;
	msg[1].flags = I2C_MSG_WRITE;

	msg[2].buf = cmd_le;
	msg[2].len = sizeof(cmd_le);
	msg[2].flags = I2C_MSG_WRITE;

	msg[3].buf = value;
	msg[3].len = len;
	msg[3].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	/* For battery cutoff, we only need to send the command. */
	int ret = i2c_transfer_dt(&cfg->i2c, msg, ((value == NULL) ? 3 : 4));

	if (ret) {
		LOG_ERR("i2c_transfer_dt returned %d", ret);
	}
	return ret;
}

static int bq41z50_battery_cutoff(const struct device *dev)
{
	int ret;

	/*
	 * As per TRM, in order to enter shutdown mode we need to send
	 * BQ41Z50_MAC_CMD_SHUTDOWNMODE twice irrespective of access mode. The first command arms
	 * the shutdown sequence and the second confirms it.
	 */
	ret = bq41z50_i2c_write_mfr_blk_access(dev, BQ41Z50_MAC_CMD_SHUTDOWNMODE, NULL, 0);
	if (ret) {
		return ret;
	}

	return bq41z50_i2c_write_mfr_blk_access(dev, BQ41Z50_MAC_CMD_SHUTDOWNMODE, NULL, 0);
}

static int bq41z50_get_buffer_prop(const struct device *dev, fuel_gauge_prop_t prop_type, void *dst,
				   size_t dst_len)
{
	int ret;

	if (dst == NULL) {
		return -EINVAL;
	}

	switch (prop_type) {
	case FUEL_GAUGE_MANUFACTURER_NAME: {
		struct sbs_gauge_manufacturer_name *mfgname = dst;

		if (dst_len != sizeof(*mfgname)) {
			return -EINVAL;
		}
		ret = bq41z50_i2c_read(dev, BQ41Z50_MANUFACTURERNAME, dst, dst_len);
		if (ret == 0) {
			mfgname->manufacturer_name[mfgname->manufacturer_name_length] = '\0';
		}
		break;
	}

	case FUEL_GAUGE_DEVICE_NAME: {
		struct sbs_gauge_device_name *devname = dst;

		if (dst_len != sizeof(*devname)) {
			return -EINVAL;
		}
		ret = bq41z50_i2c_read(dev, BQ41Z50_DEVICENAME, dst, dst_len);
		if (ret == 0) {
			devname->device_name[devname->device_name_length] = '\0';
		}
		break;
	}

	case FUEL_GAUGE_DEVICE_CHEMISTRY: {
		struct sbs_gauge_device_chemistry *devchem = dst;

		if (dst_len != sizeof(*devchem)) {
			return -EINVAL;
		}
		ret = bq41z50_i2c_read(dev, BQ41Z50_DEVICECHEMISTRY, dst, dst_len);
		if (ret == 0) {
			devchem->device_chemistry[devchem->device_chemistry_length] = '\0';
		}
		break;
	}

	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int bq41z50_set_prop(const struct device *dev, fuel_gauge_prop_t prop,
			    union fuel_gauge_prop_val val)
{
	int ret;

	switch (prop) {
	case FUEL_GAUGE_SBS_REMAINING_CAPACITY_ALARM:
		ret = bq41z50_write_u16(dev, BQ41Z50_REMAININGCAPACITYALARM,
					val.sbs_remaining_capacity_alarm);
		break;

	case FUEL_GAUGE_SBS_REMAINING_TIME_ALARM_MINS:
		ret = bq41z50_write_u16(dev, BQ41Z50_REMAININGTIMEALARM,
					val.sbs_remaining_time_alarm_mins);
		break;

	case FUEL_GAUGE_SBS_MODE:
		ret = bq41z50_write_u16(dev, BQ41Z50_BATTERYMODE, val.sbs_mode);
		if (ret == 0) {
			bq41z50_track_capacity_mode(dev, val.sbs_mode);
		}
		break;

	case FUEL_GAUGE_SBS_ATRATE:
		ret = bq41z50_write_u16(dev, BQ41Z50_ATRATE, (uint16_t)val.sbs_at_rate);
		break;

	case FUEL_GAUGE_SBS_MFR_ACCESS:
		ret = bq41z50_write_u16(dev, BQ41Z50_MANUFACTURERACCESS, val.sbs_mfr_access_word);
		break;

	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int bq41z50_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
			    union fuel_gauge_prop_val *val)
{
	uint32_t tmp_u32 = 0;
	uint16_t tmp_val = 0;
	uint8_t tmp_u8 = 0;
	int ret;

	switch (prop) {
	case FUEL_GAUGE_AVG_CURRENT_UA:
		ret = bq41z50_read_u16(dev, BQ41Z50_AVERAGECURRENT, &tmp_val);
		/* convert mA to uA */
		val->avg_current_ua = (int16_t)tmp_val * 1000;
		break;

	case FUEL_GAUGE_CURRENT_UA:
		ret = bq41z50_read_u16(dev, BQ41Z50_CURRENT, &tmp_val);
		/* convert mA to uA */
		val->current_ua = (int16_t)tmp_val * 1000;
		break;

	case FUEL_GAUGE_CHARGE_CUTOFF:
		ret = bq41z50_read_u32(dev, BQ41Z50_OPERATIONSTATUS, &tmp_u32);
		val->cutoff = IS_BIT_SET(tmp_u32, BQ41Z50_OPERATIONSTATUS_XCHG_BIT);
		break;

	case FUEL_GAUGE_CYCLE_COUNT:
		ret = bq41z50_read_u16(dev, BQ41Z50_CYCLECOUNT, &tmp_val);
		val->cycle_count = tmp_val;
		break;

	case FUEL_GAUGE_FULL_CHARGE_CAPACITY_UAH:
		ret = bq41z50_read_capacity_uah(dev, BQ41Z50_FULLCHARGECAPACITY,
						&val->full_charge_capacity_uah);
		break;

	case FUEL_GAUGE_REMAINING_CAPACITY_UAH:
		ret = bq41z50_read_capacity_uah(dev, BQ41Z50_REMAININGCAPACITY,
						&val->remaining_capacity_uah);
		break;

	case FUEL_GAUGE_RUNTIME_TO_EMPTY_MINS:
		ret = bq41z50_read_u16(dev, BQ41Z50_RUNTIMETOEMPTY, &tmp_val);
		val->runtime_to_empty_mins = tmp_val;
		break;

	case FUEL_GAUGE_RUNTIME_TO_FULL_MINS:
		ret = bq41z50_read_u16(dev, BQ41Z50_AVERAGETIMETOFULL, &tmp_val);
		val->runtime_to_full_mins = tmp_val;
		break;

	case FUEL_GAUGE_SBS_MFR_ACCESS:
		ret = bq41z50_read_u16(dev, BQ41Z50_MANUFACTURERACCESS, &tmp_val);
		val->sbs_mfr_access_word = tmp_val;
		break;

	case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE_PCT:
		ret = bq41z50_read_u8(dev, BQ41Z50_ABSOLUTESTATEOFCHARGE, &tmp_u8);
		val->absolute_state_of_charge_pct = tmp_u8;
		break;

	case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE_PCT:
		ret = bq41z50_read_u8(dev, BQ41Z50_RELATIVESTATEOFCHARGE, &tmp_u8);
		val->relative_state_of_charge_pct = tmp_u8;
		break;

	case FUEL_GAUGE_TEMPERATURE_DK:
		ret = bq41z50_read_u16(dev, BQ41Z50_TEMPERATURE, &tmp_val);
		val->temperature_dk = tmp_val;
		break;

	case FUEL_GAUGE_VOLTAGE_UV:
		ret = bq41z50_read_u16(dev, BQ41Z50_VOLTAGE, &tmp_val);
		/* convert mV to uV */
		val->voltage_uv = tmp_val * 1000;
		break;

	case FUEL_GAUGE_SBS_MODE:
		ret = bq41z50_read_u16(dev, BQ41Z50_BATTERYMODE, &tmp_val);
		if (ret == 0) {
			bq41z50_track_capacity_mode(dev, tmp_val);
		}
		val->sbs_mode = tmp_val;
		break;

	case FUEL_GAUGE_CHARGE_CURRENT_UA:
		ret = bq41z50_read_u16(dev, BQ41Z50_CHARGINGCURRENT, &tmp_val);
		/* convert mA to uA */
		val->chg_current_ua = tmp_val * 1000;
		break;

	case FUEL_GAUGE_CHARGE_VOLTAGE_UV:
		ret = bq41z50_read_u16(dev, BQ41Z50_CHARGINGVOLTAGE, &tmp_val);
		/* convert mV to uV */
		val->chg_voltage_uv = tmp_val * 1000;
		break;

	case FUEL_GAUGE_STATUS:
		ret = bq41z50_read_u16(dev, BQ41Z50_BATTERYSTATUS, &tmp_val);
		val->fg_status = tmp_val;
		break;

	case FUEL_GAUGE_DESIGN_CAPACITY:
		/* API expresses this as mAh or 10 mWh, matching CAPACITY_MODE, so pass it on. */
		ret = bq41z50_read_u16(dev, BQ41Z50_DESIGNCAPACITY, &tmp_val);
		val->design_cap = tmp_val;
		break;

	case FUEL_GAUGE_DESIGN_VOLTAGE_MV:
		ret = bq41z50_read_u16(dev, BQ41Z50_DESIGNVOLTAGE, &tmp_val);
		val->design_volt_mv = tmp_val;
		break;

	case FUEL_GAUGE_SBS_ATRATE:
		ret = bq41z50_read_u16(dev, BQ41Z50_ATRATE, &tmp_val);
		val->sbs_at_rate = (int16_t)tmp_val;
		break;

	case FUEL_GAUGE_SBS_ATRATE_TIME_TO_FULL_MINS:
		ret = bq41z50_read_u16(dev, BQ41Z50_ATRATETIMETOFULL, &tmp_val);
		val->sbs_at_rate_time_to_full_mins = tmp_val;
		break;

	case FUEL_GAUGE_SBS_ATRATE_TIME_TO_EMPTY_MINS:
		ret = bq41z50_read_u16(dev, BQ41Z50_ATRATETIMETOEMPTY, &tmp_val);
		val->sbs_at_rate_time_to_empty_mins = tmp_val;
		break;

	case FUEL_GAUGE_SBS_ATRATE_OK:
		ret = bq41z50_read_u16(dev, BQ41Z50_ATRATEOK, &tmp_val);
		val->sbs_at_rate_ok = tmp_val;
		break;

	case FUEL_GAUGE_SBS_REMAINING_CAPACITY_ALARM:
		ret = bq41z50_read_u16(dev, BQ41Z50_REMAININGCAPACITYALARM, &tmp_val);
		val->sbs_remaining_capacity_alarm = tmp_val;
		break;

	case FUEL_GAUGE_SBS_REMAINING_TIME_ALARM_MINS:
		ret = bq41z50_read_u16(dev, BQ41Z50_REMAININGTIMEALARM, &tmp_val);
		val->sbs_remaining_time_alarm_mins = tmp_val;
		break;

	case FUEL_GAUGE_STATE_OF_HEALTH:
		ret = bq41z50_read_u8(dev, BQ41Z50_STATEOFHEALTHSOH, &tmp_u8);
		val->state_of_health = tmp_u8;
		break;

	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int bq41z50_init(const struct device *dev)
{
	const struct bq41z50_config *cfg = dev->config;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	return 0;
}

static DEVICE_API(fuel_gauge,
		  bq41z50_driver_api) = {.get_property = &bq41z50_get_prop,
					 .get_buffer_property = &bq41z50_get_buffer_prop,
					 .set_property = &bq41z50_set_prop,
					 .battery_cutoff = &bq41z50_battery_cutoff};

#define BQ41Z50_INIT(inst)                                                                         \
	static const struct bq41z50_config bq41z50_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	static struct bq41z50_data bq41z50_data_##inst;                                            \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &bq41z50_init, NULL, &bq41z50_data_##inst,                     \
			      &bq41z50_config_##inst, POST_KERNEL,                                 \
			      CONFIG_FUEL_GAUGE_INIT_PRIORITY, &bq41z50_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BQ41Z50_INIT)
