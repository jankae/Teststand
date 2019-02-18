#include "PPMDriver.hpp"
#include "stm32f1xx.h"
#include "gui.hpp"
#include "cast.hpp"
#include "Config.hpp"

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

PPMDriver::PPMDriver(coords_t displaySize) {
	features.OnOff = true;
	features.Control.Percentage = true;

	widthMin = widthOffDefault;
	widthCutoff = widthMinDefault;
	widthMax = widthMaxDefault;
	updatePeriod = updatePeriodDefault;
	running = false;
	setValue = 0;

	// configure timer to generate PPM signal
	TIM_CLK_EN();
	const uint32_t APB1_freq = HAL_RCC_GetPCLK1Freq();
	const uint32_t AHB_freq = HAL_RCC_GetHCLKFreq();
	uint32_t timerFreq = APB1_freq == AHB_freq ? APB1_freq : APB1_freq * 2;

	TIM_BASE->PSC = (timerFreq / 1000000UL) - 1;
	TIM_BASE->ARR = updatePeriod - 1;
	HAL_NVIC_SetPriority(TIM_NVIC_ISR, 0, 0);
	HAL_NVIC_EnableIRQ(TIM_NVIC_ISR);

	// enable overflow and compare match 1 interrupt
	TIM_BASE->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
	TIM_BASE->CCR1 = 0;
	TIM_BASE->CR1 |= TIM_CR1_CEN;

	UpdatePPM();

	auto c = new Container(displaySize);
	c->attach(new Label("Pulsewidth", Font_Big), COORDS(0,2));
	c->attach(new Label("Min:", Font_Big), COORDS(15,20));
	auto eMin = new Entry(&widthMin, widthHigh, widthLow, Font_Big, 7,
			Unit::Time);
	eMin->setCallback(
			pmf_cast<void (*)(void*, Widget*), PPMDriver, &PPMDriver::UpdatePPM>::cfn,
			this);
	c->attach(eMin, COORDS(15,36));

	c->attach(new Label("Cutoff:", Font_Big), COORDS(15,57));
	auto eCutoff = new Entry(&widthCutoff, widthHigh, widthLow, Font_Big, 7,
			Unit::Time);
	eCutoff->setCallback(
			pmf_cast<void (*)(void*, Widget*), PPMDriver, &PPMDriver::UpdatePPM>::cfn,
			this);
	c->attach(eCutoff, COORDS(15, 73));

	c->attach(new Label("Max:", Font_Big), COORDS(15,94));
	auto eMax = new Entry(&widthMax, widthHigh, widthLow, Font_Big, 7,
			Unit::Time);
	eMax->setCallback(
			pmf_cast<void (*)(void*, Widget*), PPMDriver, &PPMDriver::UpdatePPM>::cfn,
			this);
	c->attach(eMax, COORDS(15, 110));

	c->attach(new Label("Period:", Font_Big), COORDS(15, 131));
	auto ePeriod = new Entry(&updatePeriod, updataPeriodMax, updataPeriodMin,
			Font_Big, 7, Unit::Time);
	ePeriod->setCallback(
			pmf_cast<void (*)(void*, Widget*), PPMDriver, &PPMDriver::UpdatePPM>::cfn,
			this);
	c->attach(ePeriod, COORDS(15, 147));

	topWidget = c;

	configIndex = Config::AddParseFunctions(
			pmf_cast<Config::WriteFunc, PPMDriver, &PPMDriver::WriteConfig>::cfn,
			pmf_cast<Config::ReadFunc, PPMDriver, &PPMDriver::ReadConfig>::cfn,
			this);
}

PPMDriver::~PPMDriver() {
	// disable timer
	HAL_NVIC_DisableIRQ(TIM_NVIC_ISR);
	TIM_BASE->CR1 &= ~TIM_CR1_CEN;
	// clear PPM pin
	PPM_GPIO_Port->BSRR = PPM_Pin << 16;

	Config::RemoveParseFunctions(configIndex);

	if(topWidget) {
		delete topWidget;
	}
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

void PPMDriver::UpdatePPM(Widget*) {
	uint16_t compare = (int64_t) (widthMax - widthMin) * setValue
			/ Unit::maxPercent;
	compare += widthMin;
//	if (TIM_BASE->ARR != updatePeriod - 1) {
//		TIM_BASE->CR1 &= ~TIM_CR1_CEN;
		TIM_BASE->ARR = updatePeriod - 1;
//		TIM_BASE->CR1 |= TIM_CR1_CEN;
//	}
	if (running) {
		if (compare < widthCutoff) {
			compare = widthCutoff;
		}
		TIM_BASE->CCR1 = compare;
	} else {
		TIM_BASE->CCR1 = widthMin;
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

bool PPMDriver::WriteConfig() {
	File::WriteLine("# PPM driver settings\n");
	const File::Entry entries[] = {
		{ "Driver::PPM::WidthMin", &widthMin, File::PointerType::INT32},
		{ "Driver::PPM::WidthCutoff", &widthCutoff, File::PointerType::INT32},
		{ "Driver::PPM::WidthMax", &widthMax, File::PointerType::INT32},
		{ "Driver::PPM::UpdatePeriod", &updatePeriod, File::PointerType::INT32},
	};
	File::WriteParameters(entries, 4);
	return true;
}

bool PPMDriver::ReadConfig() {
	const File::Entry entries[] = {
		{ "Driver::PPM::WidthMin", &widthMin, File::PointerType::INT32},
		{ "Driver::PPM::WidthCutoff", &widthCutoff, File::PointerType::INT32},
		{ "Driver::PPM::WidthMax", &widthMax, File::PointerType::INT32},
		{ "Driver::PPM::UpdatePeriod", &updatePeriod, File::PointerType::INT32},
	};
	File::ReadParameters(entries, 4);
	setValue = 0;
	UpdatePPM();
	topWidget->requestRedrawFull();
	return true;
}
