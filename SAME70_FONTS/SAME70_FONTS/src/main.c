/*
 * main.c
 *
 * Created: 05/03/2019 18:00:58
 *  Author: eduardo
 */ 

#include "asf.h"
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"
#include "math.h"


#define YEAR        2019
#define MONTH		4
#define DAY         8
#define WEEK        15
#define HOUR        16
#define MINUTE      18
#define SECOND      0


#define BUT2_PIO_ID				ID_PIOC
#define BUT2_PIO				PIOC
#define BUT2_PIO_IDX			31
#define BUT2_PIO_IDX_MASK		(1 << BUT2_PIO_IDX)
#define BUT2_DEBOUNCING_VALUE   79

#define BUT3_PIO				PIOA
#define BUT3_PIO_ID				ID_PIOA
#define BUT3_PIO_IDX			19
#define BUT3_PIO_IDX_MASK		(1 << BUT3_PIO_IDX)
#define BUT3_DEBOUNCING_VALUE   79

struct ili9488_opt_t g_ili9488_display_opt;

volatile uint16_t counter_total = 0;
volatile uint16_t counter_temp = 0;

volatile uint8_t flag_but2 = 0;
volatile uint8_t flag_but3 = 0;
volatile uint8_t flag_rtt = 1;
volatile uint8_t flag_rtc = 1;

static float raio = 0.325;

volatile uint32_t hour;
volatile uint32_t minute;
volatile uint32_t second;

volatile float distancia = 0;


void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);

	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
		flag_rtc = 1;
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	}
	
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
	
}

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(RTC, YEAR, MONTH, DAY, WEEK);
	rtc_set_time(RTC, HOUR, MINUTE, SECOND);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(RTC,  RTC_IER_SECEN);

}

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		flag_rtt = 1;
	}
}

static void Button2_Handler(uint32_t id, uint32_t mask)
{
	if(flag_but2){
		flag_but2 = 0;
	}
	else{
		flag_but2 = 1;
		counter_total = 0;
		rtc_set_time(RTC,HOUR,MINUTE,SECOND);
		flag_rtt = 1;
	}
}

void Button3_Handler(uint32_t id, uint32_t mask)
{
	counter_temp+=1;
	counter_total+=1;
}

void BUT_init(void){
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pio_set_input(BUT2_PIO, BUT2_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	
	pmc_enable_periph_clk(BUT3_PIO_ID);
	pio_set_input(BUT3_PIO, BUT3_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);

	pio_enable_interrupt(BUT2_PIO, BUT2_PIO_IDX_MASK);
	pio_enable_interrupt(BUT3_PIO, BUT3_PIO_IDX_MASK);
	
	pio_handler_set(BUT2_PIO, BUT2_PIO_ID, BUT2_PIO_IDX_MASK, PIO_IT_FALL_EDGE, Button2_Handler);
	pio_handler_set(BUT3_PIO, BUT3_PIO_ID, BUT3_PIO_IDX_MASK, PIO_IT_FALL_EDGE, Button3_Handler);
	
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 1);
	
	NVIC_EnableIRQ(BUT3_PIO_ID);
	NVIC_SetPriority(BUT3_PIO_ID, 1);
};

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));
	
	rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}


void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
	
}


void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}	
}


int main(void) {
	WDT->WDT_MR = WDT_MR_WDDIS;
	board_init();
	sysclk_init();
	BUT_init();
	RTC_init();
	configure_lcd();
	counter_total = 0;
	while(1) {
		if(flag_but2){
			if (flag_rtt){
				uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
				uint32_t irqRTTvalue  = 8;
				RTT_init(pllPreScale, irqRTTvalue);
				float vel_ang = 2*M_PI*counter_temp/4;
				float vel_lin = vel_ang * raio;
				distancia = 2*M_PI*raio*counter_total;
				char b[32];
				char b2[32];
				sprintf(b,"V: %.2f km/h",vel_lin *3.6);
				sprintf(b2,"D: %.2f m",distancia);
				ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, 150);
				font_draw_text(&calibri_36, b, 50, 50, 1);
				font_draw_text(&calibri_36, b2, 50, 100, 1);
				counter_temp = 0;
				flag_rtt = 0;
			}
		
			if (flag_rtc){
				rtc_get_time(RTC,&hour,&minute,&second);
				char b3[32];
				sprintf(b3,"%02d : %02d : %02d",hour - HOUR,minute - MINUTE,second - SECOND);
				font_draw_text(&calibri_36, b3, 50, 150, 1);
				flag_rtc = 0;
			}
		}
		else{
			pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
		}
	}
}