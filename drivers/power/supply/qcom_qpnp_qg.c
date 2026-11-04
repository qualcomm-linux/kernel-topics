// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/nvmem-consumer.h>
#include <linux/firmware.h>
#include <linux/iio/iio.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/alarmtimer.h>
#include <linux/rtc.h>
#include <linux/pm_wakeup.h>
#include <linux/atomic.h>
#include <linux/unaligned.h>
#define FORCE_S2_MODE_BIT		BIT(2)
#define QGAUGE_QG_STATE_FORCE_CMD	0xE7
static int qg_ss_feature;
#define FULL_SOC			100
#define BUCKET_COUNT			8
#define BUCKET_SOC_PCT			(256 / BUCKET_COUNT)
#define BATT_PROFILE_NAME_MAX		64
#define DEFAULT_UPDATE_TIME_MS		64000
#define BATT_SOC_32BIT			GENMASK(31, 0)
static int qg_delta_soc_cold_interval_ms = 4000;
static int qg_maint_soc_update_ms = 120000;
static int qg_fvss_delta_soc_interval_ms = 10000;
#define QG_BATTERY_MAGIC		0x51474247
#define SOC_SCALE_HYST_MS		2000
#define QG_LAST_ADC_V_DATA0_REG		0xC0
#define QG_LAST_ADC_I_DATA0_REG		0xC2
#define CENTI_FULL_SOC			10000
/* Battery profile ioctl helpers */
#define UV_TO_DECIUV(uv)		((uv) / 100)
#define DECIUV_TO_UV(duv)		((duv) * 100)
#define DEGC_SCALE			10
#define CAP(min, max, val)		clamp_t(int, (val), (min), (max))
#define is_between(left, right, value)					\
	(((left) >= (right) && (left) >= (value) &&			\
	  (value) >= (right)) ||					\
	  ((left) <= (right) && (left) <= (value) &&			\
	   (value) <= (right)))
#define QG_MIN_SOC			0
#define QG_MAX_SOC			10000
#define QG_MIN_OCV_UV_BP		2500000
#define QG_MAX_OCV_UV_BP		5000000
#define QG_MIN_FCC_MAH			0
#define QG_MAX_FCC_MAH			10000
#define QG_MIN_VAR			0
#define QG_MAX_VAR			100000
#define QG_MIN_SLOPE			0
#define QG_MAX_SLOPE			100000
#define QG_S2_NORMAL_AVG_V_DATA0_REG	0x80
#define QG_S2_NORMAL_AVG_I_DATA0_REG	0x82
#define QG_SOC_MONOTONIC_REG		0xBF
struct batt_profile_header {
	u32 magic;
	u32 version;
	u32 batt_id_kohm;
	u32 float_volt_uv;
	u32 nom_cap_uah;
	u32 fastchg_curr_ma;
	u32 num_ctables;
	u32 reserved;
};
enum qg_lut_type {
	QG_TABLE_FCC1 = 0,
	QG_TABLE_FCC2 = 1,
	QG_TABLE_V1   = 2,
	QG_TABLE_V2   = 3,
	QG_TABLE_Z1   = 4,
	QG_TABLE_Z2   = 5,
	QG_TABLE_Z3   = 6,
	QG_TABLE_Z4   = 7,
	QG_TABLE_Z5   = 8,
	QG_TABLE_Z6   = 9,
	QG_TABLE_Y1   = 10,
	QG_TABLE_Y2   = 11,
	QG_TABLE_Y3   = 12,
	QG_TABLE_Y4   = 13,
	QG_TABLE_Y5   = 14,
	QG_TABLE_Y6   = 15,
	QG_TABLE_MAX  = 16,
};
enum soc_scaling_feature {
	QG_FVSS = BIT(0),
	QG_TCSS = BIT(1),
	QG_BASS = BIT(2),
};
struct qg_lut_table {
	int		type;
	int		num_cols;
	int		num_rows;
	int		*col_temps;
	int		*row_socs;
	int		*data;
};
struct battery_data_table {
	int		temperature;
	int		nrows;
	int		ncols;
	int		*conv;
	int		*data;
};
struct battery_data_table0 {
	int		soc_length;
	int		ocv_length;
	int		*soc;
	int		*ocv;
};
struct batt_table_entry {
	int	soc;
	int	ocv_uv;
};
enum battery_table_type {
	TABLE_SOC_OCV1 = 0,
	TABLE_SOC_OCV2,
	TABLE_FCC1,
	TABLE_FCC2,
	TABLE_Z1,
	TABLE_Z2,
	TABLE_Z3,
	TABLE_Z4,
	TABLE_Z5,
	TABLE_Z6,
	TABLE_Y1,
	TABLE_Y2,
	TABLE_Y3,
	TABLE_Y4,
	TABLE_Y5,
	TABLE_Y6,
	TABLE_MAX,
};
struct profile_table_data {
	char		*name;
	int		rows;
	int		cols;
	int		*row_entries;
	int		*col_entries;
	int		**data;
};
struct qg_battery_data {
	dev_t			dev_no;
	struct class		*battery_class;
	struct device		*battery_device;
	struct cdev		battery_cdev;
	struct device_node	*profile_node;
	struct profile_table_data profile[TABLE_MAX];
};
#define PERPH_TYPE_REG			0x04
#define PERPH_SUBTYPE_REG		0x05
#define QG_TYPE				0x0D
#define QG_STATUS1_REG			0x08
#define QG_OK_BIT			BIT(7)
#define BATTERY_PRESENT_BIT		BIT(1)
#define QG_STATUS2_REG			0x09
#define GOOD_OCV_BIT			BIT(1)
#define BATTERY_MISSING_BIT		BIT(3)
#define VBAT_LOW_INT_RT_STS_BIT		BIT(1)
#define BATTERY_MISSING_INT_RT_STS_BIT	BIT(0)
#define QG_DATA_CTL1_REG		0x41
#define MASTER_HOLD_OR_CLR_BIT		BIT(0)
#define QG_MODE_CTL1_REG		0x43
#define PARALLEL_IBAT_SENSE_EN_BIT	BIT(7)
#define QG_S2_NORMAL_MEAS_CTL2_REG	0x51
#define FIFO_LENGTH_MASK		GENMASK(5, 3)
#define FIFO_LENGTH_SHIFT		3
#define NUM_OF_ACCUM_MASK		GENMASK(2, 0)
#define QG_MODE_CTL2_REG		0x44
#define VI_MODE_BIT			BIT(0)
#define QG_S5_OCV_VALIDATE_MEAS_CTL1_REG	0x60
#define ALLOW_S5_BIT			BIT(7)
#define QG_S7_PON_OCV_MEAS_CTL1_REG		0x64
#define ADC_CONV_DLY_MASK			GENMASK(3, 0)
#define QG_S2_NORMAL_MEAS_CTL3_REG	0x52
#define QG_STATUS3_REG			0x0A
#define QG_VBAT_EMPTY_THRESHOLD_REG	0x4B
#define QG_VBAT_LOW_THRESHOLD_REG	0x4C
#define QG_V_FIFO0_DATA0_REG		0x90
#define QG_I_FIFO0_DATA0_REG		0xA0
#define QG_S3_SLEEP_OCV_MEAS_CTL4_REG		0x59
#define S3_SLEEP_OCV_TIMER_MASK			GENMASK(2, 0)
#define QG_S3_SLEEP_OCV_TREND_CTL2_REG		0x5C
#define TREND_TOL_MASK				GENMASK(5, 0)
#define QG_S3_SLEEP_OCV_IBAT_CTL1_REG		0x5D
#define SLEEP_IBAT_QUALIFIED_LENGTH_MASK	GENMASK(2, 0)
#define QG_S3_ENTRY_IBAT_THRESHOLD_REG		0x5E
#define QG_S3_EXIT_IBAT_THRESHOLD_REG		0x5F
#define QG_ACCUM_CNT_RT_REG		0x8E
#define QG_V_ACCUM_DATA0_RT_REG		0x88
#define QG_I_ACCUM_DATA0_RT_REG		0x8B
#define QG_S7_PON_OCV_V_DATA0_REG	0x70
#define QG_S3_GOOD_OCV_V_DATA0_REG	0x74
#define QG_LAST_S3_SLEEP_V_DATA0_REG	0xCC
#define VBAT_LOW_HYST_UV			50000
#define MAX_FIFO_LENGTH			16
#define V_RAW_TO_UV(V_RAW)		div_u64(((u64)V_RAW * 194637ULL), 1000)
#define FIFO_V_RESET_VAL		0x8000
#define FIFO_I_RESET_VAL		0x8000
#define QG_MIN_OCV_UV			2000000
#define QG_MAX_OCV_UV			5000000
#define QG_SOC_FULL	10000
#define COUNT_FIFO_RT_MASK			GENMASK(3, 0)
#define SDAM_TYPE			0x2E
#define SDAM_MIN_OFFSET			0x45
#define SDAM_MAX_OFFSET			0xB3
#define QG_SDAM_VALID_OFFSET			0x46
#define QG_SDAM_SOC_OFFSET			0x47
#define QG_SDAM_TEMP_OFFSET			0x48
#define QG_SDAM_RBAT_OFFSET			0x4A
#define QG_SDAM_OCV_OFFSET			0x4C
#define QG_SDAM_IBAT_OFFSET			0x50
#define QG_SDAM_TIME_OFFSET			0x54
#define QG_SDAM_CYCLE_COUNT_OFFSET		0x58
#define QG_SDAM_LEARNED_CAPACITY_OFFSET		0x68
#define QG_SDAM_BATT_AGE_LEVEL_OFFSET		0x76
#define QG_SDAM_MAGIC_OFFSET			0x80
#define QG_SDAM_FLASH_OCV_OFFSET		0x84
#define QG_SDAM_MAX_OFFSET			0xA4
#define QG_SDAM_PON_OCV_OFFSET			0xBC
#define QG_ADC_IBAT_5A			0x3
#define QG_ADC_IBAT_10A			0x4
#define DEFAULT_S2_FIFO_LENGTH		5
#define DEFAULT_S2_VBAT_LOW_LENGTH	2
#define DEFAULT_S2_ACC_LENGTH		128
#define DEFAULT_S2_ACC_INTVL_MS		100
#define DEFAULT_SLEEP_S2_FIFO_LENGTH	8
#define DEFAULT_SLEEP_S2_ACC_LENGTH	256
#define DEFAULT_SLEEP_S2_ACC_INTVL_MS	200
#define DEFAULT_FAST_CHG_S2_FIFO_LENGTH	1
#define QG_SLEEP_EXIT_TIME_MS		15000
#define QG_INT_LATCHED_STS_REG		0x18
#define FIFO_UPDATE_DONE_INT_LAT_STS_BIT	BIT(3)
#define DEBUG_BATT_ID_LOW		6000
#define DEBUG_BATT_ID_HIGH		8500
#define DEBUG_BATT_SOC			67
#define BATT_MISSING_SOC		50
#define QG_INT_RT_STS_REG		0x10
enum qg_mode {
	QG_V_I_MODE,
	QG_V_MODE,
};
enum qg_version {
	QG_PMIC5,
	QG_LITE,
};
enum pmic_version {
	PM4125,
};
struct ocv_all {
	u32 ocv_uv;
	u32 ocv_raw;
	char ocv_type[20];
};
enum ocv_type {
	S7_PON_OCV,
	S3_GOOD_OCV,
	S3_LAST_OCV,
	SDAM_PON_OCV,
	PON_OCV_MAX,
};
enum qg {
	QG_SOC,
	QG_OCV_UV,
	QG_RBAT_MOHM,
	QG_PON_OCV_UV,
	QG_GOOD_OCV_UV,
	QG_FIFO_TIME_DELTA,
	QG_BATT_SOC,
	QG_CC_SOC,
	QG_FULL_SOC,
	QG_CLEAR_LEARNT_DATA,
	QG_SYS_SOC,
	QG_V_IBAT,
	QG_MAX,
};
enum qg_wa_flags {
	QG_VBAT_LOW_WA = BIT(0),
	QG_RECHARGE_SOC_WA = BIT(1),
	QG_CLK_ADJUST_WA = BIT(2),
	QG_PON_OCV_WA = BIT(3),
};
enum qg_sdam_param {
	SDAM_VALID,
	SDAM_SOC,
	SDAM_TEMP,
	SDAM_RBAT_MOHM,
	SDAM_OCV_UV,
	SDAM_IBAT_UA,
	SDAM_TIME_SEC,
	SDAM_PON_OCV_UV,
	SDAM_MAGIC,
	SDAM_BATT_AGE_LEVEL,
	SDAM_FLASH_OCV,
	SDAM_MAX,
};
enum s2_state {
	S2_FAST_CHARGING = BIT(0),
	S2_LOW_VBAT = BIT(1),
	S2_SLEEP = BIT(2),
	S2_DEFAULT = BIT(3),
};
struct qg_config {
	enum qg_version		qg_version;
	enum pmic_version	pmic_version;
};
static const struct qg_config config[] = {
	[PM4125]	= {QG_LITE, PM4125},
};
struct qg_irq {
	const char		*name;
	irqreturn_t		(*handler)(int irq, void *data);
	bool			wake;
	int			irq;
};
struct qg_batt_props {
	const char		*batt_type_str;
	int			float_volt_uv;
	int			vbatt_full_mv;
	int			fastchg_curr_ma;
	int			qg_profile_version;
};
struct qg_dt_props {
	int			vbatt_empty_mv;
	int			vbatt_empty_cold_mv;
	int			vbatt_low_mv;
	int			vbatt_low_cold_mv;
	int			vbatt_cutoff_mv;
	int			cold_temp_threshold;
	int			s2_fifo_length;
	int			s2_acc_length;
	int			s2_acc_intvl_ms;
	int			ocv_timer_expiry_min;
	int			ocv_tol_threshold_uv;
	int			delta_soc;
	int			iterm_ma;
	int			ignore_shutdown_soc_secs;
	int			shutdown_temp_diff;
	int			tcss_entry_soc;
	bool			hold_soc_while_full;
	int			rbat_conn_mohm;
	int			shutdown_soc_threshold;
	int			sys_min_volt_mv;
	bool			use_s7_ocv;
	bool			fvss_enable;
	int			fvss_vbat_mv;
	bool			tcss_enable;
	bool			bass_enable;
	bool			qg_ext_sense;
	bool			use_cp_iin_sns;
	bool			qg_sleep_config;
	int			min_sleep_time_secs;
	bool			qg_fast_chg_cfg;
	int			fast_chg_s2_fifo_length;
	int			sleep_s2_fifo_length;
	int			sleep_s2_acc_length;
	int			sleep_s2_acc_intvl_ms;
	int			s2_vbat_low_fifo_length;
	int			s3_entry_fifo_length;
	int			s3_entry_ibat_ua;
	int			s3_exit_ibat_ua;
};
struct qg_fifo_data {
	unsigned int		v;
	int			i;
	unsigned int		interval;
	unsigned int		count;
};
struct qg_param {
	unsigned int			data;
	bool				valid;
};
struct qg_kernel_data {
	unsigned int			seq_no;
	unsigned int			fifo_time;
	unsigned int			fifo_length;
	struct qg_fifo_data		fifo[MAX_FIFO_LENGTH];
	struct qg_param			param[QG_MAX];
};
struct qg_user_data {
	struct qg_param			param[QG_MAX];
};
struct qg_sdam {
	struct regmap		*regmap;
	u16			sdam_base;
};
struct qpnp_qg {
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*qg_psy;
	struct power_supply	*chg_psy;
	struct iio_channel	*batt_id_chan;
	struct iio_channel	*batt_therm_chan;
	struct notifier_block	nb;
	struct work_struct	qg_status_change_work;
	struct work_struct	udata_work;
	struct qg_batt_props	bp;
	struct qg_dt_props	dt;
	int			soc_reporting_ready;
	struct nvmem_cell	*soc_cell;
	unsigned long		last_maint_soc_update_time;
	struct work_struct	scale_soc_work;
	int			last_adj_ssoc;
	u32			qg_base;
	u32			wa_flags;
	enum qg_version		qg_version;
	enum pmic_version	pmic_version;
	enum qg_mode		qg_mode;
	u8			qg_subtype;
	struct qg_kernel_data	kdata;
	struct qg_user_data	udata;
	struct alarm		alarm_timer;
	u32			batt_id_ohm;
	bool			battery_missing;
	bool			profile_loaded;
	int			batt_soc;
	int			msoc;
	int			charge_status;
	int			charge_type;
	bool			charge_done;
	int			next_wakeup_ms;
	int			maint_soc;
	int			cc_soc;
	int			full_soc;
	int			sys_soc;
	int			max_fcc_limit_ma;
	bool			first_profile_load;
	int			chg_iterm_ma;
	u32			max_fifo_length;
	int			last_fifo_v_uv;
	int			last_fifo_i_ua;
	u32			seq_no;
	u32			sdam_data[SDAM_MAX];
	struct qg_fifo_data	fifo[MAX_FIFO_LENGTH];
	int			fifo_length;
	ktime_t			last_fifo_update_time;
	int			recharge_soc;
	struct device_node	*batt_node;
	int			*soc_table;
	int			*ocv_table;
	int			table_len;
	int			float_volt_uv;
	int			fastchg_curr_ma;
	bool			fvss_active;
	u32			s2_state;
	u32			s2_state_mask;
	struct mutex		data_lock;
	struct mutex		soc_lock;
	struct mutex		bus_lock;
	int			qg_v_ibat;
	int			catch_up_soc;
	const struct firmware		*batt_fw;
	struct battery_data_table0	table0[2];
	struct battery_data_table	**bp_charge_tables;
	struct battery_data_table	**bp_discharge_tables;
	int				num_ctables;
	int				num_dtables;
	u32				nom_cap_uah;
	struct qg_lut_table *lut_tables[QG_TABLE_MAX];
	int num_lut_tables;
	dev_t			dev_no;
	struct cdev		qg_cdev;
	struct class		*qg_class;
	struct device		*qg_device;
	bool			data_ready;
	wait_queue_head_t	qg_wait_q;
	struct power_supply		*cp_psy;
	bool				parallel_enabled;
	bool				usb_present;
	bool				dc_present;
	ktime_t				last_user_update_time;
	bool			qg_device_open;
	struct cycle_counter	*counter;
	struct iio_dev		*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel	**int_iio_chans;
	struct iio_channel	**ext_iio_chans;
	int			batt_age_level;
	struct power_supply		*usb_psy;
	struct power_supply		*dc_psy;
	bool			suspend_data;
	bool			vbat_low;
	int			fifo_done_count;
	struct delayed_work	qg_sleep_exit_work;
	unsigned long		suspend_time;
	bool			force_soc;
	u32			vbat_fvss_entry;
	u32			soc_fvss_entry;
	int			soc_tcss;
	int			tcss_entry_count;
	bool			tcss_active;
	int			soc_tcss_entry;
	int			ibat_tcss_entry;
	int			prev_fifo_i_ua;
	bool			bass_active;
	int			bsoc_bass_entry;
	struct wakeup_source	*qg_ws;
	atomic_t		wake_voter_udata_ready;
	atomic_t		wake_voter_fifo_done;
	atomic_t		wake_voter_fifo_rt_done;
	atomic_t		wake_voter_suspend_data;
	atomic_t		wake_voter_good_ocv;
	atomic_t		wake_voter_sleep_exit_data;
	atomic_t		wake_voter_sleep_exit;
	atomic_t		good_ocv_irq_disable_cnt;
	struct qg_sdam		sdam;
	struct qg_battery_data	*battery;
};
static int qg_fvss_vbat_scaling = 1;
struct qg_sdam_info {
	const char		*name;
	u32			offset;
	u32			length;
};
#define BPIOCXSOC	_IOWR('B', 0x01, struct battery_params)
#define BPIOCXOCV	_IOWR('B', 0x02, struct battery_params)
#define BPIOCXFCC	_IOWR('B', 0x03, struct battery_params)
#define BPIOCXSLOPE	_IOWR('B', 0x04, struct battery_params)
#define BPIOCXVAR	_IOWR('B', 0x05, struct battery_params)
struct tables {
	int		table_index;
	const char	*table_name;
};
static const struct tables batt_table[] = {
	{TABLE_SOC_OCV1, "qcom,pc-temp-v1-lut"},
	{TABLE_SOC_OCV2, "qcom,pc-temp-v2-lut"},
	{TABLE_FCC1,     "qcom,fcc1-temp-lut"},
	{TABLE_FCC2,     "qcom,fcc2-temp-lut"},
	{TABLE_Z1,       "qcom,pc-temp-z1-lut"},
	{TABLE_Z2,       "qcom,pc-temp-z2-lut"},
	{TABLE_Z3,       "qcom,pc-temp-z3-lut"},
	{TABLE_Z4,       "qcom,pc-temp-z4-lut"},
	{TABLE_Z5,       "qcom,pc-temp-z5-lut"},
	{TABLE_Z6,       "qcom,pc-temp-z6-lut"},
	{TABLE_Y1,       "qcom,pc-temp-y1-lut"},
	{TABLE_Y2,       "qcom,pc-temp-y2-lut"},
	{TABLE_Y3,       "qcom,pc-temp-y3-lut"},
	{TABLE_Y4,       "qcom,pc-temp-y4-lut"},
	{TABLE_Y5,       "qcom,pc-temp-y5-lut"},
	{TABLE_Y6,       "qcom,pc-temp-y6-lut"},
};
enum qg_ext_iio_channels {
	INPUT_CURRENT_LIMITED = 0,
	RECHARGE_SOC,
	FORCE_RECHARGE,
	CHARGE_DONE,
	PARALLEL_CHARGING_ENABLED,
	CP_CHARGING_ENABLED,
};
static const char * const qg_ext_iio_chan_name[] = {
	[INPUT_CURRENT_LIMITED]	= "input_current_limited",
	[RECHARGE_SOC]			= "recharge_soc",
	[FORCE_RECHARGE]		= "force_recharge",
	[CHARGE_DONE]			= "charge_done",
	[PARALLEL_CHARGING_ENABLED]	= "parallel_charging_enabled",
	[CP_CHARGING_ENABLED]		= "cp_charging_enabled",
};
struct qg_iio_channels {
	const char		*datasheet_name;
	int			channel_num;
	enum iio_chan_type	type;
	long			info_mask;
};
#define QG_IIO_CHAN(_name, _num, _type, _mask)	\
{						\
	.datasheet_name = _name,		\
	.channel_num    = _num,			\
	.type           = _type,		\
	.info_mask      = _mask,		\
},
#define QG_CHAN_VOLT(_name, _num)    \
	QG_IIO_CHAN(_name, _num, IIO_VOLTAGE,    BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_CUR(_name, _num)     \
	QG_IIO_CHAN(_name, _num, IIO_CURRENT,    BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_RES(_name, _num)     \
	QG_IIO_CHAN(_name, _num, IIO_RESISTANCE, BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_TEMP(_name, _num)    \
	QG_IIO_CHAN(_name, _num, IIO_TEMP,       BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_POW(_name, _num)     \
	QG_IIO_CHAN(_name, _num, IIO_POWER,      BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_ENERGY(_name, _num)  \
	QG_IIO_CHAN(_name, _num, IIO_ENERGY,     BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_INDEX(_name, _num)   \
	QG_IIO_CHAN(_name, _num, IIO_INDEX,      BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_ACT(_name, _num)     \
	QG_IIO_CHAN(_name, _num, IIO_ACTIVITY,   BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_TSTAMP(_name, _num)  \
	QG_IIO_CHAN(_name, _num, IIO_TIMESTAMP,  BIT(IIO_CHAN_INFO_PROCESSED))
#define QG_CHAN_COUNT(_name, _num)   \
	QG_IIO_CHAN(_name, _num, IIO_COUNT,      BIT(IIO_CHAN_INFO_PROCESSED))
static const struct qg_iio_channels qg_iio_psy_channels[] = {
	QG_CHAN_ENERGY("capacity",			PSY_IIO_CAPACITY)
	QG_CHAN_ENERGY("capacity_raw",			PSY_IIO_CAPACITY_RAW)
	QG_CHAN_ENERGY("real_capacity",			PSY_IIO_REAL_CAPACITY)
	QG_CHAN_TEMP("temp",				PSY_IIO_TEMP)
	QG_CHAN_VOLT("voltage_now",			PSY_IIO_VOLTAGE_NOW)
	QG_CHAN_VOLT("voltage_ocv",			PSY_IIO_VOLTAGE_OCV)
	QG_CHAN_CUR("current_now",			PSY_IIO_CURRENT_NOW)
	QG_CHAN_RES("resistance",			PSY_IIO_RESISTANCE)
	QG_CHAN_RES("resistance_id",			PSY_IIO_RESISTANCE_ID)
	QG_CHAN_ACT("soc_reporting_ready",		PSY_IIO_SOC_REPORTING_READY)
	QG_CHAN_RES("resistance_capacitive",		PSY_IIO_RESISTANCE_CAPACITIVE)
	QG_CHAN_INDEX("debug_battery",			PSY_IIO_DEBUG_BATTERY)
	QG_CHAN_VOLT("voltage_min",			PSY_IIO_VOLTAGE_MIN)
	QG_CHAN_VOLT("voltage_max",			PSY_IIO_VOLTAGE_MAX)
	QG_CHAN_CUR("batt_full_current",		PSY_IIO_BATT_FULL_CURRENT)
	QG_CHAN_INDEX("batt_profile_version",		PSY_IIO_BATT_PROFILE_VERSION)
	QG_CHAN_ENERGY("charge_full",			PSY_IIO_CHARGE_FULL)
	QG_CHAN_ENERGY("charge_full_design",		PSY_IIO_CHARGE_FULL_DESIGN)
	QG_CHAN_ENERGY("cc_soc",			PSY_IIO_CC_SOC)
	QG_CHAN_ACT("fg_reset",				PSY_IIO_FG_RESET)
	QG_CHAN_VOLT("voltage_avg",			PSY_IIO_VOLTAGE_AVG)
	QG_CHAN_CUR("current_avg",			PSY_IIO_CURRENT_AVG)
	QG_CHAN_POW("power_avg",			PSY_IIO_POWER_AVG)
	QG_CHAN_POW("power_now",			PSY_IIO_POWER_NOW)
	QG_CHAN_ACT("scale_mode_en",			PSY_IIO_SCALE_MODE_EN)
	QG_CHAN_INDEX("batt_age_level",			PSY_IIO_BATT_AGE_LEVEL)
	QG_CHAN_ACT("fg_type",				PSY_IIO_FG_TYPE)
};
struct battery_params {
	int	soc;
	int	ocv_uv;
	int	fcc_mah;
	int	slope;
	int	var;
	int	batt_temp;
	int	table_index;
};
static struct qg_sdam_info sdam_info[] = {
	[SDAM_VALID] = {
		.name	= "VALID",
		.offset = QG_SDAM_VALID_OFFSET,
		.length = 1,
	},
	[SDAM_SOC] = {
		.name	= "SOC",
		.offset = QG_SDAM_SOC_OFFSET,
		.length = 1,
	},
	[SDAM_TEMP] = {
		.name	= "BATT_TEMP",
		.offset = QG_SDAM_TEMP_OFFSET,
		.length = 2,
	},
	[SDAM_RBAT_MOHM] = {
		.name	= "RBAT_MOHM",
		.offset = QG_SDAM_RBAT_OFFSET,
		.length = 2,
	},
	[SDAM_OCV_UV] = {
		.name	= "OCV_UV",
		.offset = QG_SDAM_OCV_OFFSET,
		.length = 4,
	},
	[SDAM_IBAT_UA] = {
		.name	= "IBAT_UA",
		.offset = QG_SDAM_IBAT_OFFSET,
		.length = 4,
	},
	[SDAM_TIME_SEC] = {
		.name	= "TIME_SEC",
		.offset = QG_SDAM_TIME_OFFSET,
		.length = 4,
	},
	[SDAM_PON_OCV_UV] = {
		.name	= "SDAM_PON_OCV",
		.offset = QG_SDAM_PON_OCV_OFFSET,
		.length = 2,
	},
	[SDAM_MAGIC] = {
		.name	= "SDAM_MAGIC_OFFSET",
		.offset = QG_SDAM_MAGIC_OFFSET,
		.length = 4,
	},
	[SDAM_FLASH_OCV] = {
		.name	= "SDAM_FLASH_OCV_OFFSET",
		.offset = QG_SDAM_FLASH_OCV_OFFSET,
		.length = 1,
	},
};
static int qg_delta_soc_interval_ms = 20000;
/* Forward declarations */
static int qg_read(struct qpnp_qg *chip, u32 addr, u8 *val, int len);
static int qg_write(struct qpnp_qg *chip, u32 addr, u8 *val, int len);
static int qg_masked_write(struct qpnp_qg *chip, u32 addr, u8 mask, u8 val);
static int get_fifo_length(struct qpnp_qg *chip, u32 *fifo_length, bool rt);
static int get_sample_interval(struct qpnp_qg *chip, u32 *sample_interval);
static int get_sample_count(struct qpnp_qg *chip, u32 *sample_count);
static int qg_determine_pon_soc(struct qpnp_qg *chip);
static int qg_get_batt_id(struct qpnp_qg *chip);
static int qg_store_soc_params(struct qpnp_qg *chip);
static int qg_get_battery_current(struct qpnp_qg *chip, int *curr_ua);
static int qg_get_battery_voltage(struct qpnp_qg *chip, int *volt_uv);
static int qg_get_battery_capacity(struct qpnp_qg *chip, int *capacity);
static int qg_get_battery_temp(struct qpnp_qg *chip, int *temp);
static int qg_sdam_read(struct qpnp_qg *chip, u8 param, u32 *data);
static int qg_sdam_write(struct qpnp_qg *chip, u8 param, u32 data);
static int qg_sdam_clear(struct qpnp_qg *chip);
static int qg_process_fifo(struct qpnp_qg *chip, u32 fifo_length);
static int qg_master_hold(struct qpnp_qg *chip, bool hold);
static int qg_process_rt_fifo(struct qpnp_qg *chip);
static int qg_read_ocv(struct qpnp_qg *chip, u32 *ocv_uv, u32 *ocv_raw,
		u8 type);
static irqreturn_t qg_fifo_update_done_handler(int irq, void *data);
static irqreturn_t qg_good_ocv_handler(int irq, void *data);
static irqreturn_t qg_vbat_empty_handler(int irq, void *data);
static int qg_sdam_read_all(struct qpnp_qg *chip, u32 *sdam_data);
static int get_rtc_time(unsigned long *rtc_time);
static int qg_config_s2_state(struct qpnp_qg *chip,
		enum s2_state requested_state, bool state_enable,
		bool process_fifo);
static int qg_soc_init(struct qpnp_qg *chip);
static s64 qg_iraw_to_ua(struct qpnp_qg *chip, int iraw);
static int lookup_soc_ocv(struct qpnp_qg *chip, u32 *soc, u32 ocv_uv, int batt_temp, bool charging);
static int qg_adjust_sys_soc(struct qpnp_qg *chip);
static int get_fifo_done_time(struct qpnp_qg *chip, bool rt, int *time_ms);
static int qg_scale_soc(struct qpnp_qg *chip, bool force_soc);
static bool is_debug_batt_id(struct qpnp_qg *chip);
static int qg_vbat_thresholds_config(struct qpnp_qg *chip);
/* IRQ table - must be before suspend/resume functions that reference it */
static struct qg_irq qg_irqs[] = {
	{
		.name    = "qg-fifo-done",
		.handler = qg_fifo_update_done_handler,
		.wake    = true,
	},
	{
		.name    = "qg-good-ocv",
		.handler = qg_good_ocv_handler,
		.wake    = true,
	},
	{
		.name    = "qg-vbat-empty",
		.handler = qg_vbat_empty_handler,
		.wake    = true,
	},
};
static bool is_debug_batt_id(struct qpnp_qg *chip)
{
	if (is_between(DEBUG_BATT_ID_LOW, DEBUG_BATT_ID_HIGH,
						chip->batt_id_ohm))
		return true;
	return false;
}
static void qg_vote_awake(struct qpnp_qg *chip, atomic_t *voter, bool on)
{
	if (on) {
		if (atomic_inc_return(voter) == 1)
			__pm_stay_awake(chip->qg_ws);
	} else {
		if (atomic_read(voter) == 0)
			return;
		if (atomic_dec_return(voter) == 0)
			__pm_relax(chip->qg_ws);
	}
}
static inline void qg_ws_udata_ready(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_udata_ready, on);
}
static inline void qg_ws_fifo_done(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_fifo_done, on);
}
static inline void qg_ws_fifo_rt_done(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_fifo_rt_done, on);
}
static inline void qg_ws_suspend_data(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_suspend_data, on);
}
static inline void qg_ws_good_ocv(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_good_ocv, on);
}
static inline void qg_ws_sleep_exit_data(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_sleep_exit_data, on);
}
static inline void qg_ws_sleep_exit(struct qpnp_qg *chip, bool on)
{
	qg_vote_awake(chip, &chip->wake_voter_sleep_exit, on);
}
#define QG_FIFO_UPDATE_DONE_IRQ		1
#define QG_VBATT_EMPTY_IRQ		0
#define QG_GOOD_OCV_IRQ			2
static void qg_good_ocv_irq_disable(struct qpnp_qg *chip, bool disable)
{
	if (disable) {
		if (atomic_inc_return(&chip->good_ocv_irq_disable_cnt) == 1) {
			if (qg_irqs[QG_GOOD_OCV_IRQ].wake)
				disable_irq_wake(qg_irqs[QG_GOOD_OCV_IRQ].irq);
			if (qg_irqs[QG_GOOD_OCV_IRQ].irq)
				disable_irq_nosync(qg_irqs[QG_GOOD_OCV_IRQ].irq);
		}
	} else {
		if (atomic_read(&chip->good_ocv_irq_disable_cnt) == 0)
			return;
		if (atomic_dec_return(&chip->good_ocv_irq_disable_cnt) == 0) {
			if (qg_irqs[QG_GOOD_OCV_IRQ].irq)
				enable_irq(qg_irqs[QG_GOOD_OCV_IRQ].irq);
			if (qg_irqs[QG_GOOD_OCV_IRQ].wake)
				enable_irq_wake(qg_irqs[QG_GOOD_OCV_IRQ].irq);
		}
	}
}
static void qg_sleep_exit_work(struct work_struct *work)
{
	int rc = 0;
	struct qpnp_qg *chip = container_of(work,
			struct qpnp_qg, qg_sleep_exit_work.work);
	qg_ws_sleep_exit(chip, true);
	mutex_lock(&chip->data_lock);
	/*
	 * if this work is executing, the system has been active
	 * for a while. So, force back the S2 active configuration
	 */
	dev_dbg(chip->dev, "sleep_exit_work: exit S2_SLEEP\n");
	rc = qg_config_s2_state(chip, S2_SLEEP, false, true);
	if (rc < 0)
		dev_err(chip->dev, "Failed to exit S2_SLEEP rc=%d\n", rc);
	qg_ws_sleep_exit_data(chip, true);
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
	mutex_unlock(&chip->data_lock);
	qg_ws_sleep_exit(chip, false);
}
static int process_suspend(struct qpnp_qg *chip)
{
	int rc = 0;
	u8 status = 0;
	u8 sleep_fifo_raw = 0;
	u32 fifo_rt_length = 0, sleep_fifo_length = 0;
	if (!chip->profile_loaded)
		return 0;
	chip->suspend_data = false;
	/* read STATUS2 to clear its last state */
	rc = qg_read(chip, QG_STATUS2_REG, &status, 1);
	if (rc < 0)
		dev_dbg(chip->dev, "Failed to read STATUS2 rc=%d\n", rc);
	/* ignore suspend processing if charging */
	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		dev_dbg(chip->dev, "Charging @ suspend - ignore processing\n");
		if (chip->dt.qg_sleep_config) {
			rc = qg_config_s2_state(chip, S2_SLEEP, false, true);
			if (rc < 0) {
				dev_err(chip->dev, "Failed to exit S2-sleep rc=%d\n", rc);
				return rc;
			}
		}
		return 0;
	}
	rc = get_fifo_length(chip, &fifo_rt_length, true);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read FIFO RT count, rc=%d\n", rc);
		return rc;
	}
	rc = qg_read(chip, QG_S3_SLEEP_OCV_IBAT_CTL1_REG,
			&sleep_fifo_raw, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read sleep FIFO count, rc=%d\n", rc);
		return rc;
	}
	sleep_fifo_length = (u32)(sleep_fifo_raw & SLEEP_IBAT_QUALIFIED_LENGTH_MASK);
	sleep_fifo_length++;
	/*
	 * If the real-time FIFO count is greater than the number of
	 * FIFOs needed to enter sleep, save the FIFO data and reset
	 * the FIFO count to avoid a guaranteed wakeup due to a
	 * fifo_done event.
	 */
	if (chip->dt.qg_sleep_config) {
		dev_dbg(chip->dev, "Suspend: Forcing S2_SLEEP\n");
		rc = qg_config_s2_state(chip, S2_SLEEP, true, true);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to config S2_SLEEP rc=%d\n", rc);
			return rc;
		}
		if (chip->kdata.fifo_length > 0)
			chip->suspend_data = true;
	} else if (sleep_fifo_length <= chip->dt.s2_fifo_length &&
			fifo_rt_length >=
			(chip->dt.s2_fifo_length - sleep_fifo_length)) {
		/*
		 * If the real-time FIFO count is greater than
		 * the #fifo to enter sleep, save the FIFO data
		 * and reset the fifo count. This avoids a guaranteed wakeup
		 * due to fifo_done event as the current FIFO length is already
		 * beyond the sleep length.
		 */
		rc = qg_master_hold(chip, true);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to hold master, rc=%d\n", rc);
			return rc;
		}
		rc = qg_process_rt_fifo(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to process FIFO real-time, rc=%d\n", rc);
			qg_master_hold(chip, false);
			return rc;
		}
		rc = qg_master_hold(chip, false);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to release master, rc=%d\n", rc);
			return rc;
		}
		chip->last_fifo_update_time = ktime_get_boottime();
		chip->suspend_data = true;
	}
	get_rtc_time(&chip->suspend_time);
	dev_dbg(chip->dev, "FIFO rt_length=%u sleep_fifo_length=%u s2_count=%d suspend_data=%d\n",
			fifo_rt_length, sleep_fifo_length,
			chip->dt.s2_fifo_length, chip->suspend_data);
	return 0;
}
static int process_resume(struct qpnp_qg *chip)
{
	u8 status2 = 0, rt_status = 0;
	u32 ocv_uv = 0, ocv_raw = 0;
	int rc = 0;
	unsigned long rtc_sec = 0, sleep_time_secs = 0;
	if (!chip->profile_loaded)
		return 0;
	get_rtc_time(&rtc_sec);
	if (rtc_sec >= chip->suspend_time)
		sleep_time_secs = rtc_sec - chip->suspend_time;
	else
		sleep_time_secs = 0;
	if (chip->dt.qg_sleep_config)
		schedule_delayed_work(&chip->qg_sleep_exit_work,
				msecs_to_jiffies(QG_SLEEP_EXIT_TIME_MS));
	rc = qg_read(chip, QG_STATUS2_REG, &status2, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read status2 register, rc=%d\n", rc);
		return rc;
	}
	if (status2 & GOOD_OCV_BIT) {
		rc = qg_read_ocv(chip, &ocv_uv, &ocv_raw, S3_GOOD_OCV);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to read good_ocv, rc=%d\n", rc);
			return rc;
		}
		memset(&chip->kdata, 0, sizeof(chip->kdata));
		chip->kdata.fifo_time = (u32)rtc_sec;
		chip->kdata.param[QG_GOOD_OCV_UV].data = ocv_uv;
		chip->kdata.param[QG_GOOD_OCV_UV].valid = true;
		chip->suspend_data = false;
		/* allow SOC jump if we have slept longer */
		if (sleep_time_secs >= chip->dt.min_sleep_time_secs)
			chip->force_soc = true;
		dev_dbg(chip->dev, "GOOD OCV @ resume ocv=%duV\n", ocv_uv);
	}
	rc = qg_read(chip, QG_INT_LATCHED_STS_REG,
			&rt_status, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read latched status register, rc=%d\n", rc);
		return rc;
	}
	rt_status &= FIFO_UPDATE_DONE_INT_LAT_STS_BIT;
	/*
	 * If this is not a wakeup from FIFO-done,
	 * process the data immediately if - we have data from
	 * suspend or there is a good OCV.
	 */
	if (!rt_status && (chip->suspend_data ||
				chip->kdata.param[QG_GOOD_OCV_UV].valid)) {
		qg_ws_suspend_data(chip, true);
		mutex_lock(&chip->data_lock);
		/* signal the read thread */
		chip->data_ready = true;
		wake_up_interruptible(&chip->qg_wait_q);
		mutex_unlock(&chip->data_lock);
		chip->suspend_data = false;
	}
	return 0;
}
static int qg_suspend_noirq(struct device *dev)
{
	int rc = 0;
	struct qpnp_qg *chip = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&chip->qg_sleep_exit_work);
	mutex_lock(&chip->data_lock);
	rc = process_suspend(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to process QG suspend, rc=%d\n", rc);
	mutex_unlock(&chip->data_lock);
	return rc;
}
static int qg_resume_noirq(struct device *dev)
{
	int rc = 0;
	struct qpnp_qg *chip = dev_get_drvdata(dev);
	mutex_lock(&chip->data_lock);
	rc = process_resume(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to process QG resume, rc=%d\n", rc);
	mutex_unlock(&chip->data_lock);
	return rc;
}
static int qg_suspend(struct device *dev)
{
	struct qpnp_qg *chip = dev_get_drvdata(dev);
	if (!chip->profile_loaded)
		return 0;
	qg_good_ocv_irq_disable(chip, true);
	return 0;
}
static int qg_resume(struct device *dev)
{
	struct qpnp_qg *chip = dev_get_drvdata(dev);
	if (!chip->profile_loaded)
		return 0;
	qg_good_ocv_irq_disable(chip, false);
	return 0;
}
static const struct dev_pm_ops qg_pm_ops = {
	.suspend_noirq  = qg_suspend_noirq,
	.resume_noirq   = qg_resume_noirq,
	.suspend        = qg_suspend,
	.resume         = qg_resume,
};
static int qg_linear_interpolate(int y0, int x0, int y1, int x1, int x)
{
	s64 tmp = 0;
	if (y0 == y1 || x == x0)
		return y0;
	if (x1 == x0 || x == x1)
		return y1;
	tmp = (s64)(y1 - y0) * (s64)(x - x0);
	tmp = div_s64(tmp, x1 - x0);
	return y0 + tmp;
}
static int qg_interpolate_single_row_lut(struct profile_table_data *lut,
		int x, int scale)
{
	int i = 0, result = 0;
	int cols = lut->cols;
	if (!lut || !lut->col_entries || !lut->data || lut->cols == 0)
		return 0;
	if (x < lut->col_entries[0] * scale)
		return lut->data[0][0];
	if (x > lut->col_entries[cols-1] * scale)
		return lut->data[0][cols-1];
	for (i = 0; i < cols; i++) {
		if (x <= lut->col_entries[i] * scale)
			break;
	}
	if (x == lut->col_entries[i] * scale) {
		result = lut->data[0][i];
	} else {
		result = qg_linear_interpolate(
				lut->data[0][i-1],
				lut->col_entries[i-1] * scale,
				lut->data[0][i],
				lut->col_entries[i] * scale,
				x);
	}
	return result;
}
static int qg_interpolate_soc(struct profile_table_data *lut,
		int batt_temp, int ocv)
{
	int i = 0, j = 0, soc_high = 0, soc_low = 0, soc = 0;
	int rows = lut->rows;
	int cols = lut->cols;
	if (!lut || lut->rows <= 0 || lut->cols <= 0)
		return 0;
	if (batt_temp < lut->col_entries[0] * DEGC_SCALE)
		batt_temp = lut->col_entries[0] * DEGC_SCALE;
	if (batt_temp > lut->col_entries[cols - 1] * DEGC_SCALE)
		batt_temp = lut->col_entries[cols - 1] * DEGC_SCALE;
	for (j = 0; j < cols; j++) {
		if (batt_temp <= lut->col_entries[j] * DEGC_SCALE)
			break;
	}
	if (batt_temp == lut->col_entries[j] * DEGC_SCALE) {
		if (ocv >= lut->data[0][j])
			return lut->row_entries[0];
		if (ocv <= lut->data[rows - 1][j])
			return lut->row_entries[rows - 1];
		for (i = 0; i < rows; i++) {
			if (ocv >= lut->data[i][j]) {
				if (ocv == lut->data[i][j])
					return lut->row_entries[i];
				if (WARN_ON(i == 0))
					return lut->row_entries[0];
				soc = qg_linear_interpolate(
						lut->row_entries[i],
						lut->data[i][j],
						lut->row_entries[i - 1],
						lut->data[i - 1][j],
						ocv);
				return soc;
			}
		}
	}
	if (ocv >= lut->data[0][j])
		return lut->row_entries[0];
	if (ocv <= lut->data[rows - 1][j - 1])
		return lut->row_entries[rows - 1];
	soc_low = soc_high = 0;
	for (i = 0; i < rows-1; i++) {
		if (soc_high == 0 && is_between(lut->data[i][j],
					lut->data[i+1][j], ocv)) {
			soc_high = qg_linear_interpolate(
					lut->row_entries[i],
					lut->data[i][j],
					lut->row_entries[i + 1],
					lut->data[i+1][j],
					ocv);
		}
		if (soc_low == 0 && is_between(lut->data[i][j-1],
					lut->data[i+1][j-1], ocv)) {
			soc_low = qg_linear_interpolate(
					lut->row_entries[i],
					lut->data[i][j-1],
					lut->row_entries[i + 1],
					lut->data[i+1][j-1],
					ocv);
		}
		if (soc_high && soc_low) {
			soc = qg_linear_interpolate(
					soc_low,
					lut->col_entries[j-1] * DEGC_SCALE,
					soc_high,
					lut->col_entries[j] * DEGC_SCALE,
					batt_temp);
			return soc;
		}
	}
	if (soc_high)
		return soc_high;
	if (soc_low)
		return soc_low;
	return 10000;
}
static int qg_interpolate_var(struct profile_table_data *lut,
		int batt_temp, int soc)
{
	int i = 0, var1 = 0, var2 = 0, var = 0, rows = 0, cols = 0;
	int row1 = 0;
	int row2 = 0;
	rows = lut->rows;
	cols = lut->cols;
	if (soc > lut->row_entries[0]) {
		row1 = 0;
		row2 = 0;
	} else if (soc < lut->row_entries[rows - 1]) {
		row1 = rows - 1;
		row2 = rows - 1;
	} else {
		for (i = 0; i < rows; i++) {
			if (soc == lut->row_entries[i]) {
				row1 = i;
				row2 = i;
				break;
			}
			if (soc > lut->row_entries[i]) {
				row1 = i - 1;
				row2 = i;
				break;
			}
		}
	}
	if (batt_temp < lut->col_entries[0] * DEGC_SCALE)
		batt_temp = lut->col_entries[0] * DEGC_SCALE;
	if (batt_temp > lut->col_entries[cols - 1] * DEGC_SCALE)
		batt_temp = lut->col_entries[cols - 1] * DEGC_SCALE;
	for (i = 0; i < cols; i++)
		if (batt_temp <= lut->col_entries[i] * DEGC_SCALE)
			break;
	if (batt_temp == lut->col_entries[i] * DEGC_SCALE) {
		var = qg_linear_interpolate(
				lut->data[row1][i],
				lut->row_entries[row1],
				lut->data[row2][i],
				lut->row_entries[row2],
				soc);
		return var;
	}
	var1 = qg_linear_interpolate(
			lut->data[row1][i - 1],
			lut->col_entries[i - 1] * DEGC_SCALE,
			lut->data[row1][i],
			lut->col_entries[i] * DEGC_SCALE,
			batt_temp);
	var2 = qg_linear_interpolate(
			lut->data[row2][i - 1],
			lut->col_entries[i - 1] * DEGC_SCALE,
			lut->data[row2][i],
			lut->col_entries[i] * DEGC_SCALE,
			batt_temp);
	var = qg_linear_interpolate(
			var1,
			lut->row_entries[row1],
			var2,
			lut->row_entries[row2],
			soc);
	return var;
}
static int qg_interpolate_slope(struct profile_table_data *lut,
		int batt_temp, int soc)
{
	int i = 0, ocvrow1 = 0, ocvrow2 = 0, rows = 0, cols = 0;
	int row1 = 0, row2 = 0;
	int slope = 0;
	if (!lut || !lut->row_entries || !lut->col_entries || !lut->data)
		return 0;
	rows = lut->rows;
	cols = lut->cols;
	if (soc >= lut->row_entries[0]) {
		row1 = 0;
		row2 = 1;
	} else if (soc <= lut->row_entries[rows - 1]) {
		row1 = rows - 2;
		row2 = rows - 1;
	} else {
		for (i = 0; i < rows; i++) {
			if (soc >= lut->row_entries[i]) {
				row1 = i - 1;
				row2 = i;
				break;
			}
		}
	}
	if (batt_temp < lut->col_entries[0] * DEGC_SCALE)
		batt_temp = lut->col_entries[0] * DEGC_SCALE;
	if (batt_temp > lut->col_entries[cols - 1] * DEGC_SCALE)
		batt_temp = lut->col_entries[cols - 1] * DEGC_SCALE;
	for (i = 0; i < cols; i++) {
		if (batt_temp <= lut->col_entries[i] * DEGC_SCALE)
			break;
	}
	if (batt_temp == lut->col_entries[i] * DEGC_SCALE) {
		slope = (lut->data[row1][i] - lut->data[row2][i]);
		if (slope <= 0) {
			pr_warn_ratelimited("Slope=%d for soc=%d, using 1\n",
					slope, soc);
			slope = 1;
		}
		slope *= 10000;
		slope /= (lut->row_entries[row1] - lut->row_entries[row2]);
		return slope;
	}
	ocvrow1 = qg_linear_interpolate(
			lut->data[row1][i - 1],
			lut->col_entries[i - 1] * DEGC_SCALE,
			lut->data[row1][i],
			lut->col_entries[i] * DEGC_SCALE,
			batt_temp);
	ocvrow2 = qg_linear_interpolate(
			lut->data[row2][i - 1],
			lut->col_entries[i - 1] * DEGC_SCALE,
			lut->data[row2][i],
			lut->col_entries[i] * DEGC_SCALE,
			batt_temp);
	slope = (ocvrow1 - ocvrow2);
	if (slope <= 0) {
		pr_warn_ratelimited("Slope=%d for soc=%d, using 1\n",
				slope, soc);
		slope = 1;
	}
	slope *= 10000;
	slope /= (lut->row_entries[row1] - lut->row_entries[row2]);
	return slope;
}
static int qg_battery_data_open(struct inode *inode, struct file *file)
{
	struct qg_battery_data *battery = container_of(inode->i_cdev,
			struct qg_battery_data,
			battery_cdev);
	file->private_data = battery;
	return 0;
}
static int qg_battery_data_release(struct inode *inode, struct file *file)
{
	return 0;
}
static long qg_battery_data_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct qg_battery_data *battery = file->private_data;
	struct battery_params __user *bp_user =
		(struct battery_params __user *)arg;
	struct battery_params bp;
	int rc = 0, soc = 0, ocv_uv = 0, fcc_mah = 0, var = 0, slope = 0;
	if (!battery)
		return -ENODEV;
	if (!battery->profile_node)
		return -EINVAL;
	if (!bp_user)
		return -EINVAL;
	if (copy_from_user(&bp, bp_user, sizeof(bp)))
		return -EFAULT;
	if (bp.table_index >= ARRAY_SIZE(battery->profile))
		return -EINVAL;
	switch (cmd) {
	case BPIOCXSOC:
		if (bp.table_index != TABLE_SOC_OCV1 &&
				bp.table_index != TABLE_SOC_OCV2) {
			rc = -EINVAL;
		} else {
			soc = qg_interpolate_soc(
					&battery->profile[bp.table_index],
					bp.batt_temp, UV_TO_DECIUV(bp.ocv_uv));
			soc = CAP(QG_MIN_SOC, QG_MAX_SOC, soc);
			rc = put_user(soc, &bp_user->soc);
			if (rc < 0)
				goto ret_err;
		}
		break;
	case BPIOCXOCV:
		if (bp.table_index != TABLE_SOC_OCV1 &&
				bp.table_index != TABLE_SOC_OCV2) {
			rc = -EINVAL;
		} else {
			ocv_uv = qg_interpolate_var(
					&battery->profile[bp.table_index],
					bp.batt_temp, bp.soc);
			ocv_uv = DECIUV_TO_UV(ocv_uv);
			ocv_uv = CAP(QG_MIN_OCV_UV, QG_MAX_OCV_UV, ocv_uv);
			rc = put_user(ocv_uv, &bp_user->ocv_uv);
			if (rc < 0)
				goto ret_err;
		}
		break;
	case BPIOCXFCC:
		if (bp.table_index != TABLE_FCC1 &&
				bp.table_index != TABLE_FCC2) {
			rc = -EINVAL;
		} else {
			fcc_mah = qg_interpolate_single_row_lut(
					&battery->profile[bp.table_index],
					bp.batt_temp, DEGC_SCALE);
			fcc_mah = CAP(QG_MIN_FCC_MAH, QG_MAX_FCC_MAH, fcc_mah);
			rc = put_user(fcc_mah, &bp_user->fcc_mah);
			if (rc < 0)
				goto ret_err;
		}
		break;
	case BPIOCXVAR:
		if (bp.table_index < TABLE_Z1 ||
				bp.table_index >= TABLE_MAX) {
			rc = -EINVAL;
		} else {
			var = qg_interpolate_var(
					&battery->profile[bp.table_index],
					bp.batt_temp, bp.soc);
			var = CAP(QG_MIN_VAR, QG_MAX_VAR, var);
			rc = put_user(var, &bp_user->var);
			if (rc < 0)
				goto ret_err;
		}
		break;
	case BPIOCXSLOPE:
		if (bp.table_index != TABLE_SOC_OCV1 &&
				bp.table_index != TABLE_SOC_OCV2) {
			rc = -EINVAL;
		} else {
			slope = qg_interpolate_slope(
					&battery->profile[bp.table_index],
					bp.batt_temp, bp.soc);
			slope = CAP(QG_MIN_SLOPE, QG_MAX_SLOPE, slope);
			rc = put_user(slope, &bp_user->slope);
			if (rc < 0)
				goto ret_err;
		}
		break;
	default:
		rc = -EINVAL;
	}
ret_err:
	return rc;
}
static const struct file_operations qg_battery_data_fops = {
	.owner          = THIS_MODULE,
	.open           = qg_battery_data_open,
	.release        = qg_battery_data_release,
	.unlocked_ioctl = qg_battery_data_ioctl,
	.compat_ioctl   = qg_battery_data_ioctl,
};
static int get_table_length(struct device_node *node,
		int *length, const char *prop_name,
		bool ignore_null)
{
	struct property *prop;
	if (!node || !length || !prop_name)
		return -EINVAL;
	prop = of_find_property(node, prop_name, NULL);
	if (!prop) {
		if (ignore_null) {
			*length = 1;
			return 0;
		}
		return -ENODATA;
	}
	if (!prop->value)
		return -ENODATA;
	*length = prop->length / sizeof(u32);
	if (*length == 0)
		return -ENODATA;
	return 0;
}
static void qg_battery_profile_free(struct qpnp_qg *chip)
{
	int i = 0, j = 0;
	if (!chip->battery)
		return;
	for (i = 0; i < TABLE_MAX; i++) {
		kfree(chip->battery->profile[i].name);
		kfree(chip->battery->profile[i].row_entries);
		kfree(chip->battery->profile[i].col_entries);
		for (j = 0; j < chip->battery->profile[i].rows; j++) {
			if (chip->battery->profile[i].data)
				kfree(chip->battery->profile[i].data[j]);
		}
		kfree(chip->battery->profile[i].data);
	}
}
static int qg_parse_battery_profile(struct qg_battery_data *battery)
{
	int i = 0, j = 0, k = 0, rows = 0, cols = 0, lut_length = 0, rc = 0;
	struct device_node *node = NULL;
	struct property *prop;
	const __be32 *data;
	if (!battery || !battery->profile_node)
		return -EINVAL;
	for (i = 0; i < TABLE_MAX; i++) {
		of_node_put(node);
		node = NULL;
		node = of_find_node_by_name(battery->profile_node,
				batt_table[i].table_name);
		if (!node) {
			rc = -ENODEV;
			goto cleanup;
		}
		rc = get_table_length(node, &cols,
				"qcom,lut-col-legend", false);
		if (rc < 0)
			goto cleanup;
		rc = get_table_length(node, &rows,
				"qcom,lut-row-legend", true);
		if (rc < 0)
			goto cleanup;
		rc = get_table_length(node, &lut_length,
				"qcom,lut-data", false);
		if (rc < 0)
			goto cleanup;
		if (lut_length != cols * rows) {
			rc = -EINVAL;
			goto cleanup;
		}
		battery->profile[i].name = kzalloc(
				strlen(batt_table[i].table_name) + 1, GFP_KERNEL);
		if (!battery->profile[i].name) {
			rc = -ENOMEM;
			goto cleanup;
		}
		strscpy(battery->profile[i].name,
				batt_table[i].table_name,
				strlen(batt_table[i].table_name));
		battery->profile[i].rows = rows;
		battery->profile[i].cols = cols;
		if (rows != 1) {
			battery->profile[i].row_entries = kcalloc(rows,
					sizeof(*battery->profile[i].row_entries),
					GFP_KERNEL);
			if (!battery->profile[i].row_entries) {
				rc = -ENOMEM;
				goto cleanup;
			}
		}
		battery->profile[i].col_entries = kcalloc(cols,
				sizeof(*battery->profile[i].col_entries),
				GFP_KERNEL);
		if (!battery->profile[i].col_entries) {
			rc = -ENOMEM;
			goto cleanup;
		}
		battery->profile[i].data = kcalloc(rows,
				sizeof(*battery->profile[i].data), GFP_KERNEL);
		if (!battery->profile[i].data) {
			rc = -ENOMEM;
			goto cleanup;
		}
		for (j = 0; j < rows; j++) {
			battery->profile[i].data[j] = kcalloc(cols,
					sizeof(**battery->profile[i].data),
					GFP_KERNEL);
			if (!battery->profile[i].data[j]) {
				rc = -ENOMEM;
				goto cleanup;
			}
		}
		rc = of_property_read_u32_array(node, "qcom,lut-col-legend",
				battery->profile[i].col_entries, cols);
		if (rc < 0)
			goto cleanup;
		if (rows != 1) {
			rc = of_property_read_u32_array(node,
					"qcom,lut-row-legend",
					battery->profile[i].row_entries, rows);
			if (rc < 0)
				goto cleanup;
		}
		prop = of_find_property(node, "qcom,lut-data", NULL);
		if (!prop) {
			rc = -EINVAL;
			goto cleanup;
		}
		data = prop->value;
		for (j = 0; j < rows; j++)
			for (k = 0; k < cols; k++)
				battery->profile[i].data[j][k] =
					be32_to_cpup(data++);
	}
	of_node_put(node);
	return 0;
cleanup:
	of_node_put(node);
	for (; i >= 0; i--) {
		kfree(battery->profile[i].name);
		kfree(battery->profile[i].row_entries);
		kfree(battery->profile[i].col_entries);
		for (j = 0; j < battery->profile[i].rows; j++) {
			if (battery->profile[i].data)
				kfree(battery->profile[i].data[j]);
		}
		kfree(battery->profile[i].data);
	}
	return rc;
}
static int qg_batterydata_init(struct qpnp_qg *chip,
		struct device_node *profile_node)
{
	int rc = 0;
	struct qg_battery_data *battery;
	if (!chip || !profile_node)
		return -EINVAL;
	if (chip->battery) {
		battery = chip->battery;
		/* Clear profile_node before free to prevent stale access */
		battery->profile_node = NULL;
		qg_battery_profile_free(chip);
	} else {
		battery = kzalloc(sizeof(*battery), GFP_KERNEL);
		if (!battery)
			return -ENOMEM;
		rc = alloc_chrdev_region(&battery->dev_no, 0, 1,
				"qg_battery");
		if (rc < 0)
			goto free_battery;
		cdev_init(&battery->battery_cdev, &qg_battery_data_fops);
		rc = cdev_add(&battery->battery_cdev, battery->dev_no, 1);
		if (rc)
			goto unregister_chrdev;
		battery->battery_class = class_create("qg_battery");
		if (IS_ERR_OR_NULL(battery->battery_class)) {
			rc = -ENODEV;
			goto delete_cdev;
		}
		battery->battery_device = device_create(
				battery->battery_class, NULL,
				battery->dev_no, NULL, "qg_battery");
		if (IS_ERR_OR_NULL(battery->battery_device)) {
			rc = -ENODEV;
			goto destroy_class;
		}
		chip->battery = battery;
	}
	battery->profile_node = profile_node;
	rc = qg_parse_battery_profile(battery);
	if (rc < 0)
		goto destroy_device;
	return 0;
destroy_device:
	device_destroy(battery->battery_class, battery->dev_no);
destroy_class:
	class_destroy(battery->battery_class);
delete_cdev:
	cdev_del(&battery->battery_cdev);
unregister_chrdev:
	unregister_chrdev_region(battery->dev_no, 1);
free_battery:
	kfree(battery);
	chip->battery = NULL;
	return rc;
}
static void qg_batterydata_exit(struct qpnp_qg *chip)
{
	if (!chip->battery)
		return;
	device_destroy(chip->battery->battery_class, chip->battery->dev_no);
	class_destroy(chip->battery->battery_class);
	cdev_del(&chip->battery->battery_cdev);
	unregister_chrdev_region(chip->battery->dev_no, 1);
	qg_battery_profile_free(chip);
	kfree(chip->battery);
	chip->battery = NULL;
}
static inline bool is_sticky_register(u32 addr)
{
	return (addr & 0xFF) == QG_STATUS2_REG;
}
static int qg_read(struct qpnp_qg *chip, u32 addr, u8 *val, int len)
{
	int rc = 0;
	u32 dummy = 0;
	if (!chip || !val || len <= 0)
		return -EINVAL;
	rc = regmap_bulk_read(chip->regmap, chip->qg_base + addr, val, len);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read addr=0x%04x rc=%d\n", addr, rc);
		return rc;
	}
	if (is_sticky_register(addr)) {
		rc = regmap_write(chip->regmap, chip->qg_base + addr, dummy);
		if (rc < 0)
			return rc;
	}
	return rc;
}
static int qg_write(struct qpnp_qg *chip, u32 addr, u8 *val, int len)
{
	int rc = 0;
	if (!chip || !val || len <= 0)
		return -EINVAL;
	rc = regmap_bulk_write(chip->regmap, chip->qg_base + addr, val, len);
	if (rc < 0)
		dev_err(chip->dev, "Failed to write addr=0x%04x rc=%d\n", addr, rc);
	return rc;
}
static int qg_masked_write(struct qpnp_qg *chip, u32 addr, u8 mask, u8 val)
{
	int rc = 0;
	if (!chip)
		return -EINVAL;
	rc = regmap_update_bits(chip->regmap, chip->qg_base + addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Failed to write addr=0x%04x rc=%d\n", addr, rc);
	return rc;
}
static int get_fifo_length(struct qpnp_qg *chip, u32 *fifo_length, bool rt)
{
	int rc = 0;
	u8 reg = 0;
	u32 addr = 0;
	u8 status1 = 0;
	if (!chip || !fifo_length)
		return -EINVAL;
	rc = qg_read(chip, QG_STATUS1_REG, &status1, 1);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read STATUS1 rc=%d\n", rc);
	addr = rt ? QG_STATUS3_REG : QG_S2_NORMAL_MEAS_CTL2_REG;
	rc = qg_read(chip, addr, &reg, 1);
	if (rc < 0)
		return rc;
	if (rt) {
		*fifo_length = reg & COUNT_FIFO_RT_MASK;
	} else {
		*fifo_length = (reg & FIFO_LENGTH_MASK) >> FIFO_LENGTH_SHIFT;
		*fifo_length += 1;
	}
	return rc;
}
static int get_sample_count(struct qpnp_qg *chip, u32 *sample_count)
{
	int rc = 0;
	u8 reg = 0;
	if (!chip || !sample_count)
		return -EINVAL;
	rc = qg_read(chip, QG_S2_NORMAL_MEAS_CTL2_REG, &reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read FIFO sample count rc=%d\n", rc);
		return rc;
	}
	*sample_count = 1 << ((reg & NUM_OF_ACCUM_MASK) + 1);
	return rc;
}
static int get_sample_interval(struct qpnp_qg *chip, u32 *sample_interval)
{
	int rc = 0;
	u8 reg = 0;
	if (!chip || !sample_interval)
		return -EINVAL;
	rc = qg_read(chip, QG_S2_NORMAL_MEAS_CTL3_REG, &reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read FIFO sample interval rc=%d\n", rc);
		return rc;
	}
	*sample_interval = reg * 10;
	return rc;
}
static bool is_battery_present(struct qpnp_qg *chip)
{
	bool present = true;
	u8 reg = 0;
	int rc = 0;
	if (!chip)
		return false;
	if (chip->qg_version == QG_LITE) {
		rc = qg_read(chip, QG_STATUS2_REG, &reg, 1);
		if (rc < 0)
			dev_err(chip->dev, "Failed to read battery presence, rc=%d\n", rc);
		else
			present = !(reg & BATTERY_MISSING_BIT);
	} else {
		rc = qg_read(chip, QG_STATUS1_REG, &reg, 1);
		if (rc < 0)
			dev_err(chip->dev, "Failed to read battery presence, rc=%d\n", rc);
		else
			present = !!(reg & BATTERY_PRESENT_BIT);
	}
	return present;
}
static int qg_read_ocv(struct qpnp_qg *chip, u32 *ocv_uv, u32 *ocv_raw, u8 type)
{
	int rc = 0, addr = 0;
	u64 temp = 0;
	char ocv_name[20] = {0};
	if (!chip || !ocv_uv || !ocv_raw)
		return -EINVAL;
	switch (type) {
	case S3_GOOD_OCV:
		addr = QG_S3_GOOD_OCV_V_DATA0_REG;
		strscpy(ocv_name, "S3_GOOD_OCV", 20);
		break;
	case S7_PON_OCV:
		addr = QG_S7_PON_OCV_V_DATA0_REG;
		strscpy(ocv_name, "S7_PON_OCV", 20);
		break;
	case S3_LAST_OCV:
		addr = QG_LAST_S3_SLEEP_V_DATA0_REG;
		strscpy(ocv_name, "S3_LAST_OCV", 20);
		break;
	case SDAM_PON_OCV:
		addr = QG_SDAM_PON_OCV_OFFSET;
		strscpy(ocv_name, "SDAM_PON_OCV", 20);
		break;
	default:
		return -EINVAL;
	}
	if (type == SDAM_PON_OCV) {
		rc = qg_sdam_read(chip, SDAM_PON_OCV_UV, ocv_raw);
		if (rc < 0)
			return rc;
	} else {
		rc = qg_read(chip, addr, (u8 *)ocv_raw, 2);
		if (rc < 0)
			return rc;
	}
	temp = *ocv_raw;
	*ocv_uv = V_RAW_TO_UV(temp);
	dev_dbg(chip->dev, "%s: OCV_RAW=%x OCV=%uuV\n", ocv_name, *ocv_raw, *ocv_uv);
	return rc;
}
static int qg_master_hold(struct qpnp_qg *chip, bool hold)
{
	int rc = 0;
	if (!chip)
		return -EINVAL;
	/* clear the master */
	rc = qg_masked_write(chip, QG_DATA_CTL1_REG,
			MASTER_HOLD_OR_CLR_BIT, 0);
	if (rc < 0)
		return rc;
	if (hold) {
		/* 0 -> 1, hold the master */
		rc = qg_masked_write(chip, QG_DATA_CTL1_REG,
				MASTER_HOLD_OR_CLR_BIT,
				MASTER_HOLD_OR_CLR_BIT);
		if (rc < 0)
			return rc;
	}
	dev_dbg(chip->dev, "Master hold = %d\n", hold);
	return rc;
}
static int qg_update_fifo_length(struct qpnp_qg *chip, u8 length)
{
	int rc = 0;
	if (!chip)
		return -EINVAL;
	if (!length || length > MAX_FIFO_LENGTH) {
		dev_err(chip->dev, "Invalid FIFO length %d\n", length);
		return -EINVAL;
	}
	rc = qg_masked_write(chip, QG_S2_NORMAL_MEAS_CTL2_REG,
			FIFO_LENGTH_MASK,
			(length - 1) << FIFO_LENGTH_SHIFT);
	if (rc < 0)
		dev_err(chip->dev, "Failed to write FIFO length, rc=%d\n", rc);
	return rc;
}
static s64 qg_iraw_to_ua(struct qpnp_qg *chip, int iraw)
{
	if (!chip)
		return -EINVAL;
	if (chip->qg_subtype == QG_ADC_IBAT_5A)
		return div_s64(152588LL * (s64)iraw, 1000);
	else
		return div_s64(305176LL * (s64)iraw, 1000);
}
static int qg_process_fifo(struct qpnp_qg *chip, u32 fifo_length)
{
	int rc = 0, i = 0, j = 0, temp = 0;
	u8 v_fifo[MAX_FIFO_LENGTH * 2], i_fifo[MAX_FIFO_LENGTH * 2];
	u32 sample_interval = 0, sample_count = 0, fifo_v = 0, fifo_i = 0;
	unsigned long rtc_sec = 0;
	if (!chip)
		return -EINVAL;
	bool qg_v_mode = (chip->qg_mode == QG_V_MODE);
	rc = get_rtc_time(&rtc_sec);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read RTC time rc=%d\n", rc);
	else
		chip->kdata.fifo_time = (u32)rtc_sec;
	if (!fifo_length) {
		dev_err(chip->dev, "No FIFO data\n");
		return 0;
	}
	if (fifo_length > MAX_FIFO_LENGTH)
		return -EINVAL;
	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0)
		return rc;
	rc = get_sample_count(chip, &sample_count);
	if (rc < 0)
		return rc;
	if (chip->suspend_data &&
			(chip->kdata.fifo_length < (MAX_FIFO_LENGTH / 2))) {
		j = chip->kdata.fifo_length;
		chip->suspend_data = false;
		dev_dbg(chip->dev, "Pending suspend-data FIFO length=%d\n", j);
	} else {
		chip->kdata.fifo_length = 0;
	}
	for (i = 0; i < fifo_length * 2; i = i + 2, j++) {
		rc = qg_read(chip, QG_V_FIFO0_DATA0_REG + i, &v_fifo[i], 2);
		if (rc < 0)
			return rc;
		rc = qg_read(chip, QG_I_FIFO0_DATA0_REG + i, &i_fifo[i], 2);
		if (rc < 0)
			return rc;
		fifo_v = v_fifo[i] | (v_fifo[i + 1] << 8);
		fifo_i = i_fifo[i] | (i_fifo[i + 1] << 8);
		if (fifo_v == FIFO_V_RESET_VAL ||
				(fifo_i == FIFO_I_RESET_VAL && !qg_v_mode)) {
			return -EINVAL;
		}
		temp = sign_extend32(fifo_i, 15);
		if (j >= MAX_FIFO_LENGTH) {
			dev_warn(chip->dev, "FIFO overflow, dropping sample\n");
			break;
		}
		chip->kdata.fifo[j].v = V_RAW_TO_UV(fifo_v);
		chip->kdata.fifo[j].i =
			qg_v_mode ? 0 : qg_iraw_to_ua(chip, temp);
		chip->kdata.fifo[j].interval = sample_interval;
		chip->kdata.fifo[j].count = sample_count;
		chip->last_fifo_v_uv = chip->kdata.fifo[j].v;
		chip->last_fifo_i_ua = chip->kdata.fifo[j].i;
		dev_dbg(chip->dev, "FIFO %d raw_v=%d uV=%d raw_i=%d uA=%d interval=%d count=%d\n",
				j, fifo_v,
				chip->kdata.fifo[j].v,
				qg_v_mode ? 0 : fifo_i,
				(int)chip->kdata.fifo[j].i,
				chip->kdata.fifo[j].interval,
				chip->kdata.fifo[j].count);
	}
	chip->kdata.fifo_length += fifo_length;
	chip->kdata.seq_no = chip->seq_no++ % U32_MAX;
	return rc;
}
static int qg_process_accumulator(struct qpnp_qg *chip)
{
	int rc = 0, index = 0, sample_interval = 0;
	u8 count = 0;
	u64 acc_v = 0, acc_i = 0;
	s64 temp = 0;
	if (!chip)
		return -EINVAL;
	bool qg_v_mode = (chip->qg_mode == QG_V_MODE);
	rc = qg_read(chip, QG_ACCUM_CNT_RT_REG, &count, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read accumulator count, rc=%d\n", rc);
		return rc;
	}
	if (!count || count < 10) {
		dev_err(chip->dev, "Insufficient accumulator data\n");
		return 0;
	}
	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to get ACC sample interval, rc=%d\n", rc);
		return rc;
	}
	rc = qg_read(chip, QG_V_ACCUM_DATA0_RT_REG, (u8 *)&acc_v, 3);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read V accumulator, rc=%d\n", rc);
		return rc;
	}
	rc = qg_read(chip, QG_I_ACCUM_DATA0_RT_REG, (u8 *)&acc_i, 3);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read I accumulator, rc=%d\n", rc);
		return rc;
	}
	temp = sign_extend64(acc_i, 23);
	index = chip->kdata.fifo_length;
	if (index >= MAX_FIFO_LENGTH)
		index = MAX_FIFO_LENGTH - 1;
	chip->kdata.fifo[index].v = V_RAW_TO_UV(div_u64(acc_v, count));
	chip->kdata.fifo[index].i = qg_v_mode ?
		0 : qg_iraw_to_ua(chip, div_s64(temp, count));
	chip->kdata.fifo[index].interval = sample_interval;
	chip->kdata.fifo[index].count = count;
	chip->kdata.fifo_length++;
	if (chip->kdata.fifo_length == MAX_FIFO_LENGTH)
		chip->kdata.fifo_length = MAX_FIFO_LENGTH - 1;
	chip->last_fifo_v_uv = chip->kdata.fifo[index].v;
	chip->last_fifo_i_ua = chip->kdata.fifo[index].i;
	if (chip->kdata.fifo_length == 1)	/* Only accumulator data */
		chip->kdata.seq_no = chip->seq_no++ % U32_MAX;
	dev_dbg(chip->dev, "Accumulator: V=%u uV, I=%d uA, count=%u\n",
			chip->kdata.fifo[index].v, chip->kdata.fifo[index].i, count);
	return rc;
}
static int qg_process_rt_fifo(struct qpnp_qg *chip)
{
	int rc = 0;
	u32 fifo_length = 0;
	if (!chip)
		return -EINVAL;
	/* Get the real-time FIFO length */
	rc = get_fifo_length(chip, &fifo_length, true);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read RT FIFO length, rc=%d\n", rc);
		return rc;
	}
	if (!fifo_length)
		return 0;
	rc = qg_process_fifo(chip, fifo_length);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to process FIFO data, rc=%d\n", rc);
		return rc;
	}
	rc = qg_process_accumulator(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to process ACC data, rc=%d\n", rc);
		return rc;
	}
	return rc;
}
static int qg_get_battery_temp(struct qpnp_qg *chip, int *temp)
{
	int rc = 0, temp_raw = 0;
	if (!chip || !temp)
		return -EINVAL;
	if (chip->battery_missing) {
		*temp = 250;
		return 0;
	}
	if (!chip->batt_therm_chan) {
		*temp = 250;
		return 0;
	}
	rc = iio_read_channel_processed(chip->batt_therm_chan, &temp_raw);
	if (rc < 0) {
		*temp = 250;
		return 0;
	}
	*temp = temp_raw;
	return 0;
}
static int qg_get_vbat_avg(struct qpnp_qg *chip, int *vbat_uv)
{
	int rc = 0;
	u16 last_vbat = 0;
	if (!chip || !vbat_uv)
		return -EINVAL;
	rc = qg_read(chip, QG_S2_NORMAL_AVG_V_DATA0_REG,
			(u8 *)&last_vbat, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read S2_NORMAL_AVG_V reg rc=%d\n", rc);
		return rc;
	}
	*vbat_uv = V_RAW_TO_UV(last_vbat);
	return 0;
}
static int qg_get_ibat_avg(struct qpnp_qg *chip, int *ibat_ua)
{
	int rc = 0, last_ibat = 0;
	if (!chip  || !ibat_ua)
		return -EINVAL;
	rc = qg_read(chip, QG_S2_NORMAL_AVG_I_DATA0_REG,
			(u8 *)&last_ibat, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read S2_NORMAL_AVG_I reg rc=%d\n", rc);
		return rc;
	}
	if (last_ibat == (int)FIFO_I_RESET_VAL) {
		rc = qg_get_battery_current(chip, ibat_ua);
		if (rc < 0)
			dev_err(chip->dev, "Failed to read inst IBAT rc=%d\n", rc);
		return rc;
	}
	last_ibat  = sign_extend32(last_ibat, 15);
	*ibat_ua   = qg_iraw_to_ua(chip, last_ibat);
	return 0;
}
static int qg_write_monotonic_soc(struct qpnp_qg *chip, int msoc)
{
	u8 reg = 0;
	int rc = 0;
	if (!chip)
		return -EINVAL;
	/* Scale 0–100% to 0–255 for hardware register */
	reg = (msoc * 255) / 100;
	rc  = qg_write(chip, QG_SOC_MONOTONIC_REG, &reg, 1);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update QG_SOC_MONOTONIC reg rc=%d\n", rc);
	return rc;
}
static bool is_chg_usb_present(struct qpnp_qg *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0;
	if (!chip)
		return false;
	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("pm4125-charger");
	if (!chip->usb_psy)
		return false;
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &pval);
	if (rc < 0)
		return false;
	return pval.intval ? true : false;
}
static bool is_dc_present(struct qpnp_qg *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0;
	if (!chip)
		return false;
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");
	if (chip->dc_psy) {
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			return false;
	}
	return pval.intval ? true : false;
}
static bool is_input_present(struct qpnp_qg *chip)
{
	return is_chg_usb_present(chip) || is_dc_present(chip);
}
static bool is_chan_valid(struct qpnp_qg *chip,
		enum qg_ext_iio_channels chan)
{
	int rc = 0;
	if (!chip)
		return false;
	if (IS_ERR(chip->ext_iio_chans[chan]))
		return false;
	if (!chip->ext_iio_chans[chan]) {
		chip->ext_iio_chans[chan] = devm_iio_channel_get(chip->dev,
				qg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->ext_iio_chans[chan] = NULL;
			dev_err(chip->dev, "Failed to get IIO channel %s rc=%d\n",
					qg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}
static int qg_read_iio_chan(struct qpnp_qg *chip,
		enum qg_ext_iio_channels chan, int *val)
{
	int rc = 0;
	if (!chip)
		return -EINVAL;
	if (is_chan_valid(chip, chan)) {
		rc = iio_read_channel_processed(chip->ext_iio_chans[chan], val);
		return (rc < 0) ? rc : 0;
	}
	return -EINVAL;
}
static int qg_get_nominal_capacity(struct qpnp_qg *chip, u32 *nom_cap_uah,
		int batt_temp, bool charging)
{
	struct profile_table_data *lut;
	u8 table_index = charging ? TABLE_FCC1 : TABLE_FCC2;
	u32 fcc_mah = 0;
	if (!chip || !nom_cap_uah)
		return -EINVAL;
	if (!chip->battery || !chip->battery->profile_node)
		return -ENODEV;
	lut = &chip->battery->profile[table_index];
	if (!lut->rows)
		return -ENODATA;
	fcc_mah = qg_interpolate_single_row_lut(lut, batt_temp, DEGC_SCALE);
	fcc_mah = CAP(QG_MIN_FCC_MAH, QG_MAX_FCC_MAH, fcc_mah);
	*nom_cap_uah = fcc_mah * 1000;
	return 0;
}
static int qg_load_battery_profile(struct qpnp_qg *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *profile_node = NULL;
	u32 id_kohm, batt_id_kohm = chip->batt_id_ohm / 1000;
	int rc = 0;
	profile_node = of_parse_phandle(node, "qcom,battery-data", 0);
	if (!profile_node) {
		dev_err(chip->dev, "qcom,battery-data phandle not found in DT node: %s\n",
				node->full_name);
		return -ENXIO;
	}
	if (!of_property_read_u32(profile_node, "qcom,batt-id-kohm",
				&id_kohm)) {
		dev_dbg(chip->dev, "Profile batt-id=%ukohm,detected=%ukohm\n",
				id_kohm, batt_id_kohm);
	}
	rc = of_property_read_string(profile_node, "qcom,battery-type",
			&chip->bp.batt_type_str);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read battery-type rc=%d\n", rc);
		of_node_put(profile_node);
		return rc;
	}
	rc = qg_batterydata_init(chip, profile_node);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to initialize battery-profile rc=%d\n", rc);
		of_node_put(profile_node);
		return rc;
	}
	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
			&chip->bp.float_volt_uv);
	if (rc < 0) {
		dev_warn(chip->dev, "Failed to read max-voltage-uv rc=%d\n", rc);
		chip->bp.float_volt_uv = -EINVAL;
	}
	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
			&chip->bp.fastchg_curr_ma);
	if (rc < 0) {
		dev_warn(chip->dev, "Failed to read fastchg-current-ma rc=%d\n", rc);
		chip->bp.fastchg_curr_ma = -EINVAL;
	}
	if (chip->bp.fastchg_curr_ma > 0)
		chip->bp.fastchg_curr_ma = min(chip->max_fcc_limit_ma,
				chip->bp.fastchg_curr_ma);
	rc = of_property_read_u32(profile_node, "qcom,qg-batt-profile-ver",
			&chip->bp.qg_profile_version);
	if (rc < 0) {
		dev_warn(chip->dev, "Failed to read qg-batt-profile-ver rc=%d\n", rc);
		chip->bp.qg_profile_version = -EINVAL;
	}
	return 0;
}
static int qg_setup_battery(struct qpnp_qg *chip)
{
	int rc = 0;
	if (!is_battery_present(chip)) {
		dev_err(chip->dev, "Battery Missing!\n");
		chip->battery_missing = true;
		chip->profile_loaded = false;
		chip->soc_reporting_ready = true;
		return 0;
	}
	chip->chg_psy = power_supply_get_by_name("pm4125-charger");
	if (!chip->chg_psy) {
		dev_err(chip->dev, "Could not get charger psy, deferring\n");
		return -EPROBE_DEFER;
	}
	rc = qg_get_batt_id(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to get batt_id rc=%d\n", rc);
		chip->profile_loaded = false;
	} else {
		rc = qg_load_battery_profile(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to load battery-profile rc=%d\n", rc);
			chip->profile_loaded = false;
			chip->soc_reporting_ready = true;
		} else {
			chip->profile_loaded = true;
		}
	}
	return 0;
}
#define S7_ERROR_MARGIN_UV  20000
static int get_rtc_time(unsigned long *rtc_time)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc = 0;
	if (!rtc_time)
		return -EINVAL;
	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL)
		return -EINVAL;
	rc = rtc_read_time(rtc, &tm);
	if (rc)
		goto close_time;
	rc = rtc_valid_tm(&tm);
	if (rc)
		goto close_time;
	*rtc_time = rtc_tm_to_time64(&tm);
close_time:
	rtc_class_close(rtc);
	return rc;
}
static int lookup_soc_ocv(struct qpnp_qg *chip, u32 *soc, u32 ocv_uv, int batt_temp, bool charging)
{
	u8 table_index = charging ? TABLE_SOC_OCV1 : TABLE_SOC_OCV2;
	if (!chip || !soc)
		return -EINVAL;
	if (!chip->battery || !chip->battery->profile_node)
		return -ENODEV;
	*soc = qg_interpolate_soc(&chip->battery->profile[table_index],
			batt_temp, UV_TO_DECIUV(ocv_uv));
	*soc = CAP(0, 100, DIV_ROUND_CLOSEST(*soc, 100));
	return 0;
}
static int qg_determine_pon_soc(struct qpnp_qg *chip)
{
	struct ocv_all ocv[PON_OCV_MAX] = {
		[S7_PON_OCV]   = { 0, 0, "S7_PON_OCV"},
		[S3_GOOD_OCV]  = { 0, 0, "S3_GOOD_OCV"},
		[S3_LAST_OCV]  = { 0, 0, "S3_LAST_OCV"},
		[SDAM_PON_OCV] = { 0, 0, "SDAM_PON_OCV"},
	};
	int rc = 0, batt_temp = 0, i, shutdown_temp = 0;
	bool use_pon_ocv = true;
	unsigned long rtc_sec = 0;
	u32 ocv_uv = 0, soc = 0, pon_soc = 0, full_soc = 0, cutoff_soc = 0;
	u32 shutdown[SDAM_MAX] = {0}, soc_raw = 0;
	char ocv_type[20] = "NONE";
	if (!chip->profile_loaded) {
		dev_err(chip->dev, "No Profile,skipping PON soc\n");
		return 0;
	}
	for (i = S7_PON_OCV; i < PON_OCV_MAX; i++) {
		rc = qg_read_ocv(chip, &ocv[i].ocv_uv,
				&ocv[i].ocv_raw, i);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to read %s OCV rc=%d\n",
					ocv[i].ocv_type, rc);
		} else {
			dev_dbg(chip->dev, "%s OCV=%d\n",
					ocv[i].ocv_type, ocv[i].ocv_uv);
		}
	}
	rc = qg_get_battery_temp(chip, &batt_temp);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read BATT_TEMP at PON rc=%d\n", rc);
		goto done;
	}
	rc = get_rtc_time(&rtc_sec);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read RTC time rc=%d\n", rc);
		goto use_pon_ocv;
	}
	rc = qg_sdam_read_all(chip, shutdown);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read shutdown params rc=%d\n", rc);
		goto use_pon_ocv;
	}
	shutdown_temp = sign_extend32(shutdown[SDAM_TEMP], 15);
	rc = lookup_soc_ocv(chip, &pon_soc, ocv[S7_PON_OCV].ocv_uv, batt_temp, false);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to lookup S7_PON SOC rc=%d\n", rc);
		goto done;
	}
	dev_dbg(chip->dev, "Shutdown: Valid=%d SOC=%d OCV=%duV time=%dsecs temp=%d, time_now=%ldsecs temp_now=%d S7_soc=%d\n",
			shutdown[SDAM_VALID],
			shutdown[SDAM_SOC],
			shutdown[SDAM_OCV_UV],
			shutdown[SDAM_TIME_SEC],
			shutdown_temp,
			rtc_sec, batt_temp,
			pon_soc);
	/*
	 * Use the shutdown SOC if
	 * 1. SDAM read is a success & SDAM data is valid
	 * 2. The device was powered off for < ignore_shutdown_time
	 * 2. Batt temp has not changed more than shutdown_temp_diff
	 */
	if (!shutdown[SDAM_VALID])
		goto use_pon_ocv;
	if (!is_between(0, chip->dt.ignore_shutdown_soc_secs,
				(rtc_sec - shutdown[SDAM_TIME_SEC]))) {
		goto use_pon_ocv;
	}
	if (!is_between(0, chip->dt.shutdown_temp_diff,
				abs(shutdown_temp -  batt_temp)) &&
			(shutdown_temp < 0 || batt_temp < 0)) {
		goto use_pon_ocv;
	}
	if ((chip->dt.shutdown_soc_threshold != -EINVAL) &&
			!is_between(0, chip->dt.shutdown_soc_threshold,
				abs(pon_soc - shutdown[SDAM_SOC]))) {
		goto use_pon_ocv;
	}
	use_pon_ocv = false;
	ocv_uv = shutdown[SDAM_OCV_UV];
	soc = shutdown[SDAM_SOC];
	soc_raw = shutdown[SDAM_SOC] * 100;
	strscpy(ocv_type, "SHUTDOWN_SOC", 20);
	dev_dbg(chip->dev, "Using SHUTDOWN_SOC @ PON\n");
use_pon_ocv:
	if (use_pon_ocv) {
		if (chip->wa_flags & QG_PON_OCV_WA) {
			if (ocv[S3_LAST_OCV].ocv_raw == FIFO_V_RESET_VAL) {
				if (!ocv[SDAM_PON_OCV].ocv_uv) {
					strscpy(ocv_type, "S7_PON_SOC", 20);
					ocv_uv = ocv[S7_PON_OCV].ocv_uv;
				} else if (ocv[SDAM_PON_OCV].ocv_uv <=
						ocv[S7_PON_OCV].ocv_uv) {
					strscpy(ocv_type, "S7_PON_SOC", 20);
					ocv_uv = ocv[S7_PON_OCV].ocv_uv;
				} else if (!shutdown[SDAM_VALID] &&
						((ocv[SDAM_PON_OCV].ocv_uv -
						  ocv[S7_PON_OCV].ocv_uv) >
						 S7_ERROR_MARGIN_UV)) {
					strscpy(ocv_type, "S7_PON_SOC", 20);
					ocv_uv = ocv[S7_PON_OCV].ocv_uv;
				} else {
					strscpy(ocv_type, "SDAM_PON_SOC", 20);
					ocv_uv = ocv[SDAM_PON_OCV].ocv_uv;
				}
			} else {
				if (ocv[S3_LAST_OCV].ocv_uv >=
						ocv[S7_PON_OCV].ocv_uv) {
					strscpy(ocv_type, "S3_LAST_SOC", 20);
					ocv_uv = ocv[S3_LAST_OCV].ocv_uv;
				} else {
					strscpy(ocv_type, "S7_PON_SOC", 20);
					ocv_uv = ocv[S7_PON_OCV].ocv_uv;
				}
			}
		} else {
			/* Use S7 PON OCV */
			strscpy(ocv_type, "S7_PON_SOC", 20);
			ocv_uv = ocv[S7_PON_OCV].ocv_uv;
		}
		ocv_uv = CAP(QG_MIN_OCV_UV, QG_MAX_OCV_UV, ocv_uv);
		rc = lookup_soc_ocv(chip, &pon_soc, ocv_uv, batt_temp, false);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to lookup SOC@PON rc=%d\n", rc);
			goto done;
		}
		rc = lookup_soc_ocv(chip, &full_soc, chip->bp.float_volt_uv,
				batt_temp, true);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to lookup FULL_SOC@PON rc=%d\n", rc);
			goto done;
		}
		full_soc = CAP(0, 99, full_soc);
		rc = lookup_soc_ocv(chip, &cutoff_soc,
				chip->dt.vbatt_cutoff_mv * 1000,
				batt_temp, false);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to lookup CUTOFF_SOC@PON rc=%d\n", rc);
			goto done;
		}
		if ((full_soc > cutoff_soc) && (pon_soc > cutoff_soc)) {
			soc = DIV_ROUND_UP(((pon_soc - cutoff_soc) * 100),
					(full_soc - cutoff_soc));
			soc = CAP(0, 100, soc);
			soc_raw = soc * 100;
		} else {
			soc_raw = pon_soc * 100;
			soc = pon_soc;
		}
		dev_dbg(chip->dev, "v_float=%d v_cutoff=%d FULL_SOC=%d CUTOFF_SOC=%d PON_SYS_SOC=%d pon_soc=%d\n",
				chip->bp.float_volt_uv, chip->dt.vbatt_cutoff_mv * 1000,
				full_soc, cutoff_soc, soc, pon_soc);
	}
done:
	if (rc < 0) {
		dev_err(chip->dev, "Failed to get %s @ PON, rc=%d\n", ocv_type, rc);
		return rc;
	}
	if (chip->qg_mode == QG_V_I_MODE)
		chip->cc_soc = soc_raw;
	chip->sys_soc = soc_raw;
	chip->last_adj_ssoc = chip->catch_up_soc = chip->msoc = soc;
	chip->kdata.param[QG_PON_OCV_UV].data = ocv_uv;
	chip->kdata.param[QG_PON_OCV_UV].valid = true;
	/* write back to SDAM */
	chip->sdam_data[SDAM_SOC] = soc;
	chip->sdam_data[SDAM_OCV_UV] = ocv_uv;
	chip->sdam_data[SDAM_VALID] = 1;
	rc = qg_write_monotonic_soc(chip, chip->msoc);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update MSOC register rc=%d\n", rc);
	rc = qg_store_soc_params(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update sdam params rc=%d\n", rc);
	dev_info(chip->dev, "using %s @ PON ocv_uv=%duV soc=%d\n",
			ocv_type, ocv_uv, chip->msoc);
	/* SOC reporting is now ready */
	chip->soc_reporting_ready = 1;
	return 0;
}
#define QG_DATA_CTL2_REG			0x42
#define BURST_AVG_HOLD_FOR_READ_BIT		BIT(0)
#define QG_LAST_BURST_AVG_I_DATA0_REG		0xC6
static int qg_get_battery_voltage(struct qpnp_qg *chip, int *volt_uv)
{
	int rc = 0;
	u16 last_vbat = 0;
	if (!chip || !volt_uv)
		return -EINVAL;
	if (chip->battery_missing) {
		*volt_uv = 3700000;
		return 0;
	}
	rc = qg_masked_write(chip, QG_DATA_CTL2_REG,
			BURST_AVG_HOLD_FOR_READ_BIT,
			BURST_AVG_HOLD_FOR_READ_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to hold burst-avg data rc=%d\n", rc);
		goto release;
	}
	rc = qg_read(chip, QG_LAST_ADC_V_DATA0_REG, (u8 *)&last_vbat, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read LAST_ADC_V reg rc=%d\n", rc);
		goto release;
	}
	*volt_uv = V_RAW_TO_UV(last_vbat);
	dev_dbg(chip->dev, "VBAT raw=0x%x uv=%d\n", last_vbat, *volt_uv);
release:
	qg_masked_write(chip, QG_DATA_CTL2_REG,
			BURST_AVG_HOLD_FOR_READ_BIT, 0);
	return rc;
}
static int qg_get_battery_current(struct qpnp_qg *chip, int *curr_ua)
{
	int rc = 0, last_ibat = 0;
	if (!chip || !curr_ua)
		return -EINVAL;
	if (chip->battery_missing) {
		*curr_ua = 0;
		return 0;
	}
	if (chip->qg_mode == QG_V_MODE) {
		*curr_ua = chip->qg_v_ibat;
		return 0;
	}
	rc = qg_masked_write(chip, QG_DATA_CTL2_REG,
			BURST_AVG_HOLD_FOR_READ_BIT,
			BURST_AVG_HOLD_FOR_READ_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to hold burst-avg data rc=%d\n", rc);
		goto release;
	}
	rc = qg_read(chip, QG_LAST_BURST_AVG_I_DATA0_REG,
			(u8 *)&last_ibat, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read LAST_BURST_AVG_I reg rc=%d\n", rc);
		goto release;
	}
	last_ibat = sign_extend32(last_ibat, 15);
	*curr_ua  = qg_iraw_to_ua(chip, last_ibat);
	dev_dbg(chip->dev, "IBAT raw=%d ua=%d\n", last_ibat, *curr_ua);
release:
	qg_masked_write(chip, QG_DATA_CTL2_REG,
			BURST_AVG_HOLD_FOR_READ_BIT, 0);
	return rc;
}
static int qg_get_battery_capacity(struct qpnp_qg *chip, int *capacity)
{
	if (!chip || !capacity)
		return -EINVAL;
	if (is_debug_batt_id(chip))
		*capacity = DEBUG_BATT_SOC;
	else if (chip->battery_missing || !chip->profile_loaded)
		*capacity = BATT_MISSING_SOC;
	else
		*capacity = chip->msoc;
	dev_dbg(chip->dev, "SOC=%d\n", *capacity);
	return 0;
}
static int qg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct qpnp_qg *chip = power_supply_get_drvdata(psy);
	int rc = 0;
	if (!chip || !val)
		return -EINVAL;
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = qg_get_battery_capacity(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = qg_get_battery_voltage(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = qg_get_battery_current(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = qg_get_battery_temp(chip, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !chip->battery_missing;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->charge_status;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = chip->charge_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = (chip->bp.float_volt_uv > 0)
			? chip->bp.float_volt_uv : -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		if (chip->bp.fastchg_curr_ma <= 0)
			val->intval = 0;
		else
			val->intval = chip->bp.fastchg_curr_ma * 1000;
		break;
	default:
		dev_dbg(chip->dev, "Unsupported property %d\n", psp);
		return -EINVAL;
	}
	return rc;
}
static int qg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	return -EINVAL;
}
static enum power_supply_property qg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};
static const struct power_supply_desc qg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = qg_props,
	.num_properties = ARRAY_SIZE(qg_props),
	.get_property = qg_get_property,
	.set_property = qg_set_property,
};
static int qg_process_fvss_soc(struct qpnp_qg *chip, int sys_soc)
{
	int rc, vbat_uv = 0, vbat_cutoff_uv = chip->dt.vbatt_cutoff_mv * 1000;
	int soc_vbat = 0, wt_vbat = 0, wt_sys = 0, soc_fvss = 0;
	if (!chip->dt.fvss_enable && !(qg_ss_feature & QG_FVSS))
		goto exit_soc_scale;
	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING)
		goto exit_soc_scale;
	rc = qg_get_battery_voltage(chip, &vbat_uv);
	if (rc < 0)
		goto exit_soc_scale;
	if (!chip->last_fifo_v_uv)
		chip->last_fifo_v_uv = vbat_uv;
	if (chip->last_fifo_v_uv > (chip->dt.fvss_vbat_mv * 1000)) {
		dev_dbg(chip->dev, "FVSS: last_fifo_v=%d fvss_entry_uv=%d - exit\n",
				chip->last_fifo_v_uv, chip->dt.fvss_vbat_mv * 1000);
		goto exit_soc_scale;
	}
	if (!chip->fvss_active) {
		chip->vbat_fvss_entry = CAP(vbat_cutoff_uv,
				chip->dt.fvss_vbat_mv * 1000,
				chip->last_fifo_v_uv);
		chip->soc_fvss_entry = sys_soc;
		chip->fvss_active = true;
	} else if (chip->last_fifo_v_uv > chip->vbat_fvss_entry) {
		chip->vbat_fvss_entry = chip->last_fifo_v_uv;
		chip->soc_fvss_entry = sys_soc;
	}
	soc_vbat = qg_linear_interpolate(chip->soc_fvss_entry,
			chip->vbat_fvss_entry,
			0,
			vbat_cutoff_uv,
			chip->last_fifo_v_uv);
	soc_vbat = CAP(0, 100, soc_vbat);
	if (qg_fvss_vbat_scaling) {
		wt_vbat = 100;
		wt_sys = 0;
	} else {
		wt_sys = qg_linear_interpolate(100,
				chip->soc_fvss_entry,
				0,
				0,
				sys_soc);
		wt_sys = CAP(0, 100, wt_sys);
		wt_vbat = 100 - wt_sys;
	}
	soc_fvss = ((soc_vbat * wt_vbat) + (sys_soc * wt_sys)) / 100;
	soc_fvss = CAP(0, 100, soc_fvss);
	dev_dbg(chip->dev, "FVSS: vbat_fvss_entry=%d soc_fvss_entry=%d cutoff_uv=%d vbat_uv=%d fifo_avg_v=%d soc_vbat=%d sys_soc=%d wt_vbat=%d wt_sys=%d soc_fvss=%d\n",
			chip->vbat_fvss_entry, chip->soc_fvss_entry,
			vbat_cutoff_uv, vbat_uv, chip->last_fifo_v_uv,
			soc_vbat, sys_soc, wt_vbat, wt_sys, soc_fvss);
	return soc_fvss;
exit_soc_scale:
	chip->fvss_active = false;
	return sys_soc;
}
#define IBAT_HYST_PC			10
#define TCSS_ENTRY_COUNT		2
static int qg_process_tcss_soc(struct qpnp_qg *chip, int sys_soc)
{
	int rc, ibatt_diff = 0, ibat_inc_hyst = 0;
	int qg_iterm_ua = (-1 * chip->dt.iterm_ma * 1000);
	int soc_ibat = 0, wt_ibat = 0, wt_sys = 0, val = 0;
	union power_supply_propval prop = {0, };
	if (!chip->dt.tcss_enable && !(qg_ss_feature & QG_TCSS))
		goto exit_soc_scale;
	if (chip->sys_soc < (chip->dt.tcss_entry_soc * 100))
		goto exit_soc_scale;
	if (chip->sys_soc >= QG_MAX_SOC && chip->soc_tcss >= QG_MAX_SOC)
		goto exit_soc_scale;
	rc = power_supply_get_property(chip->chg_psy,
			POWER_SUPPLY_PROP_HEALTH, &prop);
	if (!rc && (prop.intval == POWER_SUPPLY_HEALTH_COOL ||
				prop.intval == POWER_SUPPLY_HEALTH_WARM))
		goto exit_soc_scale;
	if (chip->last_fifo_i_ua >= 0)
		goto exit_soc_scale;
	else if (++chip->tcss_entry_count < TCSS_ENTRY_COUNT)
		goto skip_entry_count;
	if (!chip->tcss_active) {
		chip->soc_tcss = sys_soc;
		chip->soc_tcss_entry = sys_soc;
		chip->ibat_tcss_entry = min(chip->last_fifo_i_ua, qg_iterm_ua);
		chip->prev_fifo_i_ua = chip->last_fifo_i_ua;
		chip->tcss_active = true;
	}
	rc = qg_read_iio_chan(chip, INPUT_CURRENT_LIMITED, &val);
	if (!rc && val) {
		dev_dbg(chip->dev, "Input limited sys_soc=%d soc_tcss=%d\n",
				sys_soc, chip->soc_tcss);
		if (chip->soc_tcss > sys_soc)
			sys_soc = chip->soc_tcss;
		goto exit_soc_scale;
	}
	ibatt_diff = chip->last_fifo_i_ua - chip->prev_fifo_i_ua;
	if (ibatt_diff > 0) {
		ibat_inc_hyst = (chip->prev_fifo_i_ua * IBAT_HYST_PC) / 100;
		if (ibatt_diff > abs(ibat_inc_hyst))
			chip->prev_fifo_i_ua -= ibat_inc_hyst;
		else
			chip->prev_fifo_i_ua = chip->last_fifo_i_ua;
	}
	chip->prev_fifo_i_ua = min(chip->prev_fifo_i_ua, qg_iterm_ua);
	soc_ibat = qg_linear_interpolate(chip->soc_tcss_entry,
			chip->ibat_tcss_entry,
			QG_MAX_SOC,
			qg_iterm_ua,
			chip->prev_fifo_i_ua);
	soc_ibat = CAP(QG_MIN_SOC, QG_MAX_SOC, soc_ibat);
	wt_ibat = qg_linear_interpolate(1, chip->soc_tcss_entry,
			10000, 10000, soc_ibat);
	wt_ibat = CAP(QG_MIN_SOC, QG_MAX_SOC, wt_ibat);
	wt_sys = 10000 - wt_ibat;
	chip->soc_tcss = DIV_ROUND_CLOSEST((soc_ibat * wt_ibat) +
			(wt_sys * sys_soc), 10000);
	chip->soc_tcss = CAP(QG_MIN_SOC, QG_MAX_SOC, chip->soc_tcss);
	dev_dbg(chip->dev, "TCSS: fifo_i=%d prev_fifo_i=%d ibatt_tcss_entry=%d qg_term=%d soc_tcss_entry=%d sys_soc=%d soc_ibat=%d wt_ibat=%d wt_sys=%d soc_tcss=%d\n",
			chip->last_fifo_i_ua, chip->prev_fifo_i_ua,
			chip->ibat_tcss_entry, qg_iterm_ua,
			chip->soc_tcss_entry, sys_soc, soc_ibat,
			wt_ibat, wt_sys, chip->soc_tcss);
	return chip->soc_tcss;
exit_soc_scale:
	chip->tcss_entry_count = 0;
skip_entry_count:
	chip->tcss_active = false;
	if (chip->dt.tcss_enable || (qg_ss_feature & QG_TCSS))
		dev_dbg(chip->dev, "TCSS: Quit - enabled=%d sys_soc=%d tcss_entry_count=%d fifo_i_ua=%d\n",
				chip->dt.tcss_enable, sys_soc, chip->tcss_entry_count,
				chip->last_fifo_i_ua);
	return sys_soc;
}
#define BASS_SYS_MSOC_DELTA   2
static int qg_process_bass_soc(struct qpnp_qg *chip, int sys_soc)
{
	int bass_soc = sys_soc, msoc = chip->msoc;
	if (!chip->dt.bass_enable && !(qg_ss_feature & QG_BASS))
		goto exit_soc_scale;
	dev_dbg(chip->dev, "BASS Entry: fifo_i=%d sys_soc=%d msoc=%d batt_soc=%d fvss_active=%d\n",
			chip->last_fifo_i_ua, sys_soc, msoc,
			chip->batt_soc, chip->fvss_active);
	/* Skip BASS if FVSS is active */
	if (chip->fvss_active)
		goto exit_soc_scale;
	if (((sys_soc - msoc) < BASS_SYS_MSOC_DELTA) ||
			chip->last_fifo_i_ua <= 0)
		goto exit_soc_scale;
	if (!chip->bass_active) {
		chip->bass_active = true;
		chip->bsoc_bass_entry = chip->batt_soc;
	}
	/* Drop the sys_soc by 1% if batt_soc has dropped */
	if ((chip->bsoc_bass_entry - chip->batt_soc) >= 100) {
		bass_soc = (msoc > 0) ? msoc - 1 : 0;
		chip->bass_active = false;
	}
	dev_dbg(chip->dev, "BASS Exit: fifo_i_ua=%d sys_soc=%d msoc=%d bsoc_bass_entry=%d batt_soc=%d bass_soc=%d\n",
			chip->last_fifo_i_ua, sys_soc, msoc,
			chip->bsoc_bass_entry, chip->batt_soc, bass_soc);
	return bass_soc;
exit_soc_scale:
	chip->bass_active = false;
	if (chip->dt.bass_enable || (qg_ss_feature & QG_BASS))
		dev_dbg(chip->dev, "BASS Quit: enabled=%d fifo_i_ua=%d sys_soc=%d msoc=%d batt_soc=%d\n",
				chip->dt.bass_enable, chip->last_fifo_i_ua,
				sys_soc, msoc, chip->batt_soc);
	return sys_soc;
}
static int qg_adjust_sys_soc(struct qpnp_qg *chip)
{
	int soc = 0, vbat_uv = 0, rc = 0;
	if (!chip)
		return -EINVAL;
	int vcutoff_uv = chip->dt.vbatt_cutoff_mv * 1000;
	chip->sys_soc = CAP(QG_MIN_SOC, QG_MAX_SOC, chip->sys_soc);
	chip->sys_soc = qg_process_tcss_soc(chip, chip->sys_soc);
	if (chip->sys_soc == QG_MAX_SOC) {
		soc = FULL_SOC;
	} else if (chip->sys_soc >= (QG_MAX_SOC - 100)) {
		if (chip->last_adj_ssoc == FULL_SOC)
			soc = FULL_SOC;
		else
			soc = FULL_SOC - 1;
	} else {
		soc = DIV_ROUND_CLOSEST(chip->sys_soc, 100);
	}
	soc = qg_process_fvss_soc(chip, soc);
	soc = qg_process_bass_soc(chip, soc);
	if (soc == 0) {
		rc = qg_get_vbat_avg(chip, &vbat_uv);
		if (!rc && (vbat_uv >= (vcutoff_uv + VBAT_LOW_HYST_UV))) {
			soc = 1;
			dev_dbg(chip->dev, "vbat_uv=%duV holding 1%% SOC\n",
					vbat_uv);
		}
	}
	dev_dbg(chip->dev, "sys_soc=%d adjusted sys_soc=%d\n",
			chip->sys_soc, soc);
	chip->last_adj_ssoc = soc;
	return soc;
}
#define MAX_FIFO_DELTA_PERCENT		10
static int qg_config_s2_state(struct qpnp_qg *chip,
		enum s2_state requested_state, bool state_enable,
		bool process_fifo)
{
	int rc = 0, acc_interval = 0, acc_length = 0;
	u8 fifo_length = 0, reg = 0, state = S2_DEFAULT;
	if (!chip)
		return -EINVAL;
	if ((chip->s2_state_mask & requested_state) && state_enable)
		return 0;
	if (!(chip->s2_state_mask & requested_state) && !state_enable)
		return 0;
	if (state_enable)
		chip->s2_state_mask |= requested_state;
	else
		chip->s2_state_mask &= ~requested_state;
	if (chip->s2_state_mask & S2_FAST_CHARGING)
		state = S2_FAST_CHARGING;
	else if (chip->s2_state_mask & S2_LOW_VBAT)
		state = S2_LOW_VBAT;
	else if (chip->s2_state_mask & S2_SLEEP)
		state = S2_SLEEP;
	else
		state = S2_DEFAULT;
	if (state == chip->s2_state)
		return 0;
	switch (state) {
	case S2_FAST_CHARGING:
		fifo_length = chip->dt.fast_chg_s2_fifo_length;
		acc_interval = chip->dt.s2_acc_intvl_ms;
		acc_length = chip->dt.s2_acc_length;
		break;
	case S2_LOW_VBAT:
		fifo_length = chip->dt.s2_vbat_low_fifo_length;
		acc_interval = chip->dt.s2_acc_intvl_ms;
		acc_length = chip->dt.s2_acc_length;
		break;
	case S2_SLEEP:
		fifo_length = chip->dt.sleep_s2_fifo_length;
		acc_interval = chip->dt.sleep_s2_acc_intvl_ms;
		acc_length = chip->dt.sleep_s2_acc_length;
		break;
	case S2_DEFAULT:
		fifo_length = chip->dt.s2_fifo_length;
		acc_interval = chip->dt.s2_acc_intvl_ms;
		acc_length = chip->dt.s2_acc_length;
		break;
	default:
		return -EINVAL;
	}
	rc = qg_master_hold(chip, true);
	if (rc < 0)
		return rc;
	if (process_fifo) {
		rc = qg_process_rt_fifo(chip);
		if (rc < 0)
			goto done;
	}
	rc = qg_update_fifo_length(chip, fifo_length);
	if (rc < 0)
		goto done;
	reg = acc_interval / 10;
	rc = qg_write(chip, QG_S2_NORMAL_MEAS_CTL3_REG,
			&reg, 1);
	if (rc < 0)
		goto done;
	reg = ilog2(acc_length) - 1;
	rc = qg_masked_write(chip, QG_S2_NORMAL_MEAS_CTL2_REG,
			NUM_OF_ACCUM_MASK, reg);
	if (rc < 0)
		goto done;
	chip->s2_state = state;
	dev_dbg(chip->dev, "S2 New state=%x  fifo_length=%d interval=%d acc_length=%d\n",
			state, fifo_length, acc_interval, acc_length);
done:
	rc = qg_master_hold(chip, false);
	if (rc < 0)
		dev_err(chip->dev, "Failed to release master hold rc=%d\n", rc);
	chip->last_fifo_update_time = ktime_get_boottime();
	return rc;
}
static int qg_fast_charge_config(struct qpnp_qg *chip)
{
	int rc = 0;
	if (!chip->dt.qg_fast_chg_cfg)
		return 0;
	rc = qg_config_s2_state(chip, S2_FAST_CHARGING,
			(chip->charge_status == POWER_SUPPLY_STATUS_CHARGING)
			? true : false, false);
	if (rc < 0)
		pr_err("Failed to config fast-charge rc=%d\n", rc);
	return rc;
}
static irqreturn_t qg_fifo_update_done_handler(int irq, void *data)
{
	ktime_t now = ktime_get_boottime();
	int rc, hw_delta_ms = 0, margin_ms = 0;
	u32 fifo_length = 0;
	s64 time_delta_ms = 0;
	struct qpnp_qg *chip = data;
	time_delta_ms = ktime_ms_delta(now, chip->last_fifo_update_time);
	chip->last_fifo_update_time = now;
	dev_dbg(chip->dev, "FIFO update done IRQ triggered\n");
	mutex_lock(&chip->data_lock);
	rc = get_fifo_length(chip, &fifo_length, false);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to get FIFO length, rc=%d\n", rc);
		goto done;
	}
	rc = qg_process_fifo(chip, fifo_length);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to process FIFO, rc=%d\n", rc);
		goto done;
	}
	if (++chip->fifo_done_count == U32_MAX)
		chip->fifo_done_count = 0;
	rc = qg_vbat_thresholds_config(chip);
	if (rc < 0)
		pr_err("Failed to apply VBAT EMPTY config rc=%d\n", rc);
	rc = qg_fast_charge_config(chip);
	if (rc < 0)
		pr_err("Failed to apply fast-charge config rc=%d\n", rc);
	rc = get_fifo_done_time(chip, false, &hw_delta_ms);
	if (rc < 0)
		hw_delta_ms = 0;
	else
		margin_ms = (hw_delta_ms * MAX_FIFO_DELTA_PERCENT) / 100;
	if (abs(hw_delta_ms - time_delta_ms) < margin_ms) {
		chip->kdata.param[QG_FIFO_TIME_DELTA].data = time_delta_ms;
		chip->kdata.param[QG_FIFO_TIME_DELTA].valid = true;
	}
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
	qg_ws_fifo_done(chip, true);
	dev_dbg(chip->dev, "FIFO processed: length=%u time_delta=%lld ms\n",
			fifo_length, time_delta_ms);
done:
	mutex_unlock(&chip->data_lock);
	if (rc < 0)
		qg_ws_fifo_done(chip, false);
	return IRQ_HANDLED;
}
static irqreturn_t qg_good_ocv_handler(int irq, void *data)
{
	struct qpnp_qg *chip = data;
	u8 status = 0;
	u32 ocv_uv = 0, ocv_raw = 0;
	int rc = 0;
	unsigned long rtc_sec = 0;
	mutex_lock(&chip->data_lock);
	rc = qg_read(chip, QG_STATUS2_REG, &status, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read status, rc=%d\n", rc);
		goto done;
	}
	if (!(status & GOOD_OCV_BIT))
		goto done;
	rc = qg_read_ocv(chip, &ocv_uv, &ocv_raw, S3_GOOD_OCV);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read good OCV, rc=%d\n", rc);
		goto done;
	}
	rc = get_rtc_time(&rtc_sec);
	if (rc < 0)
		dev_warn(chip->dev, "Failed to read RTC time rc=%d, using 0\n", rc);
	chip->kdata.fifo_time = (u32)rtc_sec;
	chip->kdata.param[QG_GOOD_OCV_UV].data = ocv_uv;
	chip->kdata.param[QG_GOOD_OCV_UV].valid = true;
	qg_ws_good_ocv(chip, true);
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
done:
	mutex_unlock(&chip->data_lock);
	return IRQ_HANDLED;
}
static irqreturn_t qg_vbat_empty_handler(int irq, void *data)
{
	struct qpnp_qg *chip = data;
	u32 ocv_uv = 0;
	int rc;
	u8 status = 0;
	dev_dbg(chip->dev, "VBAT empty IRQ triggered\n");
    /*
     * Read STATUS2 register to check if battery is missing.
     * Note: upstream qg_read() adds chip->qg_base internally,
     * so pass only the register offset here.
     * BATTERY_MISSING_INT_RT_STS_BIT is BIT(0) of STATUS2.
     */
	rc = qg_read(chip, QG_INT_RT_STS_REG, &status, 1);
	if (rc < 0)
		pr_err("Failed to read STATUS2 rc=%d\n", rc);
	/* Ignore VBAT empty if battery is physically missing */
	if ((status & BATTERY_MISSING_INT_RT_STS_BIT) ||
				chip->battery_missing)
		return IRQ_HANDLED;
	pr_warn("VBATT EMPTY: forcing SOC to 0\n");
	/* Force SOC to 0 and scale immediately */
	chip->catch_up_soc = 0;
	qg_scale_soc(chip, true);
	/* Persist SOC=0 to SDAM */
	rc = qg_sdam_read(chip, SDAM_OCV_UV, &ocv_uv);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read SDAM OCV rc=%d\n", rc);
	chip->sdam_data[SDAM_SOC]    = 0;
	chip->sdam_data[SDAM_OCV_UV] = ocv_uv;
	chip->sdam_data[SDAM_VALID]  = 1;
	rc = qg_store_soc_params(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to store SOC params rc=%d\n", rc);
	if (chip->qg_psy)
		power_supply_changed(chip->qg_psy);
	return IRQ_HANDLED;
}
static int qg_sdam_write(struct qpnp_qg *chip, u8 param, u32 data)
{
	int rc = 0;
	u32 offset = 0;
	size_t length = 0;
	u8 buf[4];
	if (!chip || !chip->sdam.regmap)
		return -EINVAL;
	if (param >= SDAM_MAX)
		return -EINVAL;
	offset = chip->sdam.sdam_base + sdam_info[param].offset;
	length = sdam_info[param].length;
	put_unaligned_le32(data, buf);
	rc = regmap_bulk_write(chip->sdam.regmap, offset, buf, length);
	return rc;
}
static int qg_sdam_read(struct qpnp_qg *chip, u8 param, u32 *data)
{
	int rc = 0;
	u32 offset = 0;
	size_t length = 0;
	if (!chip || !data)
		return -EINVAL;
	if (param >= SDAM_MAX)
		return -EINVAL;
	*data = 0;
	offset = chip->sdam.sdam_base + sdam_info[param].offset;
	length = sdam_info[param].length;
	rc = regmap_raw_read(chip->sdam.regmap, offset, (u8 *)data, length);
	return rc;
}
static int qg_sdam_read_all(struct qpnp_qg *chip, u32 *sdam_data)
{
	int i = 0, rc = 0;
	if (!chip || !sdam_data)
		return -EINVAL;
	for (i = 0; i < SDAM_MAX; i++) {
		rc = qg_sdam_read(chip, i, &sdam_data[i]);
		if (rc < 0)
			return rc;
	}
	return 0;
}
static int qg_sdam_clear(struct qpnp_qg *chip)
{
	u8 buf[SDAM_MAX_OFFSET - SDAM_MIN_OFFSET + 1] = { 0 };
	if (!chip || !chip->sdam.regmap)
		return -EINVAL;
	return regmap_bulk_write(chip->sdam.regmap,
			chip->sdam.sdam_base + SDAM_MIN_OFFSET,
			buf, sizeof(buf));
}
static int qg_sdam_init(struct qpnp_qg *chip)
{
	int rc = 0;
	u32 base = 0, type = 0;
	struct device *dev = chip->dev;
	struct device_node *child, *node = dev->of_node;
	chip->sdam.regmap = dev_get_regmap(dev->parent, NULL);
	if (!chip->sdam.regmap)
		return -ENXIO;
	node = of_get_parent(node);
	if (!node)
		return -EINVAL;
	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0)
			continue;
		rc = regmap_read(chip->sdam.regmap, base + PERPH_TYPE_REG, &type);
		if (rc < 0)
			continue;
		if (type == SDAM_TYPE) {
			chip->sdam.sdam_base = base;
			break;
		}
	}
	of_node_put(node);
	if (!chip->sdam.sdam_base)
		return -EINVAL;
	return 0;
}
static int qg_store_soc_params(struct qpnp_qg *chip)
{
	int rc, batt_temp = 0, i = 0;
	u32 flash_ocv = 0;
	unsigned long rtc_sec = 0;
	if (!chip)
		return -EINVAL;
	rc = get_rtc_time(&rtc_sec);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read RTC time rc=%d\n", rc);
	else
		chip->sdam_data[SDAM_TIME_SEC] = rtc_sec;
	rc = qg_get_battery_temp(chip, &batt_temp);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read batt_temp rc=%d\n", rc);
	else
		chip->sdam_data[SDAM_TEMP] = (u32)batt_temp;
	for (i = 0; i <= SDAM_TIME_SEC; i++) {
		rc = qg_sdam_write(chip, i, chip->sdam_data[i]);
		dev_dbg(chip->dev, "SDAM write param %d value=%d\n",
				i, chip->sdam_data[i]);
		if (rc < 0)
			dev_err(chip->dev, "Failed to write SDAM param rc=%d\n", rc);
	}
	/* store the SDAM OCV */
	flash_ocv = chip->sdam_data[SDAM_OCV_UV] / 20000;
	rc = qg_sdam_write(chip, SDAM_FLASH_OCV, flash_ocv);
	if (rc < 0)
		dev_err(chip->dev, "Failed to write SDAM_FLASH_OCV rc=%d\n", rc);
	return rc;
}
static void scale_soc_stop(struct qpnp_qg *chip)
{
	chip->next_wakeup_ms = 0;
	alarm_cancel(&chip->alarm_timer);
	dev_dbg(chip->dev, "SOC scale stopped: msoc=%d catch_up_soc=%d\n",
			chip->msoc, chip->catch_up_soc);
}
static bool is_scaling_required(struct qpnp_qg *chip)
{
	bool input_present = is_input_present(chip);
	if (!chip->profile_loaded)
		return false;
	if (chip->maint_soc > 0 &&
			(abs(chip->maint_soc - chip->msoc) >= chip->dt.delta_soc)) {
		return true;
	}
	if ((abs(chip->catch_up_soc - chip->msoc) < chip->dt.delta_soc) &&
			chip->catch_up_soc != 0 && chip->catch_up_soc != 100) {
		return false;
	}
	if (chip->catch_up_soc == chip->msoc)
		return false;
	if (chip->catch_up_soc > chip->msoc && !input_present)
		return false;
	if (chip->catch_up_soc > chip->msoc && input_present &&
			(chip->charge_status != POWER_SUPPLY_STATUS_CHARGING &&
			 chip->charge_status != POWER_SUPPLY_STATUS_FULL)) {
		return false;
	}
	return true;
}
static bool maint_soc_timeout(struct qpnp_qg *chip)
{
	unsigned long now = 0;
	int rc = 0;
	if (chip->maint_soc < 0)
		return false;
	rc = get_rtc_time(&now);
	if (rc < 0)
		return false;
	if (chip->maint_soc < chip->recharge_soc)
		return true;
	if ((now - chip->last_maint_soc_update_time) >=
			(qg_maint_soc_update_ms / 1000)) {
		chip->last_maint_soc_update_time = now;
		return true;
	}
	return false;
}
static void update_msoc(struct qpnp_qg *chip)
{
	int rc = 0, sdam_soc = 0;
	bool input_present = is_input_present(chip);
	if (chip->catch_up_soc > chip->msoc) {
		if (input_present)
			chip->msoc += chip->dt.delta_soc;
	} else if (chip->catch_up_soc < chip->msoc) {
		chip->msoc -= chip->dt.delta_soc;
	}
	chip->msoc = CAP(0, 100, chip->msoc);
	if (chip->maint_soc > 0 && chip->msoc < chip->maint_soc
			&& maint_soc_timeout(chip)) {
		chip->maint_soc -= chip->dt.delta_soc;
		chip->maint_soc = CAP(0, 100, chip->maint_soc);
	}
	if (chip->maint_soc <= chip->msoc)
		chip->maint_soc = -EINVAL;
	rc = qg_write_monotonic_soc(chip, chip->msoc);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update MSOC register rc=%d\n", rc);
	sdam_soc = (chip->maint_soc > 0) ? chip->maint_soc : chip->msoc;
	chip->sdam_data[SDAM_SOC] = sdam_soc;
	rc = qg_sdam_write(chip, SDAM_SOC, sdam_soc);
	if (rc < 0)
		dev_err(chip->dev, "Failed to write SDAM SOC rc=%d\n", rc);
}
static int get_fifo_done_time(struct qpnp_qg *chip, bool rt, int *time_ms)
{
	int rc = 0, length = 0;
	u32 sample_count = 0, sample_interval = 0, acc_count = 0;
	if (!chip || !time_ms)
		return -EINVAL;
	rc = get_fifo_length(chip, &length, rt ? true : false);
	if (rc < 0)
		return rc;
	rc = get_sample_count(chip, &sample_count);
	if (rc < 0)
		return rc;
	rc = get_sample_interval(chip, &sample_interval);
	if (rc < 0)
		return rc;
	*time_ms = length * sample_count * sample_interval;
	if (rt) {
		rc = qg_read(chip, QG_ACCUM_CNT_RT_REG,
				(u8 *)&acc_count, 1);
		if (rc < 0)
			return rc;
		*time_ms += ((sample_count - acc_count) * sample_interval);
	}
	return 0;
}
static void get_next_update_time(struct qpnp_qg *chip)
{
	int soc_points = 0, batt_temp = 0;
	int min_delta_soc_interval_ms = qg_delta_soc_interval_ms;
	int rc = 0, rt_time_ms = 0, full_time_ms = DEFAULT_UPDATE_TIME_MS;
	get_fifo_done_time(chip, false, &full_time_ms);
	get_fifo_done_time(chip, true, &rt_time_ms);
	full_time_ms = CAP(0, DEFAULT_UPDATE_TIME_MS,
			full_time_ms - rt_time_ms);
	soc_points = abs(chip->msoc - chip->catch_up_soc);
	if (chip->maint_soc > 0)
		soc_points = max(abs(chip->msoc - chip->maint_soc), soc_points);
	soc_points /= chip->dt.delta_soc;
	rc = qg_get_battery_temp(chip, &batt_temp);
	if (!rc && batt_temp < chip->dt.cold_temp_threshold)
		min_delta_soc_interval_ms = qg_delta_soc_cold_interval_ms;
	else if (chip->maint_soc > 0 && chip->maint_soc >= chip->recharge_soc)
		min_delta_soc_interval_ms = qg_maint_soc_update_ms;
	else if (chip->fvss_active)
		min_delta_soc_interval_ms = qg_fvss_delta_soc_interval_ms;
	if (!min_delta_soc_interval_ms)
		min_delta_soc_interval_ms = 1000;
	chip->next_wakeup_ms = (full_time_ms / (soc_points + 1))
		- SOC_SCALE_HYST_MS;
	chip->next_wakeup_ms = max(chip->next_wakeup_ms,
			min_delta_soc_interval_ms);
	dev_dbg(chip->dev, "fifo_full_time=%d secs fifo_real_time=%d secs soc_scale_points=%d\n",
			full_time_ms / 1000, rt_time_ms / 1000, soc_points);
}
static int qg_scale_soc(struct qpnp_qg *chip, bool force_soc)
{
	int rc = 0;
	if (!chip)
		return -EINVAL;
	mutex_lock(&chip->soc_lock);
	dev_dbg(chip->dev, "SOC scale: Start msoc=%d catch_up_soc=%d delta_soc=%d\n",
			chip->msoc, chip->catch_up_soc, chip->dt.delta_soc);
	if (force_soc) {
		chip->msoc = chip->catch_up_soc;
		rc = qg_write_monotonic_soc(chip, chip->msoc);
		if (rc < 0)
			dev_err(chip->dev, "Failed to update MSOC register rc=%d\n", rc);
		dev_dbg(chip->dev, "SOC scale: Forced msoc=%d\n", chip->msoc);
		goto done_psy;
	}
	if (!is_scaling_required(chip)) {
		scale_soc_stop(chip);
		goto done;
	}
	update_msoc(chip);
	if (is_scaling_required(chip)) {
		get_next_update_time(chip);
		alarm_start_relative(&chip->alarm_timer,
				ms_to_ktime(chip->next_wakeup_ms));
	} else {
		scale_soc_stop(chip);
		goto done_psy;
	}
	dev_dbg(chip->dev, "SOC scale: msoc=%d catch_up_soc=%d delta_soc=%d next_wakeup=%d sec\n",
			chip->msoc, chip->catch_up_soc, chip->dt.delta_soc,
			chip->next_wakeup_ms / 1000);
done_psy:
	power_supply_changed(chip->qg_psy);
done:
	mutex_unlock(&chip->soc_lock);
	return rc;
}
static void process_udata_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work, struct qpnp_qg, udata_work);
	int rc = 0;
	if (chip->udata.param[QG_CC_SOC].valid) {
		chip->cc_soc = chip->udata.param[QG_CC_SOC].data;
		dev_dbg(chip->dev, "updated cc_soc=%d\n", chip->cc_soc);
	}
	if (chip->udata.param[QG_BATT_SOC].valid) {
		chip->batt_soc = chip->udata.param[QG_BATT_SOC].data;
		dev_dbg(chip->dev, "updated batt_soc=%d\n", chip->batt_soc);
	}
	if (chip->udata.param[QG_FULL_SOC].valid) {
		chip->full_soc = chip->udata.param[QG_FULL_SOC].data;
		dev_dbg(chip->dev, "updated full_soc=%d\n", chip->full_soc);
	}
	if (chip->udata.param[QG_V_IBAT].valid) {
		chip->qg_v_ibat = chip->udata.param[QG_V_IBAT].data;
		dev_dbg(chip->dev, "updated qg_v_ibat=%d\n", chip->qg_v_ibat);
	}
	if (chip->udata.param[QG_SOC].valid ||
			chip->udata.param[QG_SYS_SOC].valid) {
		if (chip->udata.param[QG_SYS_SOC].valid) {
			chip->sys_soc = chip->udata.param[QG_SYS_SOC].data;
			dev_dbg(chip->dev, "updated sys_soc=%d\n", chip->sys_soc);
			chip->catch_up_soc = qg_adjust_sys_soc(chip);
			dev_dbg(chip->dev, "updated catch_up_soc(adjusted)=%d\n",
					chip->catch_up_soc);
		} else {
			chip->catch_up_soc = chip->udata.param[QG_SOC].data;
			dev_dbg(chip->dev, "updated catch_up_soc=%d\n",
					chip->catch_up_soc);
		}
		rc = qg_scale_soc(chip, chip->force_soc);
		if (rc < 0)
			dev_err(chip->dev, "Failed to scale SOC rc=%d\n", rc);
		else
			chip->force_soc = false;
		chip->sdam_data[SDAM_SOC] = chip->msoc;
		chip->sdam_data[SDAM_OCV_UV] = chip->udata.param[QG_OCV_UV].data;
		chip->sdam_data[SDAM_RBAT_MOHM] = chip->udata.param[QG_RBAT_MOHM].data;
		chip->sdam_data[SDAM_VALID] = 1;
		rc = qg_store_soc_params(chip);
		if (rc < 0)
			dev_err(chip->dev, "Failed to store SOC params rc=%d\n", rc);
	}
	qg_ws_udata_ready(chip, false);
}
static bool is_cp_available(struct qpnp_qg *chip)
{
	if (chip->cp_psy)
		return true;
	chip->cp_psy = power_supply_get_by_name("charge_pump_master");
	if (!chip->cp_psy)
		return false;
	return true;
}
static bool is_parallel_available(struct qpnp_qg *chip)
{
	return is_chan_valid(chip, PARALLEL_CHARGING_ENABLED);
}
static bool is_parallel_enabled(struct qpnp_qg *chip)
{
	int val = 0;
	if (is_parallel_available(chip))
		qg_read_iio_chan(chip, PARALLEL_CHARGING_ENABLED, &val);
	else if (is_cp_available(chip))
		qg_read_iio_chan(chip, CP_CHARGING_ENABLED, &val);
	return val ? true : false;
}
#define MIN_FIFO_FULL_TIME_MS   12000
static int process_rt_fifo_data(struct qpnp_qg *chip, bool update_smb)
{
	int rc = 0;
	bool master_held = false;
	ktime_t now = ktime_get_boottime();
	s64 time_delta = 0;
	if (!chip)
		return -EINVAL;
	time_delta = ktime_ms_delta(now, chip->last_user_update_time);
	if (time_delta > MIN_FIFO_FULL_TIME_MS || update_smb) {
		rc = qg_master_hold(chip, true);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to hold master, rc=%d\n", rc);
			goto done;
		}
		master_held = true;
		rc = qg_process_rt_fifo(chip);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to process FIFO real-time, rc=%d\n", rc);
			goto done;
		}
		if (update_smb) {
			rc = qg_masked_write(chip,
					QG_MODE_CTL1_REG,
					PARALLEL_IBAT_SENSE_EN_BIT,
					chip->parallel_enabled ?
					PARALLEL_IBAT_SENSE_EN_BIT : 0);
			if (rc < 0) {
				dev_err(chip->dev, "Failed to update SMB_EN, rc=%d\n", rc);
				goto done;
			}
			dev_dbg(chip->dev, "Parallel SENSE %d\n", chip->parallel_enabled);
		}
		rc = qg_master_hold(chip, false);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to release master, rc=%d\n", rc);
			goto done;
		}
		master_held = false;
		chip->last_fifo_update_time = ktime_get_boottime();
		chip->data_ready = true;
		wake_up_interruptible(&chip->qg_wait_q);
		chip->last_user_update_time = now;
		qg_ws_fifo_rt_done(chip, true);
	} else {
		dev_dbg(chip->dev, "FIFO processing too early time_delta=%lld\n",
				time_delta);
	}
done:
	/* Only release master hold if we acquired it and haven't released it yet */
	if (master_held)
		qg_master_hold(chip, false);
	return rc;
}
static int qg_parallel_status_update(struct qpnp_qg *chip)
{
	int rc = 0;
	bool parallel_enabled = is_parallel_enabled(chip);
	bool update_smb = false;
	if (parallel_enabled == chip->parallel_enabled)
		return 0;
	chip->parallel_enabled = parallel_enabled;
	dev_dbg(chip->dev, "Parallel status changed Enabled=%d\n", parallel_enabled);
	mutex_lock(&chip->data_lock);
	if (is_cp_available(chip))
		update_smb = chip->dt.use_cp_iin_sns ? true : false;
	else if (is_parallel_available(chip))
		update_smb = chip->dt.qg_ext_sense ? false : true;
	rc = process_rt_fifo_data(chip, update_smb);
	if (rc < 0)
		dev_err(chip->dev, "Failed to process RT FIFO data rc=%d\n", rc);
	mutex_unlock(&chip->data_lock);
	return 0;
}
static int qg_input_status_update(struct qpnp_qg *chip)
{
	bool usb_present = is_chg_usb_present(chip);
	bool dc_present  = is_dc_present(chip);
	int rc = 0;
	if ((chip->usb_present != usb_present) ||
			(chip->dc_present != dc_present)) {
		rc = qg_scale_soc(chip, false);
		if (rc < 0)
			return rc;
	}
	chip->usb_present = usb_present;
	chip->dc_present = dc_present;
	return 0;
}
static void qg_status_change_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work, struct qpnp_qg,
			qg_status_change_work);
	union power_supply_propval val = {0};
	ktime_t now = ktime_get_boottime();
	int rc = 0;
	if (!chip->chg_psy) {
		chip->chg_psy = power_supply_get_by_name("pm4125-charger");
		if (!chip->chg_psy) {
			dev_err(chip->dev, "charger psy not resolved\n");
			return;
		}
	}
	rc = power_supply_get_property(chip->chg_psy,
			POWER_SUPPLY_PROP_STATUS, &val);
	if (rc == 0)
		chip->charge_status = val.intval;
	rc = power_supply_get_property(chip->chg_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &val);
	if (rc == 0)
		chip->charge_type = val.intval;
	rc = qg_read_iio_chan(chip, CHARGE_DONE, &val.intval);
	if (rc < 0)
		dev_err(chip->dev, "Failed to get charge done status, rc=%d\n", rc);
	else
		chip->charge_done = val.intval;
	dev_dbg(chip->dev, "charge_status=%d charge_type=%d charge_done=%d\n",
			chip->charge_status, chip->charge_type, chip->charge_done);
	rc = qg_parallel_status_update(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update parallel-status, rc=%d\n", rc);
	rc = qg_input_status_update(chip);
	if (rc < 0)
		dev_err(chip->dev, "Failed to update input status rc=%d\n", rc);
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
	chip->last_user_update_time = now;
	pm_relax(chip->dev);
}
static int qg_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct qpnp_qg *chip = container_of(nb, struct qpnp_qg, nb);
	if (!psy || !psy->desc || !psy->desc->name)
		return NOTIFY_OK;
	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;
	if (strcmp(psy->desc->name, "bms") == 0 ||
			(strcmp(psy->desc->name, "pm4125-charger") == 0) ||
			strcmp(psy->desc->name, "usb") == 0 ||
			strcmp(psy->desc->name, "dc") == 0 ||
			strcmp(psy->desc->name, "parallel") == 0 ||
			strcmp(psy->desc->name, "charge_pump_master") == 0) {
		pm_stay_awake(chip->dev);
		schedule_work(&chip->qg_status_change_work);
	}
	return NOTIFY_OK;
}
static int qg_request_irqs(struct qpnp_qg *chip)
{
	int rc = 0, i = 0, irq = 0;
	if (!chip || !chip->dev)
		return -EINVAL;
	struct device *dev = chip->dev;
	for (i = 0; i < ARRAY_SIZE(qg_irqs); i++) {
		irq = platform_get_irq_byname(to_platform_device(dev),
				qg_irqs[i].name);
		if (irq < 0) {
			dev_err(dev, "Failed to get IRQ %s\n",
					qg_irqs[i].name);
			return irq;
		}
		rc = devm_request_threaded_irq(dev, irq, NULL,
				qg_irqs[i].handler,
				IRQF_ONESHOT,
				qg_irqs[i].name, chip);
		if (rc < 0) {
			dev_err(dev, "Failed to request IRQ %s, rc=%d\n",
					qg_irqs[i].name, rc);
			return rc;
		}
		qg_irqs[i].irq = irq;
		if (qg_irqs[i].wake)
			enable_irq_wake(irq);
		dev_dbg(dev, "Registered IRQ %s (wakeable=%d)\n",
				qg_irqs[i].name, qg_irqs[i].wake);
	}
	return 0;
}
#define DEFAULT_VBATT_EMPTY_MV		3200
#define DEFAULT_VBATT_EMPTY_COLD_MV	3000
#define DEFAULT_VBATT_CUTOFF_MV		3400
#define DEFAULT_VBATT_LOW_MV		3500
#define DEFAULT_VBATT_LOW_COLD_MV	3800
#define DEFAULT_ITERM_MA		100
#define DEFAULT_DELTA_SOC		1
#define DEFAULT_SHUTDOWN_SOC_SECS	360
#define DEFAULT_COLD_TEMP_THRESHOLD	0
#define DEFAULT_SHUTDOWN_TEMP_DIFF	60
#define DEFAULT_SLEEP_TIME_SECS		1800
#define DEFAULT_SYS_MIN_VOLT_MV		2800
#define DEFAULT_FVSS_VBAT_MV		3500
#define DEFAULT_TCSS_ENTRY_SOC		90
static int qg_parse_dt(struct qpnp_qg *chip)
{
	u32 base = 0, temp = 0;
	int rc = 0;
	u8 type = 0;
	if (!chip || !chip->dev || !chip->dev->of_node)
		return -EINVAL;
	struct device_node *child, *node = chip->dev->of_node;
	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0)
			return rc;
		rc = qg_read(chip, base + PERPH_TYPE_REG, &type, 1);
		if (rc < 0)
			return rc;
		switch (type) {
		case QG_TYPE:
			chip->qg_base = base;
			break;
		default:
			break;
		}
	}
	if (!chip->qg_base)
		return -EINVAL;
	rc = of_property_read_u32(node, "qcom,s3-entry-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s3_entry_fifo_length = -EINVAL;
	else
		chip->dt.s3_entry_fifo_length = temp;
	rc = of_property_read_u32(node, "qcom,s3-entry-ibat-ua", &temp);
	if (rc < 0)
		chip->dt.s3_entry_ibat_ua = -EINVAL;
	else
		chip->dt.s3_entry_ibat_ua = temp;
	rc = of_property_read_u32(node, "qcom,s3-exit-ibat-ua", &temp);
	if (rc < 0)
		chip->dt.s3_exit_ibat_ua = -EINVAL;
	else
		chip->dt.s3_exit_ibat_ua = temp;
	rc = of_property_read_u32(node, "qcom,vbatt-empty-mv", &temp);
	chip->dt.vbatt_empty_mv = (rc < 0) ? DEFAULT_VBATT_EMPTY_MV : temp;
	rc = of_property_read_u32(node, "qcom,vbatt-empty-cold-mv", &temp);
	chip->dt.vbatt_empty_cold_mv = (rc < 0) ? DEFAULT_VBATT_EMPTY_COLD_MV : temp;
	rc = of_property_read_u32(node, "qcom,vbatt-low-mv", &temp);
	chip->dt.vbatt_low_mv = (rc < 0) ? DEFAULT_VBATT_LOW_MV : temp;
	rc = of_property_read_u32(node, "qcom,vbatt-low-cold-mv", &temp);
	chip->dt.vbatt_low_cold_mv = (rc < 0) ? DEFAULT_VBATT_LOW_COLD_MV   : temp;
	rc = of_property_read_u32(node, "qcom,vbatt-cutoff-mv", &temp);
	chip->dt.vbatt_cutoff_mv = (rc < 0) ? DEFAULT_VBATT_CUTOFF_MV  : temp;
	rc = of_property_read_u32(node, "qcom,cold-temp-threshold", &temp);
	chip->dt.cold_temp_threshold = (rc < 0) ? 0 : temp;
	rc = of_property_read_u32(node, "qcom,s2-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s2_fifo_length = DEFAULT_S2_FIFO_LENGTH;
	else
		chip->dt.s2_fifo_length = temp;
	if (chip->dt.s2_fifo_length > chip->max_fifo_length)
		return -EINVAL;
	rc = of_property_read_u32(node, "qcom,s2-vbat-low-fifo-length", &temp);
	if (rc < 0)
		chip->dt.s2_vbat_low_fifo_length = DEFAULT_S2_VBAT_LOW_LENGTH;
	else
		chip->dt.s2_vbat_low_fifo_length = temp;
	rc = of_property_read_u32(node, "qcom,s2-acc-length", &temp);
	if (rc < 0)
		chip->dt.s2_acc_length = DEFAULT_S2_ACC_LENGTH;
	else
		chip->dt.s2_acc_length = temp;
	rc = of_property_read_u32(node, "qcom,s2-acc-interval-ms", &temp);
	if (rc < 0)
		chip->dt.s2_acc_intvl_ms = DEFAULT_S2_ACC_INTVL_MS;
	else
		chip->dt.s2_acc_intvl_ms = temp;
	dev_dbg(chip->dev, "DT: S2 FIFO length=%d low_vbat_length=%d acc_length=%d acc_interval=%d\n",
			chip->dt.s2_fifo_length, chip->dt.s2_vbat_low_fifo_length,
			chip->dt.s2_acc_length, chip->dt.s2_acc_intvl_ms);
	if (of_property_read_bool(node, "qcom,qg-sleep-config")) {
		chip->dt.qg_sleep_config = true;
		rc = of_property_read_u32(node,
				"qcom,sleep-s2-fifo-length", &temp);
		if (rc < 0)
			chip->dt.sleep_s2_fifo_length =
				DEFAULT_SLEEP_S2_FIFO_LENGTH;
		else
			chip->dt.sleep_s2_fifo_length = temp;
		if (chip->dt.s2_fifo_length > chip->max_fifo_length)
			return -EINVAL;
		rc = of_property_read_u32(node,
				"qcom,sleep-s2-acc-length", &temp);
		if (rc < 0)
			chip->dt.sleep_s2_acc_length =
				DEFAULT_SLEEP_S2_ACC_LENGTH;
		else
			chip->dt.sleep_s2_acc_length = temp;
		rc = of_property_read_u32(node,
				"qcom,sleep-s2-acc-intvl-ms", &temp);
		if (rc < 0)
			chip->dt.sleep_s2_acc_intvl_ms =
				DEFAULT_SLEEP_S2_ACC_INTVL_MS;
		else
			chip->dt.sleep_s2_acc_intvl_ms = temp;
	}
	if (of_property_read_bool(node, "qcom,qg-fast-chg-config")) {
		chip->dt.qg_fast_chg_cfg = true;
		rc = of_property_read_u32(node,
				"qcom,fast-chg-s2-fifo-length", &temp);
		if (rc < 0)
			chip->dt.fast_chg_s2_fifo_length =
				DEFAULT_FAST_CHG_S2_FIFO_LENGTH;
		else
			chip->dt.fast_chg_s2_fifo_length = temp;
		if (chip->dt.fast_chg_s2_fifo_length > chip->max_fifo_length)
			return -EINVAL;
	}
	rc = of_property_read_u32(node, "qcom,ocv-timer-expiry-min", &temp);
	chip->dt.ocv_timer_expiry_min = (rc < 0) ? -EINVAL : temp;
	rc = of_property_read_u32(node, "qcom,ocv-tol-threshold-uv", &temp);
	chip->dt.ocv_tol_threshold_uv = (rc < 0) ? -EINVAL : temp;
	rc = of_property_read_u32(node, "qcom,qg-iterm-ma", &temp);
	if (rc < 0)
		chip->dt.iterm_ma = DEFAULT_ITERM_MA;
	else
		chip->dt.iterm_ma = temp;
	rc = of_property_read_u32(node, "qcom,delta-soc", &temp);
	if (rc < 0)
		chip->dt.delta_soc = DEFAULT_DELTA_SOC;
	else
		chip->dt.delta_soc = temp;
	rc = of_property_read_u32(node, "qcom,ignore-shutdown-soc-secs", &temp);
	if (rc < 0)
		chip->dt.ignore_shutdown_soc_secs = DEFAULT_SHUTDOWN_SOC_SECS;
	else
		chip->dt.ignore_shutdown_soc_secs = temp;
	rc = of_property_read_u32(node, "qcom,shutdown-temp-diff", &temp);
	if (rc < 0)
		chip->dt.shutdown_temp_diff = DEFAULT_SHUTDOWN_TEMP_DIFF;
	else
		chip->dt.shutdown_temp_diff = temp;
	chip->dt.hold_soc_while_full = of_property_read_bool(node,
			"qcom,hold-soc-while-full");
	rc = of_property_read_u32(node, "qcom,rbat-conn-mohm", &temp);
	if (rc < 0)
		chip->dt.rbat_conn_mohm = 0;
	else
		chip->dt.rbat_conn_mohm = (int)temp;
	rc = of_property_read_u32(node, "qcom,shutdown-soc-threshold", &temp);
	if (rc < 0)
		chip->dt.shutdown_soc_threshold = -EINVAL;
	else
		chip->dt.shutdown_soc_threshold = temp;
	rc = of_property_read_u32(node, "qcom,qg-sys-min-voltage", &temp);
	if (rc < 0)
		chip->dt.sys_min_volt_mv = DEFAULT_SYS_MIN_VOLT_MV;
	else
		chip->dt.sys_min_volt_mv = temp;
	chip->dt.qg_ext_sense = of_property_read_bool(node, "qcom,qg-ext-sns");
	chip->dt.use_cp_iin_sns = of_property_read_bool(node,
			"qcom,use-cp-iin-sns");
	chip->dt.use_s7_ocv = of_property_read_bool(node, "qcom,qg-use-s7-ocv");
	rc = of_property_read_u32(node, "qcom,min-sleep-time-secs", &temp);
	if (rc < 0)
		chip->dt.min_sleep_time_secs = DEFAULT_SLEEP_TIME_SECS;
	else
		chip->dt.min_sleep_time_secs = temp;
	if (of_property_read_bool(node, "qcom,fvss-enable")) {
		chip->dt.fvss_enable = true;
		rc = of_property_read_u32(node,
				"qcom,fvss-vbatt-mv", &temp);
		if (rc < 0)
			chip->dt.fvss_vbat_mv = DEFAULT_FVSS_VBAT_MV;
		else
			chip->dt.fvss_vbat_mv = temp;
	}
	if (of_property_read_bool(node, "qcom,tcss-enable")) {
		chip->dt.tcss_enable = true;
		rc = of_property_read_u32(node,
				"qcom,tcss-entry-soc", &temp);
		if (rc < 0)
			chip->dt.tcss_entry_soc = DEFAULT_TCSS_ENTRY_SOC;
		else
			chip->dt.tcss_entry_soc = temp;
	}
	chip->dt.bass_enable = of_property_read_bool(node, "qcom,bass-enable");
	return 0;
}
#define BID_RPULL_OHM		100000
#define BID_VREF_MV		1875
static int qg_get_batt_id(struct qpnp_qg *chip)
{
	int rc = 0, batt_id_mv = 0;
	s64 denom = 0;
	if (!chip)
		return -EINVAL;
	if (!chip->batt_id_chan)
		return -EINVAL;
	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id_mv);
	if (rc < 0)
		return rc;
	batt_id_mv = div_s64(batt_id_mv, 1000);
	if (batt_id_mv == 0) {
		chip->batt_id_ohm = 0;
		return 0;
	}
	denom = div_s64(BID_VREF_MV * 1000, batt_id_mv) - 1000;
	if (denom <= 0) {
		chip->batt_id_ohm = 0;
		return 0;
	}
	chip->batt_id_ohm = div_s64(BID_RPULL_OHM * 1000 + denom / 2, denom);
	dev_info(chip->dev, "Battery ID: %d mV -> %d ohm\n",
			batt_id_mv, chip->batt_id_ohm);
	return 0;
}
#define SDAM_MAGIC_NUMBER		0x12345678
static int qg_sanitize_sdam(struct qpnp_qg *chip)
{
	int rc = 0;
	u32 data = 0;
	if (!chip)
		return -EINVAL;
	rc = qg_sdam_read(chip, SDAM_MAGIC, &data);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read SDAM rc=%d\n", rc);
		return rc;
	}
	if (data == SDAM_MAGIC_NUMBER) {
		dev_dbg(chip->dev, "SDAM valid\n");
	} else if (data == 0) {
		rc = qg_sdam_write(chip, SDAM_MAGIC, SDAM_MAGIC_NUMBER);
		if (!rc)
			dev_dbg(chip->dev, "First boot. SDAM initialized\n");
		chip->first_profile_load = true;
	} else {
		/* SDAM has invalid value */
		rc = qg_sdam_clear(chip);
		if (!rc) {
			dev_err(chip->dev, "SDAM uninitialized, SDAM reset\n");
			rc = qg_sdam_write(chip, SDAM_MAGIC, SDAM_MAGIC_NUMBER);
		}
		chip->first_profile_load = true;
	}
	if (rc < 0)
		dev_err(chip->dev, "Failed in SDAM operation, rc=%d\n", rc);
	return rc;
}
static int qg_set_wa_flags(struct qpnp_qg *chip)
{
	if (!chip)
		return -EINVAL;
	switch (chip->pmic_version) {
	case PM4125:
		chip->wa_flags |= QG_CLK_ADJUST_WA |
			QG_RECHARGE_SOC_WA |
			QG_VBAT_LOW_WA;
		break;
	default:
		dev_err(chip->dev, "Unsupported PMIC version %d\n",
				chip->pmic_version);
		return -EINVAL;
	}
	return 0;
}
#define ADC_CONV_DLY_512MS		0xA
#define IBAT_5A_FCC_MA			4800
#define IBAT_10A_FCC_MA			9600
#define QGAUGE_INT_EN_SET_REG		0x15
#define FIFO_UPDATE_DONE_INT_EN_BIT	BIT(3)
#define QG_EVENT_INT_EN_BIT		BIT(5)
static int qg_vbat_thresholds_config(struct qpnp_qg *chip)
{
	int rc, temp = 0, vbat_mv = 0;
	u8 reg = 0;
	if (!chip)
		return -EINVAL;
	rc = qg_get_battery_temp(chip, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read batt_temp rc=%d\n", rc);
		return rc;
	}
	vbat_mv = (temp < chip->dt.cold_temp_threshold) ?
		chip->dt.vbatt_empty_cold_mv :
		chip->dt.vbatt_empty_mv;
	rc = qg_read(chip, QG_VBAT_EMPTY_THRESHOLD_REG,
			&reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read vbat-empty, rc=%d\n", rc);
		return rc;
	}
	if (vbat_mv == (reg * 50))
		goto config_vbat_low;
	reg = vbat_mv / 50;
	rc = qg_write(chip, QG_VBAT_EMPTY_THRESHOLD_REG,
			&reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to write vbat-empty, rc=%d\n", rc);
		return rc;
	}
config_vbat_low:
	if (chip->qg_version == QG_LITE)
		return 0;
	vbat_mv = (temp < chip->dt.cold_temp_threshold) ?
		chip->dt.vbatt_low_cold_mv :
		chip->dt.vbatt_low_mv;
	rc = qg_read(chip, QG_VBAT_LOW_THRESHOLD_REG,
			&reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to read vbat-low, rc=%d\n", rc);
		return rc;
	}
	if (vbat_mv == (reg * 50))
		return 0;
	reg = vbat_mv / 50;
	rc = qg_write(chip, QG_VBAT_LOW_THRESHOLD_REG,
			&reg, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to write vbat-low, rc=%d\n", rc);
		return rc;
	}
	dev_dbg(chip->dev, "VBAT LOW threshold updated to %dmV temp=%d\n",
			vbat_mv, temp);
	return rc;
}
static int qg_hw_init(struct qpnp_qg *chip)
{
	int rc = 0, temp = 0;
	u8 reg = 0, ctl2 = 0, ctl3 = 0, status3 = 0;
	if (!chip)
		return -EINVAL;
	rc = qg_read(chip, QG_STATUS2_REG, &reg, 1);
	if (rc < 0)
		return rc;
	rc = qg_read(chip, PERPH_SUBTYPE_REG, &chip->qg_subtype, 1);
	if (rc < 0)
		return rc;
	if (chip->qg_subtype == QG_ADC_IBAT_5A)
		chip->max_fcc_limit_ma = IBAT_5A_FCC_MA;
	else
		chip->max_fcc_limit_ma = IBAT_10A_FCC_MA;
	if (chip->qg_version == QG_LITE) {
		rc = qg_read(chip, QG_MODE_CTL2_REG, &reg, 1);
		if (rc < 0)
			return rc;
		chip->qg_mode = (reg & VI_MODE_BIT) ? QG_V_I_MODE : QG_V_MODE;
	} else {
		chip->qg_mode = QG_V_I_MODE;
	}
	if (chip->qg_mode == QG_V_MODE) {
		chip->dt.tcss_enable = false;
		chip->dt.bass_enable = false;
	}
	rc = qg_set_wa_flags(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to update PMIC type flags, rc=%d\n", rc);
		return rc;
	}
	rc = qg_master_hold(chip, true);
	if (rc < 0)
		return rc;
	rc = qg_process_rt_fifo(chip);
	if (rc < 0)
		return rc;
	if (chip->dt.s2_fifo_length > 0) {
		rc = qg_update_fifo_length(chip, chip->dt.s2_fifo_length);
		if (rc < 0)
			goto release_master;
	}
	if (chip->dt.s2_acc_length > 0) {
		reg = ilog2(chip->dt.s2_acc_length) - 1;
		rc = qg_masked_write(chip, QG_S2_NORMAL_MEAS_CTL2_REG,
				NUM_OF_ACCUM_MASK, reg);
		if (rc < 0)
			goto release_master;
	}
	if (chip->dt.s2_acc_intvl_ms > 0) {
		reg = chip->dt.s2_acc_intvl_ms / 10;
		rc = qg_write(chip, QG_S2_NORMAL_MEAS_CTL3_REG, &reg, 1);
		if (rc < 0)
			goto release_master;
	}
	chip->s2_state = S2_DEFAULT;
	chip->s2_state_mask = S2_DEFAULT;
	dev_dbg(chip->dev, "S2 FIFO initialized: length=%d acc_length=%d acc_intvl=%d ms\n",
			chip->dt.s2_fifo_length, chip->dt.s2_acc_length,
			chip->dt.s2_acc_intvl_ms);
	chip->data_ready = true;
	wake_up_interruptible(&chip->qg_wait_q);
release_master:
	rc = qg_master_hold(chip, false);
	if (rc < 0)
		return rc;
	ctl2 = 0;
	ctl3 = 0;
	status3 = 0;
	rc = qg_read(chip, QG_S2_NORMAL_MEAS_CTL2_REG, &ctl2, 1);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read CTL2 rc=%d\n", rc);
	rc = qg_read(chip, QG_S2_NORMAL_MEAS_CTL3_REG, &ctl3, 1);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read CTL3 rc=%d\n", rc);
	else
		dev_dbg(chip->dev, "S2 CTL2=0x%02x CTL3=0x%02x\n", ctl2, ctl3);
	rc = qg_read(chip, QG_STATUS3_REG, &status3, 1);
	if (rc < 0)
		dev_err(chip->dev, "Failed to read STATUS3 rc=%d\n", rc);
	chip->last_fifo_update_time = ktime_get_boottime();
	if (chip->dt.ocv_timer_expiry_min != -EINVAL &&
			chip->qg_version != QG_LITE) {
		if (chip->dt.ocv_timer_expiry_min < 2)
			chip->dt.ocv_timer_expiry_min = 2;
		else if (chip->dt.ocv_timer_expiry_min > 30)
			chip->dt.ocv_timer_expiry_min = 30;
		reg = (chip->dt.ocv_timer_expiry_min - 2) / 4;
		rc = qg_masked_write(chip,
				QG_S3_SLEEP_OCV_MEAS_CTL4_REG,
				SLEEP_IBAT_QUALIFIED_LENGTH_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to write OCV timer, rc=%d\n", rc);
			return rc;
		}
	}
	if (chip->dt.ocv_tol_threshold_uv != -EINVAL) {
		if (chip->dt.ocv_tol_threshold_uv < 0)
			chip->dt.ocv_tol_threshold_uv = 0;
		else if (chip->dt.ocv_tol_threshold_uv > 12262)
			chip->dt.ocv_tol_threshold_uv = 12262;
		reg = chip->dt.ocv_tol_threshold_uv / 195;
		rc = qg_masked_write(chip,
				QG_S3_SLEEP_OCV_TREND_CTL2_REG,
				TREND_TOL_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to write OCV tol-thresh, rc=%d\n", rc);
			return rc;
		}
	}
	if (chip->dt.s3_entry_fifo_length != -EINVAL) {
		if (chip->dt.s3_entry_fifo_length < 1) {
			chip->dt.s3_entry_fifo_length = 1;
		} else if (chip->dt.s3_entry_fifo_length >
				chip->max_fifo_length) {
			chip->dt.s3_entry_fifo_length =
				chip->max_fifo_length;
		}
		reg = chip->dt.s3_entry_fifo_length - 1;
		rc = qg_masked_write(chip,
				QG_S3_SLEEP_OCV_IBAT_CTL1_REG,
				SLEEP_IBAT_QUALIFIED_LENGTH_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to write S3-entry fifo-length, rc=%d\n",
					rc);
			return rc;
		}
	}
	if (chip->dt.s3_entry_ibat_ua != -EINVAL) {
		if (chip->dt.s3_entry_ibat_ua < 0)
			chip->dt.s3_entry_ibat_ua = 0;
		else if (chip->dt.s3_entry_ibat_ua > 155550)
			chip->dt.s3_entry_ibat_ua = 155550;
		reg = chip->dt.s3_entry_ibat_ua / 610;
		rc = qg_write(chip, QG_S3_ENTRY_IBAT_THRESHOLD_REG,
				&reg, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to write S3-entry ibat-uA, rc=%d\n", rc);
			return rc;
		}
	}
	if (chip->dt.s3_exit_ibat_ua != -EINVAL) {
		if (chip->dt.s3_exit_ibat_ua < 0)
			chip->dt.s3_exit_ibat_ua = 0;
		else if (chip->dt.s3_exit_ibat_ua > 155550)
			chip->dt.s3_exit_ibat_ua = 155550;
		rc = qg_read(chip,
				QG_S3_ENTRY_IBAT_THRESHOLD_REG,
				&reg, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to read S3-exit ibat-uA, rc=%d\n", rc);
			return rc;
		}
		temp = reg * 610;
		if (chip->dt.s3_exit_ibat_ua < temp)
			chip->dt.s3_exit_ibat_ua = temp;
		else
			chip->dt.s3_exit_ibat_ua -= temp;
		reg = chip->dt.s3_exit_ibat_ua / 610;
		rc = qg_write(chip,
				QG_S3_EXIT_IBAT_THRESHOLD_REG,
				&reg, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Failed to write S3-exit ibat-uA, rc=%d\n", rc);
			return rc;
		}
	}
	if (chip->dt.vbatt_low_mv < 0)
		chip->dt.vbatt_low_mv = 0;
	else if (chip->dt.vbatt_low_mv > 12750)
		chip->dt.vbatt_low_mv = 12750;
	if (chip->dt.vbatt_empty_mv < 0)
		chip->dt.vbatt_empty_mv = 0;
	else if (chip->dt.vbatt_empty_mv > 12750)
		chip->dt.vbatt_empty_mv = 12750;
	if (chip->dt.vbatt_empty_cold_mv < 0)
		chip->dt.vbatt_empty_cold_mv = 0;
	else if (chip->dt.vbatt_empty_cold_mv > 12750)
		chip->dt.vbatt_empty_cold_mv = 12750;
	rc = qg_vbat_thresholds_config(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to configure VBAT empty/low rc=%d\n", rc);
		return rc;
	}
	if (chip->qg_version != QG_LITE) {
		rc = qg_masked_write(chip, QG_S5_OCV_VALIDATE_MEAS_CTL1_REG,
				ALLOW_S5_BIT, 0);
		if (rc < 0)
			return rc;
	}
	rc = qg_masked_write(chip, QG_S7_PON_OCV_MEAS_CTL1_REG,
			ADC_CONV_DLY_MASK, ADC_CONV_DLY_512MS);
	if (rc < 0)
		return rc;
	return 0;
}
static ssize_t qg_device_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc = 0;
	struct qpnp_qg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->kdata);
	if (count < data_size)
		return -EINVAL;
	if (count != sizeof(chip->kdata))
		dev_dbg(chip->dev, "read count %zu != kdata size %zu\n",
				count, sizeof(chip->kdata));
	if (!chip->data_ready && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;
	if (!(file->f_flags & O_NONBLOCK)) {
		rc = wait_event_interruptible(chip->qg_wait_q, chip->data_ready);
		if (rc < 0)
			return rc;
	}
	mutex_lock(&chip->data_lock);
	if (!chip->data_ready) {
		rc = -EFAULT;
		goto fail;
	}
	if (copy_to_user(buf, &chip->kdata, data_size)) {
		rc = -EFAULT;
		goto fail;
	}
	chip->data_ready = false;
	qg_ws_good_ocv(chip, false);
	qg_ws_fifo_done(chip, false);
	qg_ws_fifo_rt_done(chip, false);
	qg_ws_suspend_data(chip, false);
	qg_ws_sleep_exit_data(chip, false);
	memset(&chip->kdata, 0, sizeof(chip->kdata));
	mutex_unlock(&chip->data_lock);
	return data_size;
fail:
	mutex_unlock(&chip->data_lock);
	return rc;
}
static ssize_t qg_device_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc = -EINVAL;
	struct qpnp_qg *chip = file->private_data;
	unsigned long data_size = sizeof(chip->udata);
	mutex_lock(&chip->data_lock);
	if (!count || count < data_size) {
		rc = -EINVAL;
		goto fail;
	}
	if (count != sizeof(chip->udata))
		dev_dbg(chip->dev, "write count %zu != udata size %zu\n",
				count, sizeof(chip->udata));
	if (copy_from_user(&chip->udata, buf, data_size)) {
		rc = -EFAULT;
		goto fail;
	}
	rc = data_size;
	qg_ws_udata_ready(chip, true);
	schedule_work(&chip->udata_work);
fail:
	mutex_unlock(&chip->data_lock);
	return rc;
}
static int qg_device_open(struct inode *inode, struct file *file)
{
	struct qpnp_qg *chip = container_of(inode->i_cdev,
			struct qpnp_qg, qg_cdev);
	file->private_data = chip;
	return 0;
}
static unsigned int qg_device_poll(struct file *file, poll_table *wait)
{
	struct qpnp_qg *chip = file->private_data;
	unsigned int mask = 0;
	poll_wait(file, &chip->qg_wait_q, wait);
	if (chip->data_ready)
		mask = POLLIN | POLLRDNORM;
	return mask;
}
static int qg_device_release(struct inode *inode, struct file *file)
{
	struct qpnp_qg *chip = container_of(inode->i_cdev,
			struct qpnp_qg, qg_cdev);
	file->private_data = chip;
	chip->qg_device_open = false;
	dev_dbg(chip->dev, "QG device closed!\n");
	return 0;
}
static const struct file_operations qg_fops = {
	.owner   = THIS_MODULE,
	.open    = qg_device_open,
	.read    = qg_device_read,
	.write   = qg_device_write,
	.poll	 = qg_device_poll,
	.release = qg_device_release,
};
static int qg_register_device(struct qpnp_qg *chip)
{
	int rc = 0;
	rc = alloc_chrdev_region(&chip->dev_no, 0, 1, "qg");
	if (rc < 0) {
		dev_err(chip->dev, "Failed to allocate chardev rc=%d\n", rc);
		return rc;
	}
	cdev_init(&chip->qg_cdev, &qg_fops);
	rc = cdev_add(&chip->qg_cdev, chip->dev_no, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to cdev_add rc=%d\n", rc);
		goto unregister_chrdev;
	}
	chip->qg_class = class_create("qg");
	if (IS_ERR_OR_NULL(chip->qg_class)) {
		dev_err(chip->dev, "Failed to create qg class\n");
		rc = -EINVAL;
		goto delete_cdev;
	}
	chip->qg_device = device_create(chip->qg_class, NULL,
			chip->dev_no, NULL, "qg");
	if (IS_ERR(chip->qg_device)) {
		dev_err(chip->dev, "Failed to create qg_device\n");
		rc = -EINVAL;
		goto destroy_class;
	}
	return 0;
destroy_class:
	class_destroy(chip->qg_class);
delete_cdev:
	cdev_del(&chip->qg_cdev);
unregister_chrdev:
	unregister_chrdev_region(chip->dev_no, 1);
	return rc;
}
static int qg_init_psy(struct qpnp_qg *chip)
{
	struct power_supply_config psy_cfg = {};
	int rc = 0;
	psy_cfg.drv_data = chip;
	psy_cfg.fwnode = dev_fwnode(chip->dev);
	chip->qg_psy = devm_power_supply_register(chip->dev, &qg_psy_desc, &psy_cfg);
	if (IS_ERR(chip->qg_psy)) {
		rc = PTR_ERR(chip->qg_psy);
		if (rc < 0)
			return -ENODEV;
	}
	chip->nb.notifier_call = qg_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0)
		return -ENODEV;
	return rc;
}
static void qg_unregister_device(struct qpnp_qg *chip)
{
	device_destroy(chip->qg_class, chip->dev_no);
	class_destroy(chip->qg_class);
	cdev_del(&chip->qg_cdev);
	unregister_chrdev_region(chip->dev_no, 1);
}
static int qg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val1, int *val2, long mask)
{
	struct qpnp_qg *chip = iio_priv(indio_dev);
	int64_t temp = 0;
	int rc = 0;
	*val1 = 0;
	switch (chan->channel) {
	case PSY_IIO_CAPACITY:
		mutex_lock(&chip->soc_lock);
		*val1 = chip->msoc;
		mutex_unlock(&chip->soc_lock);
		break;
	case PSY_IIO_CAPACITY_RAW:
		*val1 = chip->sys_soc;
		break;
	case PSY_IIO_REAL_CAPACITY:
		mutex_lock(&chip->soc_lock);
		*val1 = chip->msoc;
		mutex_unlock(&chip->soc_lock);
		break;
	case PSY_IIO_VOLTAGE_NOW:
		rc = qg_get_battery_voltage(chip, val1);
		break;
	case PSY_IIO_VOLTAGE_OCV:
		rc = qg_sdam_read(chip, SDAM_OCV_UV, val1);
		break;
	case PSY_IIO_CURRENT_NOW:
		rc = qg_get_battery_current(chip, val1);
		break;
	case PSY_IIO_TEMP:
		rc = qg_get_battery_temp(chip, val1);
		break;
	case PSY_IIO_RESISTANCE_ID:
		*val1 = chip->batt_id_ohm;
		break;
	case PSY_IIO_DEBUG_BATTERY:
		*val1 = is_debug_batt_id(chip);
		break;
	case PSY_IIO_RESISTANCE:
		rc = qg_sdam_read(chip, SDAM_RBAT_MOHM, val1);
		if (!rc)
			*val1 *= 1000;
		break;
	case PSY_IIO_SOC_REPORTING_READY:
		*val1 = chip->soc_reporting_ready;
		break;
	case PSY_IIO_RESISTANCE_CAPACITIVE:
		*val1 = 0;
		break;
	case PSY_IIO_VOLTAGE_MIN:
		*val1 = chip->dt.vbatt_cutoff_mv * 1000;
		break;
	case PSY_IIO_VOLTAGE_MAX:
		*val1 = chip->bp.float_volt_uv;
		break;
	case PSY_IIO_BATT_FULL_CURRENT:
		*val1 = chip->chg_iterm_ma * 1000;
		break;
	case PSY_IIO_BATT_PROFILE_VERSION:
		*val1 = chip->bp.qg_profile_version;
		break;
	case PSY_IIO_CHARGE_FULL:
		rc = qg_get_nominal_capacity(chip, (u32 *)&temp, 250, true);
		if (!rc)
			*val1 = (int)temp;
		break;
	case PSY_IIO_CHARGE_FULL_DESIGN:
		rc = qg_get_nominal_capacity(chip, (u32 *)&temp, 250, true);
		if (!rc)
			*val1 = (int)temp;
		break;
	case PSY_IIO_CC_SOC:
		if (chip->battery_missing || is_debug_batt_id(chip))
			*val1 = -EINVAL;
		else if (chip->cc_soc == INT_MIN)
			*val1 = -EINVAL;
		else
			*val1 = chip->cc_soc;
		break;
	case PSY_IIO_FG_RESET:
		*val1 = 0;
		break;
	case PSY_IIO_VOLTAGE_AVG:
		rc = qg_get_vbat_avg(chip, val1);
		break;
	case PSY_IIO_CURRENT_AVG:
		rc = qg_get_ibat_avg(chip, val1);
		break;
	case PSY_IIO_POWER_NOW:
	case PSY_IIO_POWER_AVG:
		*val1 = -EINVAL;
		break;
	case PSY_IIO_SCALE_MODE_EN:
		*val1 = 0;
		break;
	case PSY_IIO_BATT_AGE_LEVEL:
		*val1 = chip->batt_age_level;
		break;
	case PSY_IIO_FG_TYPE:
		*val1 = chip->qg_mode;
		break;
	default:
		dev_dbg(chip->dev,
				"Unsupported QG IIO read chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		dev_err_ratelimited(chip->dev,
				"Couldn't read IIO channel %d rc=%d\n",
				chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}
static int qg_iio_fwnode_xlate(struct iio_dev *indio_dev,
		const struct fwnode_reference_args *iiospec)
{
	struct qpnp_qg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i = 0;
	if (!iiospec || iiospec->nargs < 1)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(qg_iio_psy_channels); i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;
	return -EINVAL;
}
static const struct iio_info qg_iio_info = {
	.read_raw       = qg_iio_read_raw,
	.fwnode_xlate   = qg_iio_fwnode_xlate,
};
static int qg_init_iio_psy(struct qpnp_qg *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(qg_iio_psy_channels);
	int rc = 0, i = 0;
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
			sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;
	chip->int_iio_chans = devm_kcalloc(chip->dev, num_iio_channels,
			sizeof(*chip->int_iio_chans), GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;
	chip->ext_iio_chans = devm_kcalloc(chip->dev,
			ARRAY_SIZE(qg_ext_iio_chan_name),
			sizeof(*chip->ext_iio_chans), GFP_KERNEL);
	if (!chip->ext_iio_chans)
		return -ENOMEM;
	indio_dev->info         = &qg_iio_info;
	indio_dev->dev.parent   = chip->dev;
	indio_dev->dev.of_node  = chip->dev->of_node;
	indio_dev->name         = "qpnp,qg";
	indio_dev->modes        = INDIO_DIRECT_MODE;
	indio_dev->channels     = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i] = devm_kzalloc(chip->dev,
				sizeof(*chip->int_iio_chans[i]), GFP_KERNEL);
		if (!chip->int_iio_chans[i])
			return -ENOMEM;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i]->indio_dev = indio_dev;
		chip->int_iio_chans[i]->channel   = chan;
		chan->address            = i;
		chan->channel            = qg_iio_psy_channels[i].channel_num;
		chan->type               = qg_iio_psy_channels[i].type;
		chan->datasheet_name     = qg_iio_psy_channels[i].datasheet_name;
		chan->extend_name        = qg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate = qg_iio_psy_channels[i].info_mask;
	}
	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		dev_err(chip->dev, "Failed to register QG IIO device rc=%d\n", rc);
	return rc;
}
static void scale_soc_work(struct work_struct *work)
{
	struct qpnp_qg *chip = container_of(work,
			struct qpnp_qg, scale_soc_work);
	mutex_lock(&chip->soc_lock);
	if (!is_scaling_required(chip)) {
		scale_soc_stop(chip);
		goto done;
	}
	update_msoc(chip);
	if (is_scaling_required(chip)) {
		alarm_start_relative(&chip->alarm_timer,
				ms_to_ktime(chip->next_wakeup_ms));
	} else {
		scale_soc_stop(chip);
		goto done_psy;
	}
done_psy:
	power_supply_changed(chip->qg_psy);
done:
	pm_relax(chip->dev);
	mutex_unlock(&chip->soc_lock);
}
static void qpnp_msoc_timer(struct alarm *alarm, ktime_t now)
{
	struct qpnp_qg *chip = container_of(alarm,
			struct qpnp_qg, alarm_timer);
	pm_stay_awake(chip->dev);
	schedule_work(&chip->scale_soc_work);
}
static int qg_soc_init(struct qpnp_qg *chip)
{
	if (!alarmtimer_get_rtcdev())
		return -EINVAL;
	alarm_init(&chip->alarm_timer, ALARM_BOOTTIME, qpnp_msoc_timer);
	INIT_WORK(&chip->scale_soc_work, scale_soc_work);
	return 0;
}
static int qg_probe(struct platform_device *pdev)
{
	struct qpnp_qg *chip;
	const struct qg_config *config;
	struct iio_dev *indio_dev;
	int rc = 0;
	bool device_registered = false;
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;
	chip = iio_priv(indio_dev);
	chip->indio_dev = indio_dev;
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);
	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap)
		return dev_err_probe(&pdev->dev, -ENODEV, "Failed to get regmap\n");
	chip->batt_id_chan = devm_iio_channel_get(&pdev->dev, "batt-id");
	if (IS_ERR(chip->batt_id_chan)) {
		rc = PTR_ERR(chip->batt_id_chan);
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "batt-id channel unavailable, rc=%d\n", rc);
		chip->batt_id_chan = NULL;
		return rc;
	}
	chip->batt_therm_chan = devm_iio_channel_get(&pdev->dev, "batt-therm");
	if (IS_ERR(chip->batt_therm_chan)) {
		rc = PTR_ERR(chip->batt_therm_chan);
		if (rc != -EPROBE_DEFER)
			dev_err(chip->dev, "batt-therm channel unavailable, rc=%d\n", rc);
		chip->batt_therm_chan = NULL;
		return rc;
	}
	mutex_init(&chip->data_lock);
	mutex_init(&chip->soc_lock);
	mutex_init(&chip->bus_lock);
	init_waitqueue_head(&chip->qg_wait_q);
	INIT_WORK(&chip->udata_work, process_udata_work);
	INIT_WORK(&chip->qg_status_change_work, qg_status_change_work);
	INIT_DELAYED_WORK(&chip->qg_sleep_exit_work, qg_sleep_exit_work);
	chip->maint_soc    = -EINVAL;
	chip->batt_soc     = INT_MIN;
	chip->cc_soc       = INT_MIN;
	chip->sys_soc      = INT_MIN;
	chip->full_soc     = QG_SOC_FULL;
	chip->chg_iterm_ma = INT_MIN;
	config = of_device_get_match_data(&pdev->dev);
	if (!config) {
		rc = -EINVAL;
		goto cleanup;
	}
	chip->qg_version = config->qg_version;
	chip->pmic_version = config->pmic_version;
	chip->max_fifo_length = (chip->qg_version == QG_LITE) ? 5 : 8;
	rc = qg_parse_dt(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_hw_init(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_sdam_init(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_setup_battery(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_register_device(chip);
	if (rc < 0)
		goto cleanup;
	device_registered = true;
	rc = qg_sanitize_sdam(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_soc_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to initialize SOC scaling init rc=%d\n", rc);
		goto cleanup;
	}
	chip->qg_ws = wakeup_source_register(chip->dev, "qg_ws");
	if (!chip->qg_ws) {
		rc = -ENOMEM;
		goto cleanup;
	}
	rc = qg_determine_pon_soc(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to determine PON SOC rc=%d\n", rc);
		goto cleanup;
	}
	atomic_set(&chip->wake_voter_udata_ready,    0);
	atomic_set(&chip->wake_voter_fifo_done,      0);
	atomic_set(&chip->wake_voter_fifo_rt_done,   0);
	atomic_set(&chip->wake_voter_suspend_data,   0);
	atomic_set(&chip->wake_voter_good_ocv,       0);
	atomic_set(&chip->wake_voter_sleep_exit_data, 0);
	atomic_set(&chip->wake_voter_sleep_exit,     0);
	atomic_set(&chip->good_ocv_irq_disable_cnt,  0);
	rc = qg_init_iio_psy(chip);
	if (rc < 0)
		goto cleanup;
	rc = qg_init_psy(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Failed to initialize QG PSY, rc=%d\n", rc);
		goto cleanup;
	}
	rc = qg_request_irqs(chip);
	if (rc < 0)
		goto cleanup;
	dev_info(chip->dev, "QG driver probed successfully\n");
	return 0;
cleanup:
	if (chip->qg_ws)
		wakeup_source_unregister(chip->qg_ws);
	if (device_registered) {
		if (chip->qg_class) {
			device_destroy(chip->qg_class, chip->dev_no);
			class_destroy(chip->qg_class);
		}
		cdev_del(&chip->qg_cdev);
		unregister_chrdev_region(chip->dev_no, 1);
	}
	mutex_destroy(&chip->soc_lock);
	mutex_destroy(&chip->data_lock);
	mutex_destroy(&chip->bus_lock);
	return rc;
}
static void qg_remove(struct platform_device *pdev)
{
	struct qpnp_qg *chip = platform_get_drvdata(pdev);
	qg_unregister_device(chip);
	qg_batterydata_exit(chip);
	power_supply_unreg_notifier(&chip->nb);
	cancel_delayed_work_sync(&chip->qg_sleep_exit_work);
	cancel_work_sync(&chip->qg_status_change_work);
	cancel_work_sync(&chip->udata_work);
	mutex_destroy(&chip->soc_lock);
	mutex_destroy(&chip->data_lock);
	mutex_destroy(&chip->bus_lock);
	wakeup_source_unregister(chip->qg_ws);
}
/*
 * Device tree match table
 */
static const struct of_device_id qg_match_table[] = {
	{
		.compatible = "qcom,pm4125-qg",
		.data = &config[PM4125],
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, qg_match_table);
static struct platform_driver qg_driver = {
	.driver = {
		.name = "qcom-qg",
		.of_match_table = qg_match_table,
		.pm             = &qg_pm_ops,
	},
	.probe = qg_probe,
	.remove = qg_remove,
};
module_platform_driver(qg_driver);
MODULE_DESCRIPTION("Qualcomm PMIC QGauge Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_IMPORT_NS("IIO_CONSUMER");
