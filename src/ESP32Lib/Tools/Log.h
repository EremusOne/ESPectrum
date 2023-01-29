/*
	Author: bitluni 2019
	License: 
	Creative Commons Attribution ShareAlike 4.0
	https://creativecommons.org/licenses/by-sa/4.0/
	
	For further details check out: 
		https://youtube.com/bitlunislab
		https://github.com/bitluni
		http://bitluni.net
*/
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DELAY(n) vTaskDelay((n) / portTICK_PERIOD_MS)

#define DEBUG_PRINTLN(a) printf("%s\n",a)
#define DEBUG_PRINT(a) printf(a)
#define DEBUG_PRINTLNF(a, f) printf(a, f)
#define DEBUG_PRINTF(a, f) printf(a, f)

#define ERROR(a) {printf("%s\n",a); DELAY(3000); /* throw 0; */ };
