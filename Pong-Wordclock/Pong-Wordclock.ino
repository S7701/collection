#include "font.h"

#define DATA_SET    PORTB |=  (1<<4)
#define DATA_CLR    PORTB &= ~(1<<4)

#define CLK_SET     PORTB |=  (1<<3)
#define CLK_CLR     PORTB &= ~(1<<3)

#define STROBE_SET  PORTB |=  (1<<2)
#define STROBE_CLR  PORTB &= ~(1<<2)

#define BUTTON      (!(PIND & (1<<2)))

//           11
// 012345678901
//
// ES..IST.ZEHN  0x001
// .FUNFZWANZIG  0x002
// .DREIVIERTEL  0x004
// NACHVOR.HALB  0x008
// ............  0x010
// FUNF..ZWEINS  0x020
// SIEBEN.SECHS  0x040
// ZEHNEUN.VIER  0x080
// DREIELF.ACHT  0x100
// .ZWOLF...UHR  0x200

#define CLEAR_ALL frame[0]=0; frame[1]=0; frame[2]=0; frame[3]=0; frame[4]=0; frame[5]=0; frame[6]=0; frame[7]=0; frame[8]=0; frame[9]=0; frame[10]=0; frame[11]=0

#define FRAME_WIDTH   12
#define BUFFER_WIDTH 256

#define MIN_SPEED 1
#define MAX_SPEED 10
#define SPEED_INC 1
#define DEFAULT_SPEED 5

const uint8_t digits[] =
{
  0x7C, 0x82, 0x82, 0x7C, // 0
  0x08, 0x04, 0xFE, 0x00, // 1
  0xC4, 0xA2, 0x92, 0x8C, // 2
  0x82, 0x92, 0x92, 0x6C, // 3
  0x1E, 0x10, 0xF8, 0x10, // 4
  0x9E, 0x92, 0x92, 0x62, // 5
  0x78, 0x94, 0x92, 0x60, // 6
  0x02, 0xF2, 0x0A, 0x06, // 7
  0x6C, 0x92, 0x92, 0x6C, // 8
  0x8C, 0x92, 0x92, 0x7C  // 9
};

uint16_t buffer[BUFFER_WIDTH];
volatile uint16_t* frame = buffer;
volatile uint8_t frame_index = 0;
volatile uint8_t hour = 12, minute = 0, second = 0;
volatile bool hour_flag = false, min_flag = false, sec_flag = false, sync_flag = false;

void setup() {
  DDRC = 0x0F;
  DDRD = 0xF0;
  DDRB = 0x03 + (1<<2) + (1<<3) + (1<<4);
  PORTD |= (1<<2);    // für Pullup von Button
//  SFIOR &= (1<<PUD);

  // Timer 2 mit externem 32kHz Quarz betreiben
  ASSR |= (1<<AS2);
  TCCR2 = (1<<CS22) + (1<<CS20);      // /128 für 1Hz Int

  // Timer 1 für LED INT mit ca. 1,2kHz
  TCCR1A = 0;
  TCCR1B = (1<<WGM12) + (1<<CS10);
  OCR1A = 6667;

  // Timer Interrupts

  TIMSK  = (1<<OCIE1A) + (1<<TOIE2);   // set interrupt mask

  for (uint8_t i = 0; i < FRAME_WIDTH; ++i) frame[i] = 0x3FF;

  sei();  // Interrupt ein
}

void loop() {
  static uint8_t button_released_cnt = 0;
  static uint8_t button_pressed_cnt = 0;
  static uint8_t mode = 0;
  static uint8_t speed = DEFAULT_SPEED;
  static bool draw_flag = true;

  if (min_flag) { // neue Minute
    min_flag = false;
    draw_flag = true;
  }

  if (sync_flag) { // every 10 ms
    sync_flag = false;
    bool button_pressed = BUTTON != 0;
    if (button_pressed) {
      // button pressed
      ++button_pressed_cnt;
      button_released_cnt = 0;
      if (button_pressed_cnt == 300) {
        // buffer pressed for 3 sec
        button_pressed_cnt = 100; // see below
        ++mode;
        if (mode > 2) mode = 0;
        draw_flag = true;
      }
    } else {
      // button released
      ++button_released_cnt;
      if (button_released_cnt == 3) {
        // debounce button
        if (button_pressed_cnt > 3 && button_pressed_cnt < 100) {
          // button pressed more than 20 ms but less than 1 sec
          switch (mode) {
            case 0:
              speed + SPEED_INC;
              if (speed > MAX_SPEED) speed = MIN_SPEED;
              break;
            case 1:
              minute = (minute < 59) ? minute + 1 : 0;
              draw_flag = true;
              break;
            case 2:
              hour = (hour < 12) ? hour + 1 : 1;
              draw_flag = true;
              break;
          }
        }
        button_pressed_cnt = 0; // reset button pressed counter
      } else if (button_released_cnt > 1000) {
        // button released more than 10 sec
        mode = 0;
        draw_flag = true;
      }
    }
    if (draw_flag) {
      draw_flag = false;
      draw_time(mode, button_pressed);
    }
  }
}

void draw_time(uint8_t mode, bool button_pressed) {
  uint8_t i, h, m;

  if (mode == 2) {
    // setup minute
    i = (hour / 10) * 4;
    buffer[0] = digits[i];
    buffer[1] = digits[i+1];
    buffer[2] = digits[i+2];
    buffer[3] = digits[i+3];
    buffer[4] = 0;
    i = (hour % 10) * 4;
    buffer[5] = digits[i];
    buffer[6] = digits[i+1];
    buffer[7] = digits[i+2];
    buffer[8] = digits[i+3];
    buffer[9] = 0;

    if (second & 1) { buffer[10] = 0x44; buffer[11] = 0;    }
    else            { buffer[10] = 0;    buffer[11] = 0x44; }
  } else if (mode == 1) {
    // setup hour
    if (second & 1) { buffer[0] = 0x44;  buffer[1] = 0;     }
    else            { buffer[0] = 0;     buffer[1] = 0x44;  }
    buffer[2] = 0;
    i = (minute / 10) * 4;
    buffer[3] = digits[i];
    buffer[4] = digits[i+1];
    buffer[5] = digits[i+2];
    buffer[6] = digits[i+3];
    buffer[7] = 0;
    i = (minute % 10) * 4;
    buffer[8]  = digits[i];
    buffer[9]  = digits[i+1];
    buffer[10] = digits[i+2];
    buffer[11] = digits[i+3];
  } else {
    // draw time
    CLEAR_ALL;

    h = hour;
    m = minute;

    frame[0] = h;
    frame[1] = m;
    frame[2] = second;
    if (button_pressed) frame[3] |= 0x001;

    if (     m <  5) {}
    else if (m < 10) {}
    else if (m < 15) {}
    else if (m < 20) {}
    else if (m < 25) {}
    else if (m < 30) {}
    else if (m < 35) {}
    else if (m < 40) {}
    else if (m < 45) {}
    else if (m < 50) {}
    else if (m < 55) {}
    else if (m < 60) {}

    if (     h ==  0) {}
    else if (h ==  1) {}
    else if (h ==  2) {}
    else if (h ==  3) {}
    else if (h ==  4) {}
    else if (h ==  5) {}
    else if (h ==  6) {}
    else if (h ==  7) {}
    else if (h ==  8) {}
    else if (h ==  9) {}
    else if (h == 10) {}
    else if (h == 11) {}
    else if (h == 12) {}
    else if (h == 13) {}
  }
}


//------------------------------------------------------------------------------
// Name:      TIMER2_OVF_vect
// Function:  TIMER2 Overflow mit 1Hz (32kHz Uhrenquarz /128 /256)
//            
// Parameter: 
// Return:    
//------------------------------------------------------------------------------
ISR(TIMER2_OVF_vect) {
  second++;
  if (second > 59) {
    second=0;
    minute++;
    if (minute > 59) {
      minute = 0;
      hour++;
      if (hour > 11) hour = 1;
      hour_flag = true;
    }
    min_flag = true;
  }
  sec_flag = true;
}

//------------------------------------------------------------------------------
// Name:      TIMER1_COMPA_vect
// Function:  LED-Matrix Refresh mit 1,2kHz -> 100Hz Bildwiderholrate
//            
// Parameter: 
// Return:    
//------------------------------------------------------------------------------
ISR(TIMER1_COMPA_vect) {
  uint8_t i = frame_index;
  if (i == 0) {
    DATA_CLR;
  } else {
    DATA_SET;
  }
  CLK_SET;
  CLK_CLR;

  PORTC &= ~0x0F;
  PORTD &= ~0xF0;
  PORTB &= ~0x03;

  STROBE_SET;
  STROBE_CLR;

  uint16_t leds = frame[i];

  PORTC |= leds & 0x0F;
  PORTD |= leds & 0xF0;
  PORTB |= (leds >> 8) & 0x03;

  if (++i >= FRAME_WIDTH) {
    frame_index = 0;
    sync_flag = true;
  } else {
    frame_index = i;
  }
}
