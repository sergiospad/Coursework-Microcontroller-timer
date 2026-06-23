#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>

const int LEDD[] = {0b11111100, 0b00011000, 0b01101100,  0b00111100, 0b10011000, 0b10110100, 0b11110100, 0b00011100, 0b11111100, 0b10111100};
const int LEDB[] = {0b00000000, 0b00000000, 0b00000001, 0b00000001, 0b00000001, 0b00000001, 0b00000001, 0b00000000, 0b00000001, 0b00000001};
int digits[4];
unsigned int counter = 0;
char multiplier = 1;
int coeff = 10;
int millis = 0;
char less_10_min = 0;

struct time {
	char minutes;
	char seconds;
	char milliseconds;
};

struct time convert_millis_to_time(unsigned int);

struct time convert_millis_to_time(unsigned int num){
	struct time result;
	result.milliseconds = num % 10;
	num /= 10;
	result.seconds = num % 60;
	result.minutes = num / 60;
	return result;
}

void EEPROM_write(unsigned int uiAddress, unsigned char ucData){
	while(EECR & (1<<EEPE))
	;
	EEARH = ((uiAddress & 0xF0) << 2);
	EEARL = uiAddress & 0x0F;
	EEDR = ucData;
	
	EECR |= 1<< EEMPE;
	EECR |= 1<<EEPE;
	
}

unsigned char EEPROM_read(unsigned int uiAddress){
	while(EECR & (1<<EEPE))
	;
	EEARH = ((uiAddress & 0xF0) << 2);
	EEARL = uiAddress & 0x0F;
	
	EECR |= (1<<EERE);
	return EEDR;
}

ISR(TIMER0_OVF_vect){
	for (int i = 0; i<4; i++)
	{
		PORTD &= 0x03;
		PORTB=~(1<<(i+2))&0b00111100;
		PORTD |= LEDD[digits[i]];
		PORTB |= LEDB[digits[i]];
		
		if((less_10_min && ((i == 2)||(i==0))) || (!less_10_min && i == 1))
		PORTB |= (1<<1);
	}
}

void return_capacity(unsigned int num){
	struct time t = convert_millis_to_time(num);
	less_10_min = (t.minutes < 10);
	if(less_10_min) {
		digits[0] = t.minutes;
		digits[1] = t.seconds / 10;
		digits[2] = t.seconds % 10;
		digits[3]= t.milliseconds;
	}
	else{
		digits[0] = t.minutes / 10;
		digits[1] = t.minutes % 10;
		digits[2] = t.seconds / 10;
		digits[3]= t.seconds % 10;
	}
}

ISR(TIMER2_OVF_vect){
	counter++;
	if(multiplier == -1 && millis == 0)
	return;
	if (coeff > millis && multiplier == -1){
		millis = 0;
		return_capacity(millis);
		return;
	}
	if(counter != 1 && counter % 200 != 0) return;
	
	if(counter == 1)
	coeff = 10;
	else if (counter == 1000)
	coeff = 50;
	else if(counter == 1600)
	coeff = 100;
	else if(counter == 2400)
	coeff = 600;
	else if(counter == 3200)
	coeff = 3000;
	else if(counter == 3800)
	coeff = 6000;
	
	millis = (millis + coeff*multiplier);
	if(millis >= 54000)
	millis = 0;

	return_capacity(millis);

}
void return_number(){
	unsigned char flag = 0;
	unsigned char r = 0;
	cli();
	flag = EEPROM_read(0);
	sei();
	for(unsigned int j = 1; j<5; j++){
		cli();
		r = EEPROM_read(j);
		sei();
		digits[j-1] = r;
	}
	
	millis = 0;
	if(flag == 0x0A){
		millis += digits[3];
		millis += digits[2]*10;
		millis += digits[1]*100;
		millis += digits[0]*600;
	}
	else if (flag == 0x0B){
		millis += digits[3] * 10;
		millis += digits[2] * 100;
		millis += digits[1] * 600;
		millis += digits[0] * 6000;
	}
	return_capacity(millis);
	
}

void write_digit(void)
{
	unsigned char r = 0;
	if(less_10_min) r = 0x0A;
	else r = 0x0B;
	cli();
	EEPROM_write(0, r);
	sei();
	for(int unsigned j = 1; j<5; j++){
		r = digits[j-1];
		cli();
		EEPROM_write(j, r);
		sei();
	}
}


ISR(TIMER1_COMPA_vect){
	if(!millis){
		PORTC|=(1<<3);
		TCCR1B = 0x00;
		return;
	}
	return_capacity(--millis);
}

void button_handler(char port, char new_multipl){
	if(~PINC&(1<<port)){
		multiplier = new_multipl;
		return_capacity(millis);
		TCCR2B = 0b00000011;
		while(~PINC&(1<<port))
		;
		counter = 0;
		TCCR2B = 0x00;
		write_digit();
	}
}

void init(){
	DDRD = ~0x03;
	DDRB = 0b00111111;
	DDRC = 0b00001000;
	PORTC = 0x07;
	
	sei();
	TCCR0B=0b00000010;
	TCNT0 = 0;
	TIMSK0|=(1<<TOIE0);
	
	//TCCR1B = 0b00001011;
	TIMSK1|=(1<<OCIE1A);
	TCNT1 = 0;
	OCR1A =6250;

	//TCCR2B = 0b00000011;
	TCNT2 = 0;
	TIMSK2|=(1<<TOIE2);
	
	return_number();
}

int main(void)
{
	init();
	while (1)
	{
		button_handler(0, 1);
		button_handler(1, -1);
		if(~PINC&(1<<2)){
			if(TCCR1B == 0x00){
				if(millis>0){
					PORTC&=~(1<<3);
					TCCR1B = 0b00001011;
				}
			}
			else
			TCCR1B = 0x00;
			while(~PINC&(1<<2))
			;
		}
	}
}


