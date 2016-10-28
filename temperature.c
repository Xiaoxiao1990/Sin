#include "temperature.h"
#include "stm8s.h"
#include "stm8s003f3.h"
#include "stm8s_bitsdefine.h"
#include "led_display.h"
#include "relay.h"
#include "keyscan.h"

#define DAT_I  		PC_IDR_4
#define DAT_O	 		PC_ODR_4
#define OUT				0
#define IN				1
#define SET_DAT(C)		{if(C){PC_DDR &= 0xef;}else{PC_DDR |= 0x10;}}
//=============IO define ==============

_Bool	isADGet;
_Bool isStartToRead;
void delay(uint tus)
{
	uint j;//fCPU = 1Mhz 1.5uS per syntax
	for(j = tus;j > 0;j--);
}

void _Temp_Initial(void)
{
//	PC_DDR |= 0b00010000;//PC4
//	PC_CR1 |= 0b00010000;
	DAT_O = 1;//initial Data line
}
_Bool call_DS18B20(void)
{
	_Bool exist;
	DAT_O = 0;
	delay(200);//hold 480uS~960uS  ;about 275uS
	_asm("sim");
	DAT_O = 1;
	SET_DAT(IN);//release Data line
	delay(60);//wait DS18B20 for 15uS~60uS
	exist = (DAT_I^1);//read presence pulse and return
	_asm("rim");
	delay(40);//DS18B20 hold presence signal 60uS~240uS
	SET_DAT(OUT);
	DAT_O = 1;
	return exist;
}
void write_DS18B20(uchar DAT)
{
	uchar i;
	_asm("sim");
	for(i = 0;i < 8;i++)
	{
		DAT_O = 0;
		delay(5);//about 17uS
		DAT_O = DAT&0x01;//LSB in front
		delay(40);//hold data more than 60uS
		DAT >>= 1;
		DAT_O = 1;//Must be push up the data line
		delay(1);
	}
	_asm("rim");
	DAT_O = 1;
}
uchar read_DS18B20(void)
{
	uchar DAT = 0,i;
	_asm("sim");
	for(i = 0;i < 8;i++)
	{
		DAT >>= 1;
		DAT_O = 1;
		delay(2);//about 3uS
		DAT_O = 0;
		delay(4);//about 6uS
		DAT_O = 1;
		delay(3);//about 4.5uS
		SET_DAT(IN);
		if(DAT_I)
		{
			DAT |= 0x80;
		}
		delay(20);//about 30uS
		SET_DAT(OUT);
	}
	_asm("rim");
	DAT_O = 1;
	return DAT;

}

/************ CRC Check *****************/
uchar cal_crc(uchar data,uchar crcValue)
{
	_Bool bXorValue;
	uchar tempValue;
	uchar i;
	for(i = 0;i < 8;i++)
	{
		bXorValue = (data ^ crcValue)&0x01;
		if(bXorValue)
		{
			tempValue = crcValue ^ 0x18;
		}
		else
		{
			tempValue = crcValue;
		}
		tempValue >>= 1;
		if(bXorValue)
		{
			crcValue = tempValue | 0x80;
		}
		else
		{
			crcValue = tempValue;
		}
		data >>= 1;
	}
	return crcValue;
}

uchar CRC_Check(uchar *dataBuf,uchar dataLength)
{
	uchar crcValue;
	uchar i;
	crcValue = 0;
	for(i = 0; i < dataLength;i++)
	{
		
		crcValue = cal_crc(*(dataBuf+i),crcValue);
	}
	/*
	if(crcValue == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}*/
	return crcValue;
}
/*************** End ********************/
void _temperature(void)
{
	static uchar status = 0,times = 0;
	static uint time_CNT = 0;
	static uchar temp_L,temp_H;
	float temp_temp;
	static uchar RAM[9];//DS18B20 RAM
	uchar i,crc_calculation,crc;
	switch(status)
	{
		case 0:
		{
			if(call_DS18B20())//if exist
			{
				status++;
				if(Error == 1)Error = 0;
			}
			else
			{//Error code:E1
				Error = 1;
				isUpdateDisplay = 1;
			}
		}
		break;
		case 1:
		{
			call_DS18B20();
			write_DS18B20(0xCC);//Skip ROM
			write_DS18B20(0x44);//convert temperature
			status++;
		}
		break;
		case 2:
		{
			if(++time_CNT > 700)//per second
			{
				time_CNT = 0;
				status++;
			}
		}
		break;
		case 3:
		{
			call_DS18B20();
			write_DS18B20(0xCC);
			write_DS18B20(0xBE);
			for(i = 0;i < 9;i++)RAM[i] = read_DS18B20();
			status++;
		}
		break;
		case 4:
		{
			crc = RAM[8];
			crc_calculation = CRC_Check(RAM,8);
			if(crc == crc_calculation)
			{
				status++;
				temp_L = RAM[0];
				temp_H = RAM[1];
			}
			else
			{
				status = 0;
			}
		}break;
		case 5:
		{
			currentTempvalue = temp_H<<8;
			currentTempvalue |= temp_L;
			temp_temp = currentTempvalue * 0.625;
			currentTempvalue = temp_temp+0.5;
			isUpdateDisplay = 1;
			status = 0;
		}
		break;
		default:;
	}
	if((currentTempvalue >= 1250)||(currentTempvalue <= -550))
	{
		if(Error == 0)
		{
			if(++times > 3)
			{
				times = 0;
				Error = 2;
			}
		}
	}
	else
	{
		if(Error == 2)Error = 0;
	}
}
