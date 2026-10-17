#ifndef _LED_H
#define _LED_H

#define RED_COLOR 10,0,0
#define GREEN_COLOR 0,10,0
#define BLUE_COLOR 0,0,50
#define YELLOW_COLOR 10,10,0
#define WHITE_COLOR 10,10,10

#define LED_OFF	0
#define LED_ON	1

void LED_init(void);
void LED_SetColor(uint8_t red, uint8_t green, uint8_t blue);
void LED_on(void);
void LED_off(void);
void LED_blink(uint8_t frequency);

extern uint8_t LED_state;

#endif
