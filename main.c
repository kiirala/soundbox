#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>

#define nop() __asm__ __volatile__("nop"::)
#define SET_BIT(port, bit) port |= (uint8_t)(1 << (bit))
#define CLEAR_BIT(port, bit) port &= (uint8_t)(~(1 << (bit)))

uint8_t rand() {
  static unsigned int val;
  val = val * 1103515245U + 12345U;
  return val >> 8;
}

uint8_t adc_read(uint8_t channel) {
  ADMUX = channel | (1 << ADLAR);
  ADCSRA |= (1 << ADSC);
  while((ADCSRA & (1 << ADSC)))
    ;
  return ADCH;
}

#define PITCH_PORT ((0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0))
#define TEMPO_PORT ((0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (1 << MUX0))
#define TEMPO_LED PORTB1

struct Oscillator {
  uint16_t value;
  uint16_t frequencyControl;
  uint8_t volume;
  uint8_t overflowCount;
  uint8_t *pacTable;
};
#define OSCILLATOR_COUNT 2

struct Oscillator oscillators[OSCILLATOR_COUNT];

ISR(TIMER0_OVF_vect) {
  uint16_t sum = 0;

  oscillators[0].value += oscillators[0].frequencyControl;
  sum += oscillators[0].pacTable[oscillators[0].value >> 8] *
    oscillators[0].volume;

  uint16_t oldValue = oscillators[1].value;
  oscillators[1].value += oscillators[1].frequencyControl;
  if ((oscillators[1].value ^ oldValue) & 0x1000)
    oscillators[1].overflowCount++;
  sum += oscillators[1].pacTable[oscillators[1].overflowCount] *
    oscillators[1].volume;

  sum /= OSCILLATOR_COUNT;
  sum >>= 7;
  if (sum > 255) sum = 255;
  OCR0A = sum;
}

uint8_t randomTable[256];
uint8_t sinTable[256];

#define LOOP_LENGTH 64
uint8_t loop[64];

int main() {
  wdt_disable();

  DDRB = 0xff;
  DDRD = (1 << DDD6);
  PORTD = (1 << PORTD0);
  PORTB = 0xff;

  for (int i = 0 ; i < OSCILLATOR_COUNT ; ++i) {
    oscillators[i].value = 0;
    oscillators[i].frequencyControl = 0;
    oscillators[i].volume = 0xff;
    oscillators[i].overflowCount = 0;
  }

  for (int i = 0 ; i < 256 ; i += 2) {
    float u1 = rand() / 255.0f;
    float u2 = rand() / 255.0f;
    // Box-Muller transform
    float nat1 = sqrtf(-2 * log(u1)) * cosf(2 * M_PI * u2);
    float nat2 = sqrtf(-2 * log(u1)) * sinf(2 * M_PI * u2);
    int val1 = nat1 * 64 + 127;
    if (val1 < 0) val1 = 0;
    if (val1 > 255) val1 = 255;
    int val2 = nat2 * 64 + 127;
    if (val2 < 0) val2 = 0;
    if (val2 > 255) val2 = 255;
    randomTable[i] = val1;
    randomTable[i + 1] = val2;
  }
  oscillators[1].pacTable = randomTable;

  for (int i = 0 ; i < 256 ; ++i) {
    sinTable[i] = (uint8_t)(sinf(i * M_PI / 128.0f) * 127.0f + 127.0f);
  }
  oscillators[0].pacTable = sinTable;

  // Disable digital input buffers on ADC channels.
  DIDR0 = (1 << ADC1D) | (1 << ADC0D);
  // Enable ADC
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Clock 0, output A in fast PWM mode
  TCCR0A = (1 << COM0A1) | (0 << COM0A0) | (0 << COM0B1) | (0 << COM0B0) |
    (1 << WGM01) | (1 << WGM00);
  // continued, set clock on, no prescaling
  TCCR0B = (0 << WGM02) | (0 << CS02) | (0 << CS01) || (1 << CS00);
  // Start at 50% cycle 
  OCR0A = 0x7f;
  TIFR0 = TIFR0;
  // Enable counter overflow interrupt
  TIMSK0 = (0 << OCIE0B) || (0 << OCIE0A) || (1 << TOIE0);

  sei();

  int loopPosition = 0;
  while (1) {
    if (loopPosition == 0) PORTB = (1 << PORTB1);
    else PORTB = 0;
    uint8_t pitch = adc_read(PITCH_PORT);
    if (!(PIND & (1 << PIND0))) {
      loop[loopPosition] = pitch;
    }
    oscillators[0].frequencyControl = loop[loopPosition] << 4;
    oscillators[1].frequencyControl = loop[loopPosition] << 4;
    uint8_t tempo = adc_read(TEMPO_PORT);
    for (int i = 0 ; i < (255 - tempo) ; ++i) {
      for (int j = 0 ; j < 256 * 4 ; ++j) {
	nop();
      }
    }
    ++loopPosition;
    if (loopPosition == LOOP_LENGTH) loopPosition = 0;
  }

}
