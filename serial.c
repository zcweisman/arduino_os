#include <avr/io.h>
#include <stdio.h>

#define ESC  27
#define BYTE 8

/*
 * Initialize the serial port.
 */
void serial_init() {
   uint16_t baud_setting;

   UCSR0A = _BV(U2X0);
   baud_setting = 16; //115200 baud

   // assign the baud_setting
   UBRR0H = baud_setting >> 8;
   UBRR0L = baud_setting;

   // enable transmit and receive
   UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
}

/*
 * Return 1 if a character is available else return 0.
 */
uint8_t byte_available() {
   return (UCSR0A & (1 << RXC0)) ? 1 : 0;
}

/*
 * Unbuffered read
 * Return 255 if no character is available otherwise return available character.
 */
uint8_t read_byte() {
   if (UCSR0A & (1 << RXC0)) return UDR0;
   return 255;
}

/*
 * Unbuffered write
 *
 * b byte to write.
 */
uint8_t write_byte(uint8_t b) {
   //loop until the send buffer is empty
   while (((1 << UDRIE0) & UCSR0B) || !(UCSR0A & (1 << UDRE0))) {}

   //write out the byte
   UDR0 = b;
   return 1;
}

//Print a string
void print_string(char* s)
{
   while(*s != '\0')
      write_byte(*s++);
   return;
}

//Print an 8-bit or 16-bit unsigned integer
void print_int(uint16_t i) 
{
   char str[10];
   sprintf(str, "%u", i);
   print_string(str);
}

//Print a 32-bit unsigned integer
void print_int32(uint32_t i) {
   char buf[32] = {0};
   int ndx = 31;

   write_byte(0x1B);
   write_byte('[');
   write_byte('1');
   write_byte('i');

   for (; i && ndx; --ndx, i /= 10)
      buf[ndx] = "0123456789"[i % 10];

   while (ndx <= 31) write_byte(buf[ndx++]);
   return;
}

//Print an 8-bit or 16-bit unsigned integer in hex format
void print_hex(uint16_t i)
{
   char str[10];
   sprintf(str, "%#x", i);
   print_string(str);
}

//Print a 32-bit unsigned integer in hex format
void print_hex32(uint32_t i) {
   char buf[32] = {0};
   int ndx = 31;

   write_byte(0x1B);
   write_byte('[');
   write_byte('1');
   write_byte('i');
   write_byte('0');
   write_byte('x');

   for (; i && ndx; --ndx, i /= 16)
      buf[ndx] = "0123456789abcdef"[i % 16];

   while (ndx <= 31) write_byte(buf[ndx++]);

   return;
}

//Set the cursor position
void set_cursor(uint8_t row, uint8_t col)
{
   //<ESC>[{ROW};{COLUMN}H
   write_byte(ESC);
   write_byte('[');
   print_int(row);
   write_byte(';');
   print_int(col);
   write_byte('H');
}

//Set the foreground color
void set_color(uint8_t color)
{
   write_byte(ESC);
   write_byte('[');
   print_int(color);
   write_byte('m');
}

//Erase all text on the screen
void clear_screen(void)
{
   write_byte(ESC);
   print_string("[2J");
   set_cursor(0, 0);
   return;
} 


