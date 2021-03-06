/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../../ssp.h"
#include "../ssp_factory.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/


#define GM_AKM_DATA_SPEC_MIN	-6500
#define GM_AKM_DATA_SPEC_MAX	6500


#define GM_SELFTEST_X_SPEC_MIN	-200
#define GM_SELFTEST_X_SPEC_MAX	200
#define GM_SELFTEST_Y_SPEC_MIN	-200
#define GM_SELFTEST_Y_SPEC_MAX	200
#define GM_SELFTEST_Z_SPEC_MIN	-1000
#define GM_SELFTEST_Z_SPEC_MAX	-200



int check_adc_data_spec(struct ssp_data *data, int sensortype)
{
	int data_spec_max = 0;
	int data_spec_min = 0;

    data_spec_max = GM_AKM_DATA_SPEC_MAX;
    data_spec_min = GM_AKM_DATA_SPEC_MIN;

	if ((data->buf[sensortype].x == 0) &&
		(data->buf[sensortype].y == 0) &&
		(data->buf[sensortype].z == 0))
		return FAIL;
	else if ((data->buf[sensortype].x > data_spec_max)
		|| (data->buf[sensortype].x < data_spec_min)
		|| (data->buf[sensortype].y > data_spec_max)
		|| (data->buf[sensortype].y < data_spec_min)
		|| (data->buf[sensortype].z > data_spec_max)
		|| (data->buf[sensortype].z < data_spec_min))
		return FAIL;
	else
		return SUCCESS;
}


int set_pdc_matrix(struct ssp_data *data)
{
	int ret = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & 0x04)) {
		pr_info("[SSP] %s - Skip this function!!!"\
			", magnetic sensor is not connected(0x%llx)\n",
			__func__, data->uSensorState);
		return ret;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX;
	msg->length = sizeof(data->pdc_matrix);
	msg->options = AP2HUB_WRITE;
	msg->buffer = data->pdc_matrix;

	msg->free_buffer = 0;

	ret = ssp_spi_async(data, msg);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - i2c fail %d\n", __func__, ret);
		ret = ERROR;
	}

	pr_info("[SSP] %s: finished\n", __func__);

	return ret;
}

ssize_t get_magnetic_ak09916c_name(char *buf)
{
	return sprintf(buf, "%s\n", "AK09916C");
}

ssize_t get_magnetic_ak09916c_vendor(char *buf)
{
	return sprintf(buf, "%s\n", "AKM");
}

ssize_t get_magnetic_ak09916c_adc(struct ssp_data *data, char *buf)
{
	bool bSuccess = false;
	u8 chTempbuf[4] = {0, };
	s16 sensor_buf[3] = {0, };
	int retries = 10;
	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].x = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].y = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].z = 0;


	if (!(atomic64_read(&data->aSensorEnable) & (1 << SENSOR_TYPE_GEOMAGNETIC_FIELD)))
		send_instruction(data, ADD_SENSOR, SENSOR_TYPE_GEOMAGNETIC_FIELD,
			chTempbuf, 4);

	do {
		msleep(60);
		if (check_adc_data_spec(data, SENSOR_TYPE_GEOMAGNETIC_FIELD) == SUCCESS)
			break;
	} while (--retries);

	if (retries > 0)
		bSuccess = true;

	sensor_buf[0] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].x;
	sensor_buf[1] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].y;
	sensor_buf[2] = data->buf[SENSOR_TYPE_GEOMAGNETIC_FIELD].z;


	if (!(atomic64_read(&data->aSensorEnable) & (1 << SENSOR_TYPE_GEOMAGNETIC_FIELD)))
		send_instruction(data, REMOVE_SENSOR, SENSOR_TYPE_GEOMAGNETIC_FIELD,
			chTempbuf, 4);

	pr_info("[SSP] %s - x = %d, y = %d, z = %d\n", __func__,
		sensor_buf[0], sensor_buf[1], sensor_buf[2]);

	return sprintf(buf, "%s,%d,%d,%d\n", (bSuccess ? "OK" : "NG"),
		sensor_buf[0], sensor_buf[1], sensor_buf[2]);
}

ssize_t get_magnetic_ak09916c_dac(struct ssp_data *data, char *buf)
{
	bool bSuccess = false;
	char chTempBuf[22] = { 0,  };

	if (!data->uMagCntlRegData) {
		bSuccess = true;
	} else {
		int ret;
		struct ssp_msg *msg;
		
		pr_info("[SSP] %s - check cntl register before selftest",
			__func__);
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = GEOMAGNETIC_FACTORY;
		msg->length = 22;
		msg->options = AP2HUB_READ;
		msg->buffer = chTempBuf;
		msg->free_buffer = 0;

		ret = ssp_spi_sync(data, msg, 1000);

		if (ret != SUCCESS) {
			pr_err("[SSP] %s - spi sync failed due to Timeout!! %d\n",
					__func__, ret);
		}


		data->uMagCntlRegData = chTempBuf[21];
		bSuccess = !data->uMagCntlRegData;
	}

	pr_info("[SSP] %s - CTRL : 0x%x\n", __func__,
				data->uMagCntlRegData);

	data->uMagCntlRegData = 1;	/* reset the value */

	return sprintf(buf, "%s,%d,%d,%d\n",
		(bSuccess ? "OK" : "NG"), (bSuccess ? 1 : 0), 0, 0);
}

ssize_t get_magnetic_ak09916c_raw_data(struct ssp_data *data, char *buf)
{

	pr_info("[SSP] %s - %d,%d,%d\n", __func__,
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x,
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y,
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z);

	if (data->bGeomagneticRawEnabled == false) {
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x = -1;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y = -1;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z = -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x,
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y,
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z);
}

ssize_t set_magnetic_ak09916c_raw_data(struct ssp_data *data, const char *buf)
{
	char chTempbuf[4] = { 0 };
	int ret;
	int64_t dEnable;
	s32 dMsDelay = 20;
	memcpy(&chTempbuf[0], &dMsDelay, 4);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0)
		return ret;


	if (dEnable) {
		int retries = 50;
		
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x = 0;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y = 0;
		data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z = 0;

		send_instruction(data, ADD_SENSOR, SENSOR_TYPE_GEOMAGNETIC_POWER,
			chTempbuf, 4);

		do {
			msleep(20);
			if (check_adc_data_spec(data, SENSOR_TYPE_GEOMAGNETIC_POWER) == SUCCESS)
				break;
		} while (--retries);

		if (retries > 0)
		{
			pr_info("[SSP] %s - success, %d\n", __func__, retries);
            data->bGeomagneticRawEnabled = true;
		}
		else
		{
			pr_err("[SSP] %s - wait timeout, %d\n", __func__,
				retries);
            data->bGeomagneticRawEnabled = false;
		}


	} else {
		send_instruction(data, REMOVE_SENSOR, SENSOR_TYPE_GEOMAGNETIC_POWER,
			chTempbuf, 4);
		data->bGeomagneticRawEnabled = false;
	}


	return ret;
}

ssize_t get_magnetic_ak09916c_asa(struct ssp_data *data, char *buf)
{
	return sprintf(buf, "%d,%d,%d\n", (s16)data->uFuseRomData[0],
		(s16)data->uFuseRomData[1], (s16)data->uFuseRomData[2]);
}

ssize_t get_magnetic_ak09916c_status(struct ssp_data *data, char *buf)
{
	bool bSuccess;

	if ((data->uFuseRomData[0] == 0) ||
		(data->uFuseRomData[0] == 0xff) ||
		(data->uFuseRomData[1] == 0) ||
		(data->uFuseRomData[1] == 0xff) ||
		(data->uFuseRomData[2] == 0) ||
		(data->uFuseRomData[2] == 0xff))
		bSuccess = false;
	else
		bSuccess = true;

	return sprintf(buf, "%s,%u\n", (bSuccess ? "OK" : "NG"), bSuccess);
}


ssize_t get_magnetic_ak09916c_logging_data(struct ssp_data *data, char *buf)
{
	char buffer[21] = {0, };
	int ret = 0;
	int logging_data[8] = {0, };


	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	msg->cmd = MSG2SSP_AP_GEOMAG_LOGGING;
	msg->length = 21;
	msg->options = AP2HUB_READ;
	msg->buffer = buffer;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 1000);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - Magnetic logging data Timeout!! %d\n",
			__func__, ret);
		goto exit;
	}


	logging_data[0] = buffer[0];	/* ST1 Reg */
	logging_data[1] = (short)((buffer[3] << 8) + buffer[2]);
	logging_data[2] = (short)((buffer[5] << 8) + buffer[4]);
	logging_data[3] = (short)((buffer[7] << 8) + buffer[6]);
	logging_data[4] = buffer[1];	/* ST2 Reg */
	logging_data[5] = (short)((buffer[9] << 8) + buffer[8]);
	logging_data[6] = (short)((buffer[11] << 8) + buffer[10]);
	logging_data[7] = (short)((buffer[13] << 8) + buffer[12]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			logging_data[0], logging_data[1],
			logging_data[2], logging_data[3],
			logging_data[4], logging_data[5],
			logging_data[6], logging_data[7],
			data->uFuseRomData[0], data->uFuseRomData[1],
			data->uFuseRomData[2]);
exit:
	return snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
}

ssize_t get_magnetic_ak09916c_matrix(struct ssp_data *data, char *buf)
{

	return sprintf(buf,
		"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2], data->pdc_matrix[3], data->pdc_matrix[4], 
		data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7], data->pdc_matrix[8], data->pdc_matrix[9], 
		data->pdc_matrix[10], data->pdc_matrix[11], data->pdc_matrix[12], data->pdc_matrix[13], data->pdc_matrix[14], 
		data->pdc_matrix[15], data->pdc_matrix[16], data->pdc_matrix[17], data->pdc_matrix[18], data->pdc_matrix[19], 
		data->pdc_matrix[20], data->pdc_matrix[21], data->pdc_matrix[22], data->pdc_matrix[23], data->pdc_matrix[24], 
		data->pdc_matrix[25], data->pdc_matrix[26]);
}

ssize_t set_magnetic_ak09916c_matrix(struct ssp_data *data, const char *buf)
{
	u8 val[PDC_SIZE] = {0, };
 	int ret;
 	int i;
	char* token;
	char* str;
	str = (char *)buf;
	
	for(i = 0; i < PDC_SIZE; i++)
 	{
		token = strsep(&str, " \n");
		if(token == NULL)
		{
			pr_err("[SSP] %s : too few arguments (%d needed)",__func__,PDC_SIZE);
 			return -EINVAL;
		}

		ret = kstrtou8(token, 10, &val[i]);
		if (ret<0) {
 			pr_err("[SSP] %s : kstros16 error %d",__func__,ret);
 			return ret;
 		}
 	}		
	
	for(i = 0; i < PDC_SIZE; i++)
		data->pdc_matrix[i] = val[i];

	pr_info("[SSP] %s : %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n", 
		__func__, data->pdc_matrix[0], data->pdc_matrix[1], data->pdc_matrix[2], data->pdc_matrix[3], data->pdc_matrix[4], 
		data->pdc_matrix[5], data->pdc_matrix[6], data->pdc_matrix[7], data->pdc_matrix[8], data->pdc_matrix[9], 
		data->pdc_matrix[10], data->pdc_matrix[11], data->pdc_matrix[12], data->pdc_matrix[13], data->pdc_matrix[14], 
		data->pdc_matrix[15], data->pdc_matrix[16], data->pdc_matrix[17], data->pdc_matrix[18], data->pdc_matrix[19], 
		data->pdc_matrix[20], data->pdc_matrix[21], data->pdc_matrix[22], data->pdc_matrix[23], data->pdc_matrix[24], 
		data->pdc_matrix[25], data->pdc_matrix[26]);
		set_pdc_matrix(data);
 	
	return ret;
}

ssize_t get_magnetic_ak09916c_selftest(struct ssp_data *data, char *buf)
{
	s8 result[4] = {-1, -1, -1, -1};
	char bufSelftset[22] = {0, };
	char bufAdc[4] = {0, };
	s16 iSF_X = 0, iSF_Y = 0, iSF_Z = 0;
	s16 iADC_X = 0, iADC_Y = 0, iADC_Z = 0;
	s32 dMsDelay = 20;
	int ret = 0;
	int spec_out_retries = 0;
	struct ssp_msg *msg;

	pr_info("[SSP] %s in\n", __func__);

	/* STATUS AK09916C doesn't need FuseRomdata more*/
	result[0] = 0;

Retry_selftest:
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = GEOMAGNETIC_FACTORY;
	msg->length = 22;
	msg->options = AP2HUB_READ;
	msg->buffer = bufSelftset;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 1000);
	if (ret != SUCCESS) {
		pr_err("[SSP] %s - Magnetic Selftest Timeout!! %d\n",
			__func__, ret);
		goto exit;
	}


	/* read 6bytes data registers */
	iSF_X = (s16)((bufSelftset[13] << 8) + bufSelftset[14]);
	iSF_Y = (s16)((bufSelftset[15] << 8) + bufSelftset[16]);
	iSF_Z = (s16)((bufSelftset[17] << 8) + bufSelftset[18]);

	/* DAC (store Cntl Register value to check power down) */
	result[2] = bufSelftset[21];

	iSF_X = (s16)(((iSF_X * data->uFuseRomData[0]) >> 7) + iSF_X);
	iSF_Y = (s16)(((iSF_Y * data->uFuseRomData[1]) >> 7) + iSF_Y);
	iSF_Z = (s16)(((iSF_Z * data->uFuseRomData[2]) >> 7) + iSF_Z);

	pr_info("[SSP] %s: self test x = %d, y = %d, z = %d\n",
		__func__, iSF_X, iSF_Y, iSF_Z);

	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN)
		&& (iSF_X <= GM_SELFTEST_X_SPEC_MAX))
		pr_info("[SSP] x passed self test, expect -200<=x<=200\n");
	else
		pr_info("[SSP] x failed self test, expect -200<=x<=200\n");
	if ((iSF_Y >= GM_SELFTEST_Y_SPEC_MIN)
		&& (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX))
		pr_info("[SSP] y passed self test, expect -200<=y<=200\n");
	else
		pr_info("[SSP] y failed self test, expect -200<=y<=200\n");
	if ((iSF_Z >= GM_SELFTEST_Z_SPEC_MIN)
		&& (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX))
		pr_info("[SSP] z passed self test, expect -1000<=z<=-200\n");
	else
		pr_info("[SSP] z failed self test, expect -1000<=z<=-200\n");

	/* SELFTEST */
	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN)
		&& (iSF_X <= GM_SELFTEST_X_SPEC_MAX)
		&& (iSF_Y >= GM_SELFTEST_Y_SPEC_MIN)
		&& (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX)
		&& (iSF_Z >= GM_SELFTEST_Z_SPEC_MIN)
		&& (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX))
		result[1] = 0;

	if ((result[1] == -1) && (spec_out_retries++ < 5)) {
		pr_err("[SSP] %s, selftest spec out. Retry = %d", __func__,
			spec_out_retries);
		goto Retry_selftest;
	}

	spec_out_retries = 10;

	/* ADC */
	memcpy(&bufAdc[0], &dMsDelay, 4);

	data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y = 0;
	data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z = 0;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << SENSOR_TYPE_GEOMAGNETIC_POWER)))
		send_instruction(data, ADD_SENSOR, SENSOR_TYPE_GEOMAGNETIC_POWER,
			bufAdc, 4);

	do {
		msleep(60);
		if (check_adc_data_spec(data, SENSOR_TYPE_GEOMAGNETIC_POWER) == SUCCESS)
			break;
	} while (--spec_out_retries);

	if (spec_out_retries > 0)
		result[3] = 0;

	iADC_X = data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].x;
	iADC_Y = data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].y;
	iADC_Z = data->buf[SENSOR_TYPE_GEOMAGNETIC_POWER].z;

	if (!(atomic64_read(&data->aSensorEnable) & (1 << SENSOR_TYPE_GEOMAGNETIC_POWER)))
		send_instruction(data, REMOVE_SENSOR, SENSOR_TYPE_GEOMAGNETIC_POWER,
			bufAdc, 4);

	pr_info("[SSP] %s -adc, x = %d, y = %d, z = %d, retry = %d\n",
		__func__, iADC_X, iADC_Y, iADC_Z, spec_out_retries);

exit:
	pr_info("[SSP] %s out. Result = %d %d %d %d\n",
		__func__, result[0], result[1], result[2], result[3]);

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		result[0], result[1], iSF_X, iSF_Y, iSF_Z,
		result[2], result[3], iADC_X, iADC_Y, iADC_Z);

}


struct magnetic_sensor_operations magnetic_ak09916c_ops = {
	.get_magnetic_name = get_magnetic_ak09916c_name,
	.get_magnetic_vendor = get_magnetic_ak09916c_vendor,
	.get_magnetic_adc = get_magnetic_ak09916c_adc,
	.get_magnetic_dac = get_magnetic_ak09916c_dac,
	.get_magnetic_raw_data = get_magnetic_ak09916c_raw_data,
	.set_magnetic_raw_data = set_magnetic_ak09916c_raw_data,
	.get_magnetic_asa = get_magnetic_ak09916c_asa,
	.get_magnetic_status = get_magnetic_ak09916c_status,
	.get_magnetic_logging_data = get_magnetic_ak09916c_logging_data,
	.get_magnetic_matrix = get_magnetic_ak09916c_matrix, 
	.set_magnetic_matrix = set_magnetic_ak09916c_matrix,
	.get_magnetic_selftest = get_magnetic_ak09916c_selftest,
};

int initialize_magnetic_sensor(struct ssp_data *data)
{
	int ret = 0;
	
	/* STATUS AK09916C doesn't need FuseRomdata more*/
	data->uFuseRomData[0] = 0;
	data->uFuseRomData[1] = 0;
	data->uFuseRomData[2] = 0;
	
	ret = set_pdc_matrix(data);
	if (ret < 0)
		pr_err("[SSP] %s - set_magnetic_pdc_matrix failed %d\n",
			__func__, ret);

	
	return ret < 0 ? ret : SUCCESS;
}

void magnetic_ak09916c_function_pointer_initialize(struct ssp_data *data)
{
	data->magnetic_ops = &magnetic_ak09916c_ops;
}

