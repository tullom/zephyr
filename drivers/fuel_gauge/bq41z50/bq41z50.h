/**
 * Copyright (c) 2026 Open Device Partnership and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_FUELGAUGE_BQ41Z50_GAUGE_H_
#define ZEPHYR_DRIVERS_FUELGAUGE_BQ41Z50_GAUGE_H_

#include <zephyr/drivers/i2c.h>

enum bq41z50_regs {
	BQ41Z50_MANUFACTURERACCESS = 0x00,      /* R/W */
	BQ41Z50_REMAININGCAPACITYALARM = 0x01,  /* R/W, Unit: mAh/10mWh, Range: 0..65535 */
	BQ41Z50_REMAININGTIMEALARM = 0x02,      /* R/W, Unit: minutes, Range: 0..65535 */
	BQ41Z50_BATTERYMODE = 0x03,             /* R/W, Unit: ---, Range: 0x0000..0xFFFF */
	BQ41Z50_ATRATE = 0x04,                  /* R/W, Unit: mA/10mW, Range: -32768..32767 */
	BQ41Z50_ATRATETIMETOFULL = 0x05,        /* R/O, Unit: minutes, Range: 0..65535 */
	BQ41Z50_ATRATETIMETOEMPTY = 0x06,       /* R/O, Unit: minutes, Range: 0..65535 */
	BQ41Z50_ATRATEOK = 0x07,                /* R/O, Unit: ---, Range: 0..65535 */
	BQ41Z50_TEMPERATURE = 0x08,             /* R/O, Unit: 0.1 K, Range: 0..65535 */
	BQ41Z50_VOLTAGE = 0x09,                 /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_CURRENT = 0x0A,                 /* R/O, Unit: mA, Range: -32768..32767 */
	BQ41Z50_AVERAGECURRENT = 0x0B,          /* R/O, Unit: mA, Range: -32768..32767 */
	BQ41Z50_MAXERROR = 0x0C,                /* R/O, Unit: percent, Range: 0..100 */
	BQ41Z50_RELATIVESTATEOFCHARGE = 0x0D,   /* R/O, Unit: percent, Range: 0..100 */
	BQ41Z50_ABSOLUTESTATEOFCHARGE = 0x0E,   /* R/O, Unit: percent, Range: 0..100 */
	BQ41Z50_REMAININGCAPACITY = 0x0F,       /* R/O, Unit: mAh/10mWh, Range: 0..65535 */
	BQ41Z50_FULLCHARGECAPACITY = 0x10,      /* R/O, Unit: mAh/10mWh, Range: 0..65535 */
	BQ41Z50_RUNTIMETOEMPTY = 0x11,          /* R/O, Unit: minutes, Range: 0..65535 */
	BQ41Z50_AVERAGETIMETOEMPTY = 0x12,      /* R/O, Unit: minutes, Range: 0..65535 */
	BQ41Z50_AVERAGETIMETOFULL = 0x13,       /* R/O, Unit: minutes, Range: 0..65535 */
	BQ41Z50_CHARGINGCURRENT = 0x14,         /* R/O, Unit: mA, Range: 0..65535 */
	BQ41Z50_CHARGINGVOLTAGE = 0x15,         /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_BATTERYSTATUS = 0x16,           /* R/O, Unit: ---, Range: --- */
	BQ41Z50_CYCLECOUNT = 0x17,              /* R/O, Unit: cycles, Range: 0..65535 */
	BQ41Z50_DESIGNCAPACITY = 0x18,          /* R/O, Unit: mAh/10mWh, Range: 0..65535 */
	BQ41Z50_DESIGNVOLTAGE = 0x19,           /* R/O, Unit: mV, Range: 7000..18000 */
	BQ41Z50_SPECIFICATIONINFO = 0x1A,       /* R/O, Unit: ---, Range: --- */
	BQ41Z50_MANUFACTURERDATE = 0x1B,        /* R/O, Unit: ---, Range: 0..65535 */
	BQ41Z50_SERIALNUMBER = 0x1C,            /* R/O, Unit: ---, Range: 0..65535 */
	BQ41Z50_MANUFACTURERNAME = 0x20,        /* R/O, Unit: ASCII, Range: --- */
	BQ41Z50_DEVICENAME = 0x21,              /* R/O, Unit: ASCII, Range: --- */
	BQ41Z50_DEVICECHEMISTRY = 0x22,         /* R/O, Unit: ASCII, Range: --- */
	BQ41Z50_MANUFACTURERDATA = 0x23,        /* R/O, Unit: ---, Range: --- */
	BQ41Z50_AUTHENTICATE = 0x2F,            /* R/W, Unit: ---, Range: --- */
	BQ41Z50_CELLVOLTAGE4 = 0x3C,            /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_CELLVOLTAGE3 = 0x3D,            /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_CELLVOLTAGE2 = 0x3E,            /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_CELLVOLTAGE1 = 0x3F,            /* R/O, Unit: mV, Range: 0..65535 */
	BQ41Z50_MANUFACTURERBLOCKACCESS = 0x44, /* R/W */
	BQ41Z50_BTPDISCHARGESET = 0x4A,         /* R/W, Unit: mAh, Range: 150..65535 */
	BQ41Z50_BTPCHARGESET = 0x4B,            /* R/W, Unit: mAh, Range: 175..65535 */
	BQ41Z50_STATEOFHEALTHSOH = 0x4F,        /* R/O, Unit: percent, Range: 0..100 */
	BQ41Z50_SAFETYALERT = 0x50,             /* Cannot read in Sealed Mode */
	BQ41Z50_SAFETYSTATUS = 0x51,            /* Cannot read in Sealed Mode */
	BQ41Z50_PFALERT = 0x52,                 /* Cannot read in Sealed Mode */
	BQ41Z50_PFSTATUS = 0x53,                /* Cannot read in Sealed Mode */
	BQ41Z50_OPERATIONSTATUS = 0x54,         /* Cannot read in Sealed Mode */
	BQ41Z50_CHARGINGSTATUS = 0x55,          /* Cannot read in Sealed Mode */
	BQ41Z50_GAUGINGSTATUS = 0x56,           /* Cannot read in Sealed Mode */
	BQ41Z50_MANUFACTURINGSTATUS = 0x57,     /* Cannot read in Sealed Mode */
	BQ41Z50_AFEREG = 0x58,                  /* Cannot read in Sealed Mode */
	BQ41Z50_MAXTURBOPWR = 0x59,             /* R/W, Unit: cW, Range: --- */
	BQ41Z50_SUSTURBOPWR = 0x5A,             /* R/W, Unit: cW, Range: --- */
	BQ41Z50_TURBOPACKR = 0x5B,              /* R/W, Unit: mOhm, Range: --- */
	BQ41Z50_TURBOSYSR = 0x5C,               /* R/W, Unit: mOhm, Range: --- */
	BQ41Z50_TURBOEDV = 0x5D,                /* R/W, Unit: mV, Range: --- */
	BQ41Z50_MAXTURBOCURR = 0x5E,            /* R/W, Unit: mA, Range: --- */
	BQ41Z50_SUSTURBOCURR = 0x5F,            /* R/W, Unit: mA, Range: --- */
	BQ41Z50_LIFETIMEDATABLOCK1 = 0x60,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK2 = 0x61,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK3 = 0x62,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK4 = 0x63,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK5 = 0x64,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK6 = 0x65,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK7 = 0x66,      /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK8 = 0x67,      /* Cannot read in Sealed Mode */
	BQ41Z50_TURBORHFEFFECTIVE = 0x68,       /* R/O, Unit: mOhm, Range: --- */
	BQ41Z50_TURBOVLOAD = 0x69,              /* R/O, Unit: mV, Range: --- */
	BQ41Z50_LIFETIMEDATABLOCK11 = 0x6A,     /* Cannot read in Sealed Mode */
	BQ41Z50_LIFETIMEDATABLOCK12 = 0x6B,     /* Cannot read in Sealed Mode */
	BQ41Z50_DASTATUS1 = 0x71,               /* Cannot read in Sealed Mode */
	BQ41Z50_DASTATUS2 = 0x72,               /* Cannot read in Sealed Mode */
	BQ41Z50_GAUGESTATUS1 = 0x73,            /* Cannot read in Sealed Mode */
	BQ41Z50_GAUGESTATUS2 = 0x74,            /* Cannot read in Sealed Mode */
	BQ41Z50_GAUGESTATUS3 = 0x75,            /* Cannot read in Sealed Mode */
	BQ41Z50_CBSTATUS = 0x76,                /* Cannot read in Sealed Mode */
	BQ41Z50_STATEOFHEALTH = 0x77,           /* Cannot read in Sealed Mode */
	BQ41Z50_FILTERCAPACITY = 0x78           /* Cannot read in Sealed Mode */
};

#endif
