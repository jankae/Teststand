#include "PPMDriver.hpp"
#include "stm32f1xx.h"
#include "gui.hpp"

#define TIM 						2

/* Automatically build register names based on timer selection */
#define TIM_M2(y) 					TIM ## y
#define TIM_M1(y)  					TIM_M2(y)
#define TIM_BASE					TIM_M1(TIM)

#define TIM_CLK_EN_M2(y) 			__HAL_RCC_TIM ## y ## _CLK_ENABLE
#define TIM_CLK_EN_M1(y)  			TIM_CLK_EN_M2(y)
#define TIM_CLK_EN					TIM_CLK_EN_M1(TIM)

#define TIM_HANDLER_M2(x) 			TIM ## x ## _IRQHandler
#define TIM_HANDLER_M1(x)  			TIM_HANDLER_M2(x)
#define TIM_HANDLER					TIM_HANDLER_M1(TIM)

#define TIM_NVIC_ISR_M2(x) 			TIM ## x ## _IRQn
#define TIM_NVIC_ISR_M1(x)  		TIM_NVIC_ISR_M2(x)
#define TIM_NVIC_ISR				TIM_NVIC_ISR_M1(TIM)

PPMDriver::PPMDriver() {
	features.OnOff = true;
	features.Control.Percentage = true;

	widthOff = widthOffDefault;
	widthMin = widthMinDefault;
	widthMax = widthMaxDefault;
	running = false;
	setValue = 0;

	// configure timer to generate PPM signal
	TIM_CLK_EN();
	const uint32_t APB1_freq = HAL_RCC_GetPCLK1Freq();
	const uint32_t AHB_freq = HAL_RCC_GetHCLKFreq();
	uint32_t timerFreq = APB1_freq == AHB_freq ? APB1_freq : APB1_freq * 2;

	TIM_BASE->PSC = (timerFreq / 1000000UL) - 1;
	// overflow after 20ms
	TIM_BASE->ARR = 19999;
	HAL_NVIC_SetPriority(TIM_NVIC_ISR, 0, 0);
	HAL_NVIC_EnableIRQ(TIM_NVIC_ISR);

	// enable overflow and compare match 1 interrupt
	TIM_BASE->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
	TIM_BASE->CCR1 = 0;
	TIM_BASE->CR1 |= TIM_CR1_CEN;

	UpdatePPM();
	// TODO GUI
}

PPMDriver::~PPMDriver() {
	// disable timer
	HAL_NVIC_DisableIRQ(TIM_NVIC_ISR);
	TIM_BASE->CR1 &= ~TIM_CR1_CEN;
	// clear PPM pin
	PPM_GPIO_Port->BSRR = PPM_Pin << 16;

	// TODO GUI
}

bool PPMDriver::SetRunning(bool running) {
	this->running = running;
	UpdatePPM();
	return true;
}

bool PPMDriver::SetControl(ControlMode mode, int32_t value) {
	if(mode != ControlMode::Percentage) {
		return false;
	} else {
		setValue = value;
		UpdatePPM();
		return true;
	}
}

Driver::Readback PPMDriver::GetData() {
	Readback ret;
	memset(&ret, 0, sizeof(ret));
	return ret;
}

void PPMDriver::UpdatePPM() {
	uint16_t compare = (int64_t) (widthMax - widthOff) * setValue
			/ Unit::maxPercent;
	compare += widthOff;
	if(running) {
		if (compare < widthMin) {
			compare = widthMin;
		}
		TIM_BASE->CCR1 = compare;
	} else {
		TIM_BASE->CCR1 = widthOff;
	}
}
extern "C" {
void TIM_HANDLER(void) {
	if (TIM_BASE->SR & TIM_SR_UIF) {
		// clear the flag
		TIM_BASE->SR = ~TIM_SR_UIF;
		// set pin if PPM active
		if (TIM_BASE->CCR1 > 0) {
			PPM_GPIO_Port->BSRR = PPM_Pin;
		}
	} if(TIM_BASE->SR & TIM_SR_CC1IF) {
		// clear the flag
		TIM_BASE->SR = ~TIM_SR_CC1IF;
		// clear pin if PPM active
		if (TIM_BASE->CCR1 > 0) {
			PPM_GPIO_Port->BSRR = PPM_Pin << 16;
		}
	}
}
}
