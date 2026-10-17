#ifndef _OLED_H_
#define _OLED_H_

#define OLED_I2C_ADDRESS    0x78

#define OLED_CMD        0x00
#define OLED_DATA       0x01


void OLED_Init(void);

void OLED_Display_On(void);
void OLED_Display_Off(void);

void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_Refresh_Gram(void);
void OLED_Clear(void);

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t dot);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr);
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *str);
void OLED_printf(uint8_t row, uint8_t col, const char *fmt,...);

void OLED_LOGO(void);

extern uint8_t OLED_Disconnect;
extern uint8_t OLED_GRAM[128][8];

#endif
