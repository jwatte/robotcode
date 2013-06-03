
#if !defined (Ada1306_h)
#define Ada1306_h

#define I2C_WRITE 0x78
#define I2C_READ 0x79

#define WIDTH 22    //  actually goes across end
#define HEIGHT 4

void LCD_Setup(void);
void LCD_Clear(void);
void LCD_Flush(void);

void LCD_DrawChar(unsigned char ch, unsigned char x, unsigned char y);
void LCD_DrawString(char const *str, unsigned char x, unsigned char y, unsigned char wrap);

#endif  //  Ada1306_h
