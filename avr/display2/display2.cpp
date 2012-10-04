/**********************************************
Pay an attention!

This code is designed for Arduino Mega board.

**********************************************/

#include <libavr.h>
#include <pins_avr.h>

/**********************************************
Define zone
**********************************************/
#define RS (0x0 | 0)
#define WR (0x0 | 1)
#define CS (0x0 | 2)
#define RST (0x0 | 3)


#define X_CONST 240
#define Y_CONST 320

#define WINDOW_XADDR_START	0x0050 // Horizontal Start Address Set
#define WINDOW_XADDR_END	0x0051 // Horizontal End Address Set
#define WINDOW_YADDR_START	0x0052 // Vertical Start Address Set
#define WINDOW_YADDR_END	0x0053 // Vertical End Address Set
#define GRAM_XADDR		    0x0020 // GRAM Horizontal Address Set
#define GRAM_YADDR		    0x0021 // GRAM Vertical Address Set
#define GRAMWR 			    0x0022 // memory write

/* LCD color */
#define White          0xFFFF
#define Black          0x0000
#define Blue           0x001F
#define Blue2          0x051F
#define Red            0xF800
#define Magenta        0xF81F
#define Green          0x07E0
#define Cyan           0x7FFF
#define Yellow         0xFFE0


/**********************************************
Standard C functions zone
**********************************************/

void Write_Command(unsigned int c)
{
    digitalWrite(RS,LOW);//LCD_RS=0; 
	digitalWrite(CS,LOW);//LCD_CS =0;	 
	PORTD = c>>8; //LCD_DataPortH=DH>>8;
	digitalWrite(WR,LOW);//LCD_WR=0;
	digitalWrite(WR,HIGH);//LCD_WR=1;	
	PORTD = c;//LCD_DataPortH=DH;		
	digitalWrite(WR,LOW);//LCD_WR=0;
	digitalWrite(WR,HIGH);//LCD_WR=1;
	digitalWrite(CS,HIGH);//LCD_CS =0;
}


void Write_Data(unsigned int c)
{
    digitalWrite(RS,HIGH);//LCD_RS=0;
	digitalWrite(CS,LOW);//LCD_CS =0;	 
	PORTD = c>>8; //LCD_DataPortH=DH>>8;
	digitalWrite(WR,LOW);//LCD_WR=0;
	digitalWrite(WR,HIGH);//LCD_WR=1;	
	PORTD = c;//LCD_DataPortH=DH;		
	digitalWrite(WR,LOW);//LCD_WR=0;
	digitalWrite(WR,HIGH);//LCD_WR=1;
	digitalWrite(CS,HIGH);//LCD_CS =0;
}

void Write_Command_Data(unsigned int cmd,unsigned int dat)
{
	Write_Command(cmd);
	Write_Data(dat);
}

void Lcd_Init()
{
	pinMode(RS,OUTPUT);
	pinMode(WR,OUTPUT);
	pinMode(CS,OUTPUT);
	pinMode(RST,OUTPUT);
	
	DDRD = 0xFF;
  
	digitalWrite(RST,HIGH);
    delay(1);	
	digitalWrite(RST,LOW);
	delay(1);
	
	digitalWrite(CS,HIGH);
	digitalWrite(WR,HIGH);
	digitalWrite(RST,HIGH);
	delay(20);

	
	Write_Command_Data(0x0011,0x2004);		
    Write_Command_Data(0x0013,0xCC00);		
    Write_Command_Data(0x0015,0x2600);	
	Write_Command_Data(0x0014,0x252A);	
//	Write_Command_Data(0x14,0x002A);		
	Write_Command_Data(0x0012,0x0033);		
	Write_Command_Data(0x0013,0xCC04);		
	//delayms(1); 
	Write_Command_Data(0x0013,0xCC06);		
	//delayms(1); 
	Write_Command_Data(0x0013,0xCC4F);		
	//delayms(1); 
	Write_Command_Data(0x0013,0x674F);
	Write_Command_Data(0x0011,0x2003);
	//delayms(1); 	
	Write_Command_Data(0x0030,0x2609);		
	Write_Command_Data(0x0031,0x242C);		
	Write_Command_Data(0x0032,0x1F23);		
	Write_Command_Data(0x0033,0x2425);		
	Write_Command_Data(0x0034,0x2226);		
	Write_Command_Data(0x0035,0x2523);		
	Write_Command_Data(0x0036,0x1C1A);		
	Write_Command_Data(0x0037,0x131D);		
	Write_Command_Data(0x0038,0x0B11);		
	Write_Command_Data(0x0039,0x1210);		
	Write_Command_Data(0x003A,0x1315);		
	Write_Command_Data(0x003B,0x3619);		
	Write_Command_Data(0x003C,0x0D00);		
	Write_Command_Data(0x003D,0x000D);		
	Write_Command_Data(0x0016,0x0007);		
	Write_Command_Data(0x0002,0x0013);		
	Write_Command_Data(0x0003,0x0003);		
	Write_Command_Data(0x0001,0x0127);		
	//delayms(1); 
	Write_Command_Data(0x0008,0x0303);		
	Write_Command_Data(0x000A,0x000B);		
	Write_Command_Data(0x000B,0x0003);   
	Write_Command_Data(0x000C,0x0000);   
	Write_Command_Data(0x0041,0x0000);    
	Write_Command_Data(0x0050,0x0000);   
	Write_Command_Data(0x0060,0x0005);    
    Write_Command_Data(0x0070,0x000B);    
	Write_Command_Data(0x0071,0x0000);    
	Write_Command_Data(0x0078,0x0000);    
	Write_Command_Data(0x007A,0x0000);   
	Write_Command_Data(0x0079,0x0007);		
	Write_Command_Data(0x0007,0x0051);   
	//delayms(1); 	
	Write_Command_Data(0x0007,0x0053);		
	Write_Command_Data(0x0079,0x0000);

	Write_Command(0x0022);
	
}
void SetXY(unsigned int x0,unsigned int x1,unsigned int y0,unsigned int y1)
{
  Write_Command_Data(0x0046,(x1 << 8)| x0);
  //Write_Command_Data(0x0047,x1);
  Write_Command_Data(0x0047,y1);
  Write_Command_Data(0x0048,y0);
  Write_Command_Data(0x0020,x0);
  Write_Command_Data(0x0021,y0);
  Write_Command (0x0022);//LCD_WriteCMD(GRAMWR);
}
void Pant(unsigned int color)
{
	int i,j;
	SetXY(0,239,0,319);

    for(i=0;i<320;i++)
	 {
	  for (j=0;j<240;j++)
	   	{
         Write_Data(color);
	    }

	  }		
}
void LCD_clear()
{
    unsigned int i,j;
	SetXY(0,239,0,319);
	for(i=0;i<X_CONST;i++)
	{
	    for(j=0;j<Y_CONST;j++)
		{    
          	Write_Data(0x0000);
		}
	}
}


/**********************************************
Arduino functions zone
**********************************************/
void setup()
{ 
    setup_timers(F_CPU);
  
  Lcd_Init();
	Pant(0xf800);
	Pant(0x07e0);
	Pant(0x001f);
	Pant(0xffff);
	Pant(0x0000);
}


