#include <stm32f4xx.h>
float temp = 0.0;
int main(void) {
	/* Flash settings (see RM0090 rev9, p80) */

	FLASH->ACR = FLASH_ACR_LATENCY_5WS               // 6 CPU cycle wait 
          | FLASH_ACR_PRFTEN                    // enable prefetch 
          | FLASH_ACR_ICEN                      // instruction cache enable *
          | FLASH_ACR_DCEN;                     // data cache enable 
	
	/*****************************************************/
	/****   CLOCK GENERATION USING PLL AND EXT CLOCK   ***/
	/*****************************************************/
	
	RCC->CFGR = RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_PPRE1_DIV4; //APB1 PRESCALER AND APB2 PRESCALER
	RCC->CR = RCC_CR_HSEON; //ENABLE EXTERNAL OSCILLATOR
	/* Wait for locked external oscillator */
	while((RCC->CR & RCC_CR_HSERDY) != RCC_CR_HSERDY);
	
	/* PLL config */
	RCC->PLLCFGR =
          RCC_PLLCFGR_PLLSRC_HSE                /* PLL source */
        | (4 << 0)                              /* PLL input division */
        | (168 << 6)                            /* PLL multiplication */
        | (0 << 16)                             /* PLL sys clock division */
        | (7 << 24);                            /* PLL usb clock division =48MHz */
	RCC->CR |= RCC_CR_PLLON;		/* Enable PLL */
	while((RCC->CR & RCC_CR_PLLRDY) != RCC_CR_PLLRDY);	/* Wait for locked PLL */
	RCC->CFGR &= ~RCC_CFGR_SW; 		/* select system clock */			/* clear */
	RCC->CFGR |= RCC_CFGR_SW_PLL | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;   /* SYSCLK is PLL */
	while((RCC->CFGR & RCC_CFGR_SW_PLL) != RCC_CFGR_SW_PLL);		/* Wait for SYSCLK to be PLL */
	
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
	
	GPIOA->MODER   |= GPIO_MODER_MODE0_1 
													| GPIO_MODER_MODE0_0 
													| GPIO_MODER_MODE1_0 
													| GPIO_MODER_MODE1_1 
													| GPIO_MODER_MODE2_0 
													| GPIO_MODER_MODE2_1
													| GPIO_MODER_MODE3_0 
													| GPIO_MODER_MODE3_1
													| GPIO_MODER_MODE4_0 
													| GPIO_MODER_MODE4_1
													| GPIO_MODER_MODE5_0 
													| GPIO_MODER_MODE5_1
													| GPIO_MODER_MODE6_0 
													| GPIO_MODER_MODE6_1
													| GPIO_MODER_MODE7_0 
													| GPIO_MODER_MODE7_1;		//ANALOG MODE (PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7)
	
	RCC->APB2ENR = 1 << 8; //ADC1 ON
	
	ADC->CCR |= 1 << 16; //ADCPRE is set to 4 i.e APB2 / 4 = 21MHz for ADCCLK
	
	ADC1->SMPR2 |= 6 << 0 | 6 << 3 | 6 << 6; //Sampling time set to 144 cycles for channel 0
	
	ADC1->SMPR1 |= 6 << 18;
	
	ADC->CCR |= 1 << 23;
	
	ADC1->CR1 |= 0 << 24 | 1 << 10 | 1 << 8; //12bit_Resolution|JAUTO|Scan_Mode
	
	ADC1->CR2 = 1 << 0 | 1 << 1 | 1 << 10 | 1 << 8 | 1 << 9; // ADON, CONT, EOCS, DMA_ENABLE, DDS
	
	ADC1->SQR3 |= 0 << 0 |1 << 5|2 << 10| 16 << 15;//|4 << 20|5 << 25; // put channel number (CH0, CH1, CH2, CH3, CH4, CH5)
	
	//ADC1->SQR2 |= 6 << 0|7 << 5; //put channel number (CH6, CH7)
	
	ADC1->SQR1 |= 3 << 20; //temp | L = 8 conversions(n-1) (8 ADC conversions)
	
	ADC1->JSQR |= 4 << 0 | 5 << 5 | 6 << 10 | 7 << 15 | 3 << 20; //JL=4
	
	uint32_t adc1[4];
	uint32_t jadc1[4];
	while((DMA2_Stream4->CR & 0x00000001) == 1);  //wait for stream4 to be 0(stop)
	DMA2_Stream4->PAR |= (uint32_t)&ADC1->DR; //peripheral data register address
	DMA2_Stream4->M0AR |= (uint32_t)&adc1;  //memory data register address
	DMA2_Stream4->NDTR = 4;  // No. of data transfer 
	DMA2_Stream4->CR =  0 << 6 | 1 << 8 | 1 << 10 | 2 << 11 | 2 << 13 | 2 << 16 | 0 << 25;  //DIR,CIRC,MINC,PSIZE,MSIZE,PL,CHSEL
	DMA2_Stream4->FCR |= 0 << 2 ; //| 3 << 0; // Direct mode
	DMA2_Stream4->CR |= 1 << 0; // start stream4
	
	ADC1->CR2 |= ADC_CR2_SWSTART;  //start conversion regular
	ADC1->CR2 |= ADC_CR2_JSWSTART; //injected conversion start
	
	/**************************************************************************/
	/* TO GET ADC READINGS CONTINOUSLY WITHOUT SETTING THE CONT BIT IN ADC->CR2
	REGISTER COPY AND PASTE ABOVE LINE IN WHILE LOOP BUT MAKE SURE TO KEEP 
	DMA IN CIRCULAR MODE OTHERWISE IT WILL GIVE OVERRUN ERROR.                */
	/**************************************************************************/
	
	while(1) {
		while(!ADC_SR_JEOC);
		jadc1[0] = ADC1->JDR1;
		jadc1[1] = ADC1->JDR2;
		jadc1[2] = ADC1->JDR3;
		jadc1[3] = ADC1->JDR4;
		ADC1->SR |= 0 << 2;
		if(DMA2->HISR == 0x00000030) {
			DMA2->HIFCR = 1 << 5 | 1 << 4;  // Clear DMA Transfer Complete Flag
		} 
		temp = ((adc1[3]-760)/2500)+25;
	}
}
