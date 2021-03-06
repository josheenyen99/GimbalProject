#include "eventHandler.h"
#include "cmsis_os.h"
#include "stm32l4xx_hal.h"

uint32_t prev_time = 0;
uint32_t cur_time;

void setpointButtons(){

	osEventFlagsSet( setPointButtonEvents, 0x50 );

}

void modeChangeButton(){
	cur_time = HAL_GetTick();
	//button switching issues while prototyping
	if (cur_time - prev_time > 300){
	osEventFlagsSet( stateMachineEvents, 0x69 );
	}

	prev_time = cur_time;

}
