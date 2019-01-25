#ifndef IOX_MAX11254_MAX11254_H_
#define IOX_MAX11254_MAX11254_H_

#define MAX11254_MODE_POLLING		0
#define MAX11254_MODE_IT			1
#define MAX11254_MODE_DMA			2

#define MAX11254_MODE				MAX11254_MODE_POLLING

#include "stm.h"
#include "exti.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	SPI_HandleTypeDef *spi;
	GPIO_TypeDef *CSgpio;
	uint16_t CSpin;
	GPIO_TypeDef *RDYBgpio;
	uint16_t RDYBpin;
#if MAX11254_MODE != MAX11254_MODE_POLLING
	uint8_t SPIdone;
#endif
} max11254_t;

typedef enum {
	MAX11254_MODE_POWERDOWN = 0X10,
	MAX11254_MODE_CALIBRATION = 0X20,
	MAX11254_MODE_SEQUENCER = 0X30,
} max11254_mode_t;

typedef enum {
	MAX11254_RATE_CONT1_9_SINGLE50 = 0X00,
	MAX11254_RATE_CONT3_9_SINGLE62_5 = 0X01,
	MAX11254_RATE_CONT7_8_SINGLE100 = 0X02,
	MAX11254_RATE_CONT15_6_SINGLE125 = 0X03,
	MAX11254_RATE_CONT31_2_SINGLE200 = 0X04,
	MAX11254_RATE_CONT62_5_SINGLE250 = 0X05,
	MAX11254_RATE_CONT125_SINGLE400 = 0X06,
	MAX11254_RATE_CONT250_SINGLE500 = 0X07,
	MAX11254_RATE_CONT500_SINGLE800 = 0X08,
	MAX11254_RATE_CONT1000_SINGLE1000 = 0X09,
	MAX11254_RATE_CONT2000_SINGLE1600 = 0X0A,
	MAX11254_RATE_CONT4000_SINGLE2000 = 0X0B,
	MAX11254_RATE_CONT8000_SINGLE3200 = 0X0C,
	MAX11254_RATE_CONT16000_SINGLE4000 = 0X0D,
	MAX11254_RATE_CONT32000_SINGLE6400 = 0X0E,
	MAX11254_RATE_CONT64000_SINGLE12800 = 0X0F,
} max11254_rate_t;

typedef enum {
	MAX11254_REG_STAT = 0x00,
	MAX11254_REG_CTRL1 = 0x01,
	MAX11254_REG_CTRL2 = 0x02,
	MAX11254_REG_CTRL3 = 0x03,
	MAX11254_REG_GPIO_CTRL = 0x04,
	MAX11254_REG_DELAY = 0x05,
	MAX11254_REG_CHMAP1 = 0x06,
	MAX11254_REG_CHMAP0 = 0x07,
	MAX11254_REG_SEQ = 0x08,
	MAX11254_REG_GPO_DIR = 0x09,
	MAX11254_REG_SOC = 0x0A,
	MAX11254_REG_SGC = 0x0B,
	MAX11254_REG_SCOC = 0x0C,
	MAX11254_REG_SCGC = 0x0D,
	MAX11254_REG_DATA0 = 0x0E,
	MAX11254_REG_DATA1 = 0x0F,
	MAX11254_REG_DATA2 = 0x10,
	MAX11254_REG_DATA3 = 0x11,
	MAX11254_REG_DATA4 = 0x12,
	MAX11254_REG_DATA5 = 0x13,
} max11254_reg_t;

typedef enum {
	MAX11254_PDMODE_NOP = 0x00,
	MAX11254_PDMODE_SLEEP = 0x01,
	MAX11254_PDMODE_STANDBY = 0x02,
	MAX11254_PDMODE_RESET = 0x03,
} max11254_pdmode_t;

typedef enum {
	MAX11254_GAIN1 = 0X00,
	MAX11254_GAIN2 = 0X01,
	MAX11254_GAIN4 = 0X02,
	MAX11254_GAIN8 = 0X03,
	MAX11254_GAIN16 = 0X04,
	MAX11254_GAIN32 = 0X05,
	MAX11254_GAIN64 = 0X06,
	MAX11254_GAIN128 = 0X07,
} max11254_pga_gain_t;

typedef enum {
	MAX11254_PGAMODE_LOWNOISE,
	MAX11254_PGAMODE_LOWPOWER,
} max11254_pga_mode_t;

typedef enum {
	MAX11254_GPIO_None,
	MAX11254_GPIO_GPO0 = 0x00,
	MAX11254_GPIO_GPO1 = 0x01,
	MAX11254_GPIO_GPIO0 = 0x02,
	MAX11254_GPIO_GPIO1 = 0x03,
} max11254_gpio_t;

typedef enum {
	MAX11254_PINSTATE_LOW,
	MAX11254_PINSTATE_HIGH,
	MAX11254_PINSTATE_INPUT,
	MAX11254_PINSTATE_ALTERNATIVE,
} max11254_pinstate_t;

typedef enum {
	MAX11254_RES_OK,
	MAX11254_RES_ERROR,
} max11254_result_t;

typedef uint32_t max11254_state_t;

max11254_result_t max11254_init(max11254_t *m);

max11254_state_t max11254_get_state(max11254_t *m);
uint8_t max11254_out_of_range(max11254_state_t s);
max11254_pdmode_t max11254_pdmode_from_state(max11254_state_t s);
uint8_t max11254_has_error(max11254_state_t s);

void max11254_set_pdmode(max11254_t *m, max11254_pdmode_t pdmode);
void max11254_power_down(max11254_t *m);
int32_t max11254_read_result(max11254_t *m, uint8_t channel);

// Sequence Mode 1 functions
int32_t max11254_single_conversion(max11254_t *m, uint8_t channel, max11254_rate_t rate);
max11254_result_t max11254_continuous_conversion(max11254_t *m, uint8_t channel, max11254_rate_t rate, exti_callback_t cb, void *ptr);

// Sequence Mode 2+3 functions
void max11254_sequence_enable_channel(max11254_t *m, uint8_t channel, uint8_t order, max11254_gpio_t gpio);
void max11254_sequence_disable_channel(max11254_t *m, uint8_t channel);

max11254_result_t max11254_scan_conversion_static_gpio(max11254_t *m, max11254_rate_t rate, exti_callback_t cb, void *ptr);
max11254_result_t max11254_scan_conversion_dynamic_gpio(max11254_t *m, max11254_rate_t rate, exti_callback_t cb, void *ptr);

void max11254_enable_MUX_delay(max11254_t *m, uint8_t enable);
void max11254_enable_GPO_delay(max11254_t *m, uint8_t enable);
void max11254_set_GPO_delay(max11254_t *m, uint16_t us);
void max11254_set_MUX_delay(max11254_t *m, uint16_t us);
void max11254_set_PGA(max11254_t *m, max11254_pga_gain_t g, max11254_pga_mode_t pgamode);

void max11254_set_GPIO(max11254_t *m, max11254_gpio_t gpio, max11254_pinstate_t state);
uint8_t max11254_get_GPIO(max11254_t *m, max11254_gpio_t gpio);

#if MAX11254_MODE != MAX11254_MODE_POLLING
	void max11254_on_SPI_complete(max11254_t *m);
#endif

#ifdef __cplusplus
}
#endif

#endif
