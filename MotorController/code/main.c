#include "stm32l432xx.h" /* Microcontroller information */

//global variables
static int button_val;  //stores previously pressed keypad button
//static uint16_t period_scaler = 10
static char button_indices [2]; // array of rows (0) and columns (1) of pressed key
static char button_pressed_indicator = 0; //indicates if a button has been pressed
static unsigned int autoreload = 441; //arr determines period
static unsigned int prescaler = 0;
static unsigned char dutycycle = 0; //ccr determines duty cycle
static int adc_speed;
static int desired_adc_speed = 0;
static int max_overshoot = 100;
static int direction = 0;
static int counter = 0;
static int pwm_value;
static int mode = 0;

//GPIO SETUP
void GPIO_setup()
{
      RCC->AHB2ENR |= 0x03; // enable GPIOA and GPIOB clocks

      //pin setup
      GPIOA->MODER &= ~(0x00FF0FF3); //clear PA0, PA[2:5], PA[8:11]
      GPIOA->MODER |=  (0x00550002); //PA0 to AF, PA[2:5] to input (rows), PA[8:11] to output (col)
      GPIOB->MODER &= ~(0x00003FC3); //clear PB0, PB[3:6]
      GPIOB->MODER |=  (0x00005540); //PB0 to input (trigger), PB[3:6] to output (display)


      //AF setup
      GPIOA->AFR[0] &= ~(0x0000000F); //clear AF[0] to use PA0
      GPIOA->AFR[0] |= 0x01; //bit 0 to AF1

      //PUPDR setup
      GPIOA->PUPDR &= ~(0x00000FF0); //clear PA[5:2]
      GPIOA->PUPDR |= 0x00000550; // PA[5:2] to pull up
}

//INTERRUPT SETUP
void interrupts_setup()
{
      RCC->APB2ENR |= 0x01; //enable syscfg clock

      SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0); //clear EXTI0 bits for PB0
      SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; //set EXTI0 bits for PB0

      EXTI->FTSR1 |= EXTI_FTSR1_FT0; //EXTI0 to falling edge trigger
      EXTI->PR1 = EXTI_PR1_PIF0; //EXTI0 pending bit clear
      EXTI->IMR1 |= EXTI_IMR1_IM0; //enable EXTI0 interrupts

      NVIC_EnableIRQ(EXTI0_IRQn);
      NVIC_ClearPendingIRQ(EXTI0_IRQn);
}

//TIMER SETUP
void timer_setup()
{
      RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN; //enable TIM2

      TIM2->ARR = autoreload;
      TIM2->PSC = prescaler;
      /* Configure prescale and auto-reload values for 1ms intervals */
      //TIM2->PSC = 0; /* load value into PSC */
      //TIM2->ARR = (int) (3999/period_scaler); /* Load value into ARR,initially at 1000 Hz */
      TIM2->CCR1 = autoreload / (dutycycle +1); //init CCR1
//      TIM2->CCR1 = autoreload/2; //set initial pwm compare value

      //set up timer
      TIM2->CR1 |= TIM_CR1_CEN; //enable TIM2 counter

      TIM2->CCMR1 &= ~(0x03); //timer channel to output mode

      TIM2->CCMR1 &= ~(0x70); //clear bits 6:4
      TIM2->CCMR1 |=  (0x60); //set bits for PWM mode 1

      TIM2->CCER &= ~(0x03); //clear bits 1:0
      TIM2->CCER |=  (0x01); //enable timer channel output

//dynamic stuff
//      TIM2->CCMR1 |= TIM_CCMR1_OC1PE; //enable output preload
//      TIM2->CR1 |= TIM_CR1_URS; //update event only in overflow; may need if unstable

//      TIM2->CCER &= ~TIM_CCER_CC1P; //may be necessary to invert signal

      TIM2->DIER |= (TIM_DIER_CC1IE | TIM_DIER_UIE); //enable capture/compare and update interrupts
      TIM2->SR &= ~(TIM_SR_CC1IF | TIM_SR_UIF); //clear pending interrupts before enabling them
      TIM2->CR1 |=1; //start tim2

      /* Enable TIM6 in RCC */
      RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN; /* Enable TIM6 in APB1ENR */

      /* Configure prescale and auto-reload values for 1 second intervals */
      TIM6->PSC = 999; /* Load value into PSC */
      TIM6->ARR = 1789; // 3578 old value

      /* Enable interrupts for TIM6 */
      TIM6->DIER |= TIM_DIER_UIE; /* Enable TIM6 to trigger an interrupt */
      NVIC_EnableIRQ(TIM6_DAC_IRQn); /* Enable TIM6 in NVIC */

      TIM6->CR1 ^= TIM_CR1_CEN;


      __enable_irq(); //enable interrupts
}

void adc_setup()
{
	RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
	RCC->CCIPR &= 0x30000000;
	RCC->CCIPR |= RCC_CCIPR_ADCSEL;

	ADC1->CR &= ~(ADC_CR_DEEPPWD);
	ADC1->CR |= ADC_CR_ADVREGEN;
	for(uint16_t i = 0; i <= 240; i++)
		ADC1->CFGR |= ADC_CFGR_CONT;
	ADC1->CFGR |= ADC_CFGR_OVRMOD;
	ADC1->SQR1 &= 0x0F;
	ADC1->SQR1 &= ~(0x0F << 6);
	ADC1->SQR1 |= 16 << 6;

	ADC1->CR |= ADC_CR_ADEN;
	while (!(ADC1->ISR & ADC_ISR_ADRDY));
	ADC1->CR |= ADC_CR_ADSTART;
}




void get_pressed_button()
{
      /* Drive columns low */
      GPIOA->ODR &= ~(0x00000F00); /* Drive PA[11:8] low */
      /* Short delay for signal propagation */
     uint16_t i = 0;
      for (i = 0; i < 640; i++);
      /* Read pressed row */
      button_indices[0] = (GPIOA->IDR & (0x0000003C)) >> 2; /* Mask to read rows only (PA[5:2]) */
      /* Swap GPIO directions between rows and columns */
      /* Rows */
      GPIOA->MODER &=   ~(0x00000FF0); /* Clear bits for PA[5:2]*/
      GPIOA->MODER |=   (0x00000550); /* General purpose output mode for PA[5:2]*/
      /* Columns */
      GPIOA->MODER &=   ~(0x00FF0000); /* Input mode for PA[11:8]*/
      /* Clear row and column bits in PUPDR */
      GPIOA->PUPDR &= ~(0x00000FF0); /* Clear for bits PA[11:2] */
      GPIOA->PUPDR |= 0x00550000; /* Pull PA[11:8] high */
      /* Drive rows low */
      GPIOA->ODR &= ~(0x0000003C); /* Drive PA[5:2] low */
      /* Short delay for signal propagation */
      for (i = 0; i < 640; i++);
      /* Read pressed column */
      button_indices[1] = (GPIOA->IDR & (0x00000F00)) >> 8; /* Mask to read columns only (PA[11:8]) */
      /* Decode row */
      switch(button_indices[0])
      {
            case 0x07: /* Row 1*/
                  button_indices[0] = 3;
                  break;
            case 0x0B: /* Row 2 */
                  button_indices[0] = 2;
                  break;
            case 0x0D: /* Row 3 */
                  button_indices[0] = 1;
                  break;
            case 0x0E: /* Row 4 */
                  button_indices[0] = 0;
                  break;
      }
      /* Decode column */
      switch(button_indices[1])
      {
            case 0x07: /* Col 1 */
                  button_indices[1] = 3;
                  break;
            case 0x0B: /* Col 2 */
                  button_indices[1] = 2;
                  break;
            case 0x0D: /* Col 3 */
                  button_indices[1] = 1;
                  break;
            case 0x0E: /* Col 4 */
                  button_indices[1] = 0;
                  break;
      }
      /* Restore previous GPIOA->MODER state */
      GPIOA->MODER &= ~(0x00000FF0); /* Clear PA[5:2] mode bits */
      GPIOA->MODER |=   (0x00550000); /* General purpose output mode for PA[11:8]*/

      /* Clear PUPDR */
      GPIOA->PUPDR &= ~(0x00FF0000); /* Clear for bits PA[11:8] */

      /* Pull rows high */
      GPIOA->PUPDR |= 0x00000550; /* Pull PA[5:2] high */
}

void button_to_value()
{
      /* Set button_val to 0 */
      button_val = 0;

      /* Lookup table for each button value. Indexed as lookup_table[row][col]. */
      char lookup_table [4][4] = {
            {0x01, 0x02, 0x03, 0x0A}, /* Row 1 */
            {0x04, 0x05, 0x06, 0x0B}, /* Row 2 */
            {0x07, 0x08, 0x09, 0x0C}, /* Row 3 */
            {0x0E, 0x00, 0x0F, 0x0D}  /* Row 4 */
      };

      uint16_t ccr_table [11] = {
            0, 3999, 7999, 11999, 15999, 19999, /* Buttons 0 -5 */
            23999, 27999, 31999, 35999, 39999 /* Buttons 6 - A */
      };

      /* Look up value to display in keypad_table */
      button_val = lookup_table[button_indices[0]][button_indices[1]]; /* index as (row | col) */

      if(button_val <= 0x0A)
      {
            /* set new CCR value in TIM2 */

    	  	if(mode==0){
    	  		TIM2->CCR1 = button_val * (TIM2->ARR + 1)/10; /* set CCR value to pressed key */
    	  		pwm_value = button_val * (TIM2->ARR + 1)/10;
    	  	}
    	  	if(mode==1){
    	  		if(button_val <= 0x04){
    	  			TIM2->CCR1 = button_val * (TIM2->ARR + 1)/4;
    	  			pwm_value = button_val * (TIM2->ARR + 1)/4;
    	  			desired_adc_speed = button_val * 750;
    	  			max_overshoot = desired_adc_speed * 0.01;
    	  	}
    	  	}

      }
      if(button_val == 0x0D){
    	  mode = !mode;
      }
}

void display()
{
    /* Write counter to PB[6:3] */
    GPIOB->ODR = counter << 3;
}

void EXTI0_IRQHandler()
{
      get_pressed_button(); /* Get pressed button indices and save to button_indices[] */
      button_to_value(); /* Convert button indices into a usable value */

      button_pressed_indicator = 1; /* Signify pressing a button */

      if (button_val == 0x0E) { //if * is pressed
            TIM6->CR1 ^= TIM_CR1_CEN; //toggle clock
        }
        else if (button_val == 0x0F) { //if # is pressed
            direction = !direction;
        }//do nothing if other button is pressed

      /* Pull rows high */
      GPIOA->PUPDR &= ~(0x00000FF0); /* Clear bits for PA[5:2] */
      GPIOA->PUPDR |= 0x00000550; /* Pull PA[5:2] high */


      /* Clear pending bits */

      EXTI->PR1 = EXTI_PR1_PIF0; /* Clear pending bit for EXTI0 in EXTI->PR1 Register */
      NVIC_ClearPendingIRQ(EXTI0_IRQn); /* Clear pending bit for EXTI0 in NVIC */
}

void TIM6_IRQHandler()
{
   //count to 9 and roll over to 0

	   if (direction == 0) {
	      if(counter == 9) counter = -1;
	      counter++;  // Increment if direction is 0
	  } else {
	      if(counter == 0) counter = 10;
	      counter--;  // Decrement if direction is 1
	  }

	//feedback
	if(mode==1){
		if (adc_speed > (desired_adc_speed + max_overshoot)){
			TIM2->CCR1 -= 2;
			pwm_value -= 2;
		}
		   if (adc_speed < (desired_adc_speed - max_overshoot)){
			TIM2->CCR1 += 2;
			pwm_value += 2;
		}
	}
   display();

   TIM6->SR &= ~TIM_SR_UIF; //clear TIM flag
   NVIC_ClearPendingIRQ(TIM6_IRQn); //clear NVIC flag
}

int main(void) {
    GPIO_setup(); /* Configure GPIO pins */
    interrupts_setup(); /* Configure Interrupts */
    timer_setup(); /* set up TIM2 for PWM */
    adc_setup();
    while (1) {
    	adc_speed = ADC1->DR;
    }
}
