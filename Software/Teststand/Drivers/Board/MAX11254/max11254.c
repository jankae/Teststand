#include "max11254.h"

#include "util.h"
#include "log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern SemaphoreHandle_t xMutexSPI1;

static max11254_result_t SPIWrite(max11254_t *m, uint8_t *data, uint8_t length) {
	xSemaphoreTake(xMutexSPI1, portMAX_DELAY);
	m->CSgpio->BSRR = m->CSpin << 16;
	// Start the transmission
#if MAX11254_MODE == MAX11254_MODE_POLLING
	HAL_SPI_Transmit(m->spi, data, length, 1000);
#elif MAX11254_MODE == MAX11254_MODE_IT
	m->SPIdone = false;
	HAL_SPI_Transmit_IT(m->spi, data, length);
#elif MAX11254_MODE == MAX11254_MODE_DMA
	m->SPIdone = false;
	HAL_SPI_Transmit_DMA(m->spi, data, length);
#endif
#if MAX11254_MODE != MAX11254_MODE_POLLING
	// Wait for transmission to finish
	while (!m->SPIdone) {
		vTaskDelay(1);
	}
#endif
	m->CSgpio->BSRR = m->CSpin;
	xSemaphoreGive(xMutexSPI1);
	return MAX11254_RES_OK;
}

static max11254_result_t SPIWriteRead(max11254_t *m, uint8_t *write, uint8_t *read, uint8_t length) {
	xSemaphoreTake(xMutexSPI1, portMAX_DELAY);
	m->CSgpio->BSRR = m->CSpin << 16;
	// Start the transmission
#if MAX11254_MODE == MAX11254_MODE_POLLING
	HAL_SPI_TransmitReceive(m->spi, write, read, length, 1000);
#elif MAX11254_MODE == MAX11254_MODE_IT
	m->SPIdone = false;
	HAL_SPI_TransmitReceive_IT(m->spi, write, read, length);
#elif MAX11254_MODE == MAX11254_MODE_DMA
	m->SPIdone = false;
	HAL_SPI_TransmitReceive_DMA(m->spi, write, read, length);
#endif
#if MAX11254_MODE != MAX11254_MODE_POLLING
	// Wait for transmission to finish
	while (!m->SPIdone) {
		vTaskDelay(1);
	}
#endif
	m->CSgpio->BSRR = m->CSpin;
	xSemaphoreGive(xMutexSPI1);
	return MAX11254_RES_OK;
}

static max11254_result_t Command(max11254_t *m, max11254_mode_t mode, max11254_rate_t rate) {
	uint8_t cmd = 0x80 | (uint8_t) mode | (uint8_t) rate;
	return SPIWrite(m, &cmd, 1);
}

static uint8_t PrepareData(max11254_reg_t reg, uint8_t *dest, uint32_t value) {
	uint8_t *v8 = (uint8_t*) &value;
	switch(reg) {
	case MAX11254_REG_STAT:
	case MAX11254_REG_CHMAP0:
	case MAX11254_REG_CHMAP1:
	case MAX11254_REG_SOC:
	case MAX11254_REG_SGC:
	case MAX11254_REG_SCOC:
	case MAX11254_REG_SCGC:
	case MAX11254_REG_DATA0:
	case MAX11254_REG_DATA1:
	case MAX11254_REG_DATA2:
	case MAX11254_REG_DATA3:
	case MAX11254_REG_DATA4:
	case MAX11254_REG_DATA5:
		dest[0] = v8[2];
		dest[1] = v8[1];
		dest[2] = v8[0];
		return 3;
	case MAX11254_REG_DELAY:
		dest[0] = v8[1];
		dest[1] = v8[0];
		return 2;
	case MAX11254_REG_CTRL1:
	case MAX11254_REG_CTRL2:
	case MAX11254_REG_CTRL3:
	case MAX11254_REG_GPIO_CTRL:
	case MAX11254_REG_SEQ:
	case MAX11254_REG_GPO_DIR:
		dest[0] = v8[0];
		return 1;
	}
	// should not get here
	return 0;
}

static uint8_t RegLength(max11254_reg_t reg) {
	switch(reg) {
	case MAX11254_REG_STAT:
	case MAX11254_REG_CHMAP0:
	case MAX11254_REG_CHMAP1:
	case MAX11254_REG_SOC:
	case MAX11254_REG_SGC:
	case MAX11254_REG_SCOC:
	case MAX11254_REG_SCGC:
	case MAX11254_REG_DATA0:
	case MAX11254_REG_DATA1:
	case MAX11254_REG_DATA2:
	case MAX11254_REG_DATA3:
	case MAX11254_REG_DATA4:
	case MAX11254_REG_DATA5:
		return 3;
	case MAX11254_REG_DELAY:
		return 2;
	case MAX11254_REG_CTRL1:
	case MAX11254_REG_CTRL2:
	case MAX11254_REG_CTRL3:
	case MAX11254_REG_GPIO_CTRL:
	case MAX11254_REG_SEQ:
	case MAX11254_REG_GPO_DIR:
		return 1;
	}
	// should not get here
	return 0;
}

static uint32_t Extract(max11254_reg_t reg, uint8_t *src) {
	uint32_t ret = 0;
	switch(reg) {
	case MAX11254_REG_STAT:
	case MAX11254_REG_CHMAP0:
	case MAX11254_REG_CHMAP1:
	case MAX11254_REG_SOC:
	case MAX11254_REG_SGC:
	case MAX11254_REG_SCOC:
	case MAX11254_REG_SCGC:
	case MAX11254_REG_DATA0:
	case MAX11254_REG_DATA1:
	case MAX11254_REG_DATA2:
	case MAX11254_REG_DATA3:
	case MAX11254_REG_DATA4:
	case MAX11254_REG_DATA5:
		ret = *src++;
		ret <<= 8;
		/* no break */
	case MAX11254_REG_DELAY:
		ret += *src++;
		ret <<= 8;
		/* no break */
	case MAX11254_REG_CTRL1:
	case MAX11254_REG_CTRL2:
	case MAX11254_REG_CTRL3:
	case MAX11254_REG_GPIO_CTRL:
	case MAX11254_REG_SEQ:
	case MAX11254_REG_GPO_DIR:
		ret += *src;
		break;
	}

	return ret;
}

static max11254_result_t Write(max11254_t *m, max11254_reg_t reg, uint32_t data) {
	uint8_t d[4];
	// build register address + write command
	d[0] = 0xC0 | ((uint8_t) reg << 1);
	// copy necessary data bytes
	uint8_t length = PrepareData(reg, &d[1], data);
	return SPIWrite(m, d, length + 1);
}

static uint32_t Read(max11254_t *m, max11254_reg_t reg) {
	uint8_t rec[4], snd[4];
	uint8_t length = RegLength(reg);
	// build register address + read command
	snd[0] = 0xC1 | ((uint8_t) reg << 1);
	if (SPIWriteRead(m, snd, rec, length + 1) != MAX11254_RES_OK) {
		return 0;
	}
	return Extract(reg, &rec[1]);
}

static max11254_result_t SetBits(max11254_t *m, max11254_reg_t reg, uint32_t mask) {
	uint32_t val = Read(m, reg);
	val |= mask;
	return Write(m, reg, val);
}

static max11254_result_t ClearBits(max11254_t *m, max11254_reg_t reg, uint32_t mask) {
	uint32_t val = Read(m, reg);
	val &= ~mask;
	return Write(m, reg, val);
}

static uint8_t ModifyReg(max11254_t *m, max11254_reg_t reg, uint32_t mask, uint32_t value) {
	uint32_t val = Read(m, reg);
	// clear bits indicated by mask
	val &= ~mask;
	// set bits depending on mask and value
	val |= (mask & value);
	return Write(m, reg, val);
}

max11254_result_t max11254_init(max11254_t *m){
	// initialize GPIO pins
	GPIO_InitTypeDef gpio;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	gpio.Pin = m->CSpin;
	gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(m->CSgpio, &gpio);
	gpio.Mode = GPIO_MODE_IT_FALLING;
	gpio.Pull = GPIO_PULLUP;
	gpio.Pin = m->RDYBpin;
	HAL_GPIO_Init(m->RDYBgpio, &gpio);

	// Set CS pin high
	m->CSgpio->BSRR = m->CSpin;

	max11254_set_MUX_delay(m, 200);
	max11254_enable_MUX_delay(m, 1);

	max11254_set_PGA(m, MAX11254_GAIN128, MAX11254_PGAMODE_LOWNOISE);

	max11254_set_pdmode(m, MAX11254_PDMODE_SLEEP);
	max11254_power_down(m);

	// read back CTRL1 to check whether SPI communication worked
	uint32_t stat = Read(m, MAX11254_REG_CTRL1);
	if (stat != 0x12) {
		LOG(Log_MAX11254, LevelError, "Invalid ctrl1 register: %lx", stat);
		return MAX11254_RES_ERROR;
	}

	return MAX11254_RES_OK;
}

max11254_state_t max11254_get_state(max11254_t *m) {
	return Read(m, MAX11254_REG_STAT);
}
uint8_t max11254_out_of_range(max11254_state_t s) {
	return (s & 0x000300);
}
max11254_pdmode_t max11254_pdmode_from_state(max11254_state_t s) {
	return (max11254_pdmode_t) ((s & 0x00000C) >> 2);
}
uint8_t max11254_has_error(max11254_state_t s) {
	return (s & 0x00BF00) != 0x000000;
}

void max11254_set_pdmode(max11254_t *m, max11254_pdmode_t pdmode) {
	ModifyReg(m, MAX11254_REG_CTRL1, 0x30, (uint8_t) pdmode << 4);
}
void max11254_power_down(max11254_t *m) {
	Command(m, MAX11254_MODE_POWERDOWN, MAX11254_RATE_CONT1_9_SINGLE50);
}
int32_t max11254_read_result(max11254_t *m, uint8_t channel) {
	ASSERT(channel < 6);
	int32_t ret = Read(m, (max11254_reg_t) ((int) MAX11254_REG_DATA0 + channel));
	return util_sign_extend_32(ret, 24);
}

// Sequence Mode 1 functions
int32_t max11254_single_conversion(max11254_t *m, uint8_t channel,
		max11254_rate_t rate) {
	ASSERT(channel < 6);
	// select sequencer mode 1: SEQ:Mode = 00
	ModifyReg(m, MAX11254_REG_SEQ, 0x18, 0);

	// select channel: SEQ:MUX
	ModifyReg(m, MAX11254_REG_SEQ, 0xE0, channel << 5);

	// select single cycle: CTRL1:SCYCLE, CTRL1:CONTSC
	ModifyReg(m, MAX11254_REG_CTRL1, 0x03, 0x02);

	// start conversion
	Command(m, MAX11254_MODE_SEQUENCER, rate);

	// wait for conversion to finish
	uint16_t max_delay = pdMS_TO_TICKS(100);
	while (m->RDYBgpio->IDR & m->RDYBpin) {
		vTaskDelay(1);
		if (!--max_delay) {
			LOG(Log_MAX11254, LevelError,
					"Timed out waiting for single measurement");
			return 0;
		}
	}

	// read result
	return max11254_read_result(m, channel);
}
max11254_result_t max11254_continuous_conversion(max11254_t *m, uint8_t channel,
		max11254_rate_t rate, exti_callback_t cb, void *ptr) {
	ASSERT(channel < 6);
	if (exti_set_callback(m->RDYBgpio, m->RDYBpin, EXTI_TYPE_FALLING,
			EXTI_PULL_UP, cb, ptr) != EXTI_RES_OK) {
		return MAX11254_RES_ERROR;
	}

	// select sequencer mode 1: SEQ:Mode = 00
	ModifyReg(m, MAX11254_REG_SEQ, 0x18, 0);

	// select channel: SEQ:MUX
	ModifyReg(m, MAX11254_REG_SEQ, 0xE0, channel << 5);

	// select continuous cycle: CTRL1:SCYCLE, CTRL1:CONTSC
	ModifyReg(m, MAX11254_REG_CTRL1, 0x03, 0x03);

	if (max11254_has_error(max11254_get_state(m))) {
		LOG(Log_MAX11254, LevelError,
				"Unable to start continuous conversion due to error");
		return MAX11254_RES_ERROR;
	}

	// start conversion
	Command(m, MAX11254_MODE_SEQUENCER, rate);

	return MAX11254_RES_OK;
}

void max11254_sequence_enable_channel(max11254_t *m, uint8_t channel,
		uint8_t order, max11254_gpio_t gpio) {
	ASSERT(channel < 6);
	ASSERT(order >= 1 && order <= 6);

	max11254_reg_t reg = MAX11254_REG_CHMAP0;
	if(channel > 2) {
		reg = MAX11254_REG_CHMAP1;
	}
	uint32_t val = 0x02			// channel enable
			| (order << 2);		// select order of the channel
	if(gpio != MAX11254_GPIO_None) {
		val |= (int) gpio << 6; // select GPIO pin
		val |= 0x01;			// enable GPIO for channel
	}

	uint8_t shift = (channel % 3) * 8;
	uint32_t mask = 0xFF << shift;
	val <<= shift;
	ModifyReg(m, reg, mask, val);
}
void max11254_sequence_disable_channel(max11254_t *m, uint8_t channel) {
	ASSERT(channel < 6);

	max11254_reg_t reg = MAX11254_REG_CHMAP0;
	if(channel > 2) {
		reg = MAX11254_REG_CHMAP1;
	}
	// clear the channel mapping
	uint32_t mask = 0xFF;
	uint8_t shift = (channel % 3) * 8;
	mask <<= shift;

	ClearBits(m, reg, mask);
}

max11254_result_t max11254_scan_conversion_static_gpio(max11254_t *m,
		max11254_rate_t rate, exti_callback_t cb, void *ptr) {
	// select sequencer mode 2: SEQ:MODE = 01
	ModifyReg(m, MAX11254_REG_SEQ, 0x18, 0x08);

	// RDBY asserted only at end of sequence: SEQ:RDYBEN = 1
	SetBits(m, MAX11254_REG_SEQ, 0x01);

	// select single cycle: CTRL1:SCYCLE, CTRL1:CONTSC
	ModifyReg(m, MAX11254_REG_CTRL1, 0x03, 0x02);

	// Enable finished conversion callback
	if (exti_set_callback(m->RDYBgpio, m->RDYBpin, EXTI_TYPE_FALLING,
			EXTI_PULL_UP, cb, ptr) != EXTI_RES_OK) {
		return MAX11254_RES_ERROR;
	}

	if (max11254_has_error(max11254_get_state(m))) {
		LOG(Log_MAX11254, LevelError,
				"Unable to start sequencer mode 2 due to error");
		return MAX11254_RES_ERROR;
	}

	// start conversion
	Command(m, MAX11254_MODE_SEQUENCER, rate);

	return MAX11254_RES_OK;
}
max11254_result_t max11254_scan_conversion_dynamic_gpio(max11254_t *m,
		max11254_rate_t rate, exti_callback_t cb, void *ptr) {
	// select sequencer mode 3: SEQ:MODE = 10
	ModifyReg(m, MAX11254_REG_SEQ, 0x18, 0x10);

	// RDBY asserted only at end of sequence: SEQ:RDYBEN = 1
	SetBits(m, MAX11254_REG_SEQ, 0x01);

	// Enable GPO/GPIO sequencing: CTRL3:GPO_MODE
	SetBits(m, MAX11254_REG_CTRL3, 0x60);

	// select single cycle: CTRL1:SCYCLE, CTRL1:CONTSC
	ModifyReg(m, MAX11254_REG_CTRL1, 0x03, 0x02);

	// Enable finished conversion callback
	if (exti_set_callback(m->RDYBgpio, m->RDYBpin, EXTI_TYPE_FALLING,
			EXTI_PULL_UP, cb, ptr) != EXTI_RES_OK) {
		return MAX11254_RES_ERROR;
	}

	if (max11254_has_error(max11254_get_state(m))) {
		LOG(Log_MAX11254, LevelError,
				"Unable to start sequencer mode 3 due to error");
		return MAX11254_RES_ERROR;
	}

	// start conversion
	Command(m, MAX11254_MODE_SEQUENCER, rate);

	return MAX11254_RES_OK;
}

void max11254_enable_MUX_delay(max11254_t *m, uint8_t enable) {
	uint8_t val = enable ? 0x02 : 0x00;
	ModifyReg(m, MAX11254_REG_SEQ, 0x02, val);
}
void max11254_enable_GPO_delay(max11254_t *m, uint8_t enable) {
	uint8_t val = enable ? 0x04 : 0x00;
	ModifyReg(m, MAX11254_REG_SEQ, 0x04, val);
}
void max11254_set_GPO_delay(max11254_t *m, uint16_t us) {
	// 1LSB equals 20us
	uint8_t val = us / 20;
	ModifyReg(m, MAX11254_REG_DELAY, 0x00FF, val);
}
void max11254_set_MUX_delay(max11254_t *m, uint16_t us) {
	// 1LSB equals 4us
	uint8_t val = us / 4;
	ModifyReg(m, MAX11254_REG_DELAY, 0xFF00, val << 8);
}
void max11254_set_PGA(max11254_t *m, max11254_pga_gain_t g,
		max11254_pga_mode_t pgamode) {
	uint8_t val = (int) g;
	if (val > 0) {
		// enable PGA
		val |= 0x08;
	}
	if (pgamode == MAX11254_PGAMODE_LOWPOWER) {
		val |= 0x10;
	}
	ModifyReg(m, MAX11254_REG_CTRL2, 0x1F, val);
}

void max11254_set_GPIO(max11254_t *m, max11254_gpio_t gpio,
		max11254_pinstate_t state) {
	uint8_t shift = 1;
	switch (gpio) {
	case MAX11254_GPIO_GPIO0:
		shift = 0;
		/* no break */
	case MAX11254_GPIO_GPIO1:
		if (state == MAX11254_PINSTATE_ALTERNATIVE) {
			ClearBits(m, MAX11254_REG_GPIO_CTRL, 0x40 << shift);
		} else {
			uint8_t mask = 0x49 << shift;
			uint8_t set = 0x40;
			if (state == MAX11254_PINSTATE_HIGH) {
				// pin is set as output high
				set |= 0x09;
			} else if (state == MAX11254_PINSTATE_LOW) {
				// pin is set as output low
				set |= 0x08;
			}
			set <<= shift;
			ModifyReg(m, MAX11254_REG_GPIO_CTRL, mask, set);
		}
		break;
	case MAX11254_GPIO_GPO0:
		shift = 0;
		/* no break */
	case MAX11254_GPIO_GPO1:
		if (state == MAX11254_PINSTATE_HIGH) {
			ClearBits(m, MAX11254_REG_GPO_DIR, 0x01 << shift);
		} else if (state == MAX11254_PINSTATE_LOW) {
			SetBits(m, MAX11254_REG_GPO_DIR, 0x01 << shift);
		} else {
			// GPO0/1 implemented as open drain, other states not supported
		}
		break;
	}
}
uint8_t max11254_get_GPIO(max11254_t *m, max11254_gpio_t gpio) {
	uint8_t shift = 1;
	uint8_t ret = 0;
	switch (gpio) {
	case MAX11254_GPIO_GPIO0:
		shift = 0;
		/* no break */
	case MAX11254_GPIO_GPIO1:
		if (Read(m, MAX11254_REG_GPIO_CTRL) & (0x01 << shift)) {
			ret = 1;
		}
		break;
	case MAX11254_GPIO_GPO0:
	case MAX11254_GPIO_GPO1:
		// can't read from these pins
		break;
	}

	return ret;
}

#if MAX11254_MODE != MAX11254_MODE_POLLING
void max11254_on_SPI_complete(max11254_t *m) {
	m->SPIdone = 1;
}
#endif
