#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <SDL.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#define INT16_MAX (32767)
#define INT16_MIN (-32768)

class Oscillator {
public:
  uint16_t phase;
  uint16_t frequencyControl;
  uint16_t volume;
  unsigned int overflowCount;
  int16_t *pacTable;

  Oscillator(int16_t *pacTable)
    : phase(0), frequencyControl(0), overflowCount(0), pacTable(pacTable)
  { }

  virtual int16_t next() {
    uint16_t oldPhase = phase;
    phase += frequencyControl;
    if (phase < oldPhase) ++overflowCount;
    return pacTable[phase];
  }

  void setFrequency(unsigned int frequency, unsigned int samplerate) {
    uint16_t old = frequencyControl;
    frequencyControl = 65536 * frequency / samplerate;
    if (old != frequencyControl)
      printf("freq: %6u, fcw: %u\n", frequency, frequencyControl);
  }
};

class NoiseOscillator : public Oscillator {
public:
  NoiseOscillator(int16_t *pacTable)
    : Oscillator(pacTable)
  { }

  virtual int16_t next() {
    Oscillator::next();
    return pacTable[overflowCount & 0xffff];
  }
};

/* global decoding state. */
class AudioCallbackData {
public:
  SDL_AudioSpec devformat;
  unsigned int tick;
  int oscillatorCount;
  Oscillator **oscillator;

  AudioCallbackData(int oscillatorCount)
    : tick(0), oscillatorCount(oscillatorCount),
      oscillator(new Oscillator*[oscillatorCount])
  { }

  ~AudioCallbackData() {
    delete [] oscillator;
  }

  void setFrequency(int oscillatorIndex, int frequency) {
    assert(oscillatorIndex >= 0 && oscillatorIndex < oscillatorCount);
    oscillator[oscillatorIndex]->setFrequency(frequency, this->devformat.freq);
  }
};

static void audio_callback(void *userdata, Uint8 *stream_orig, int len) {
  AudioCallbackData *data = reinterpret_cast<AudioCallbackData*>(userdata);
  int bytesWritten = 0;
  int16_t *stream = reinterpret_cast<int16_t*>(stream_orig);
  int streamPos = 0;

  while (bytesWritten < len) {
    int32_t accumulator = 0;
    for (int i = 0 ; i < data->oscillatorCount ; ++i) {
      accumulator += data->oscillator[i]->next() *
	data->oscillator[i]->volume / 65535;
    }
    if (accumulator > INT16_MAX) accumulator = INT16_MAX;
    else if (accumulator < INT16_MIN) accumulator = INT16_MAX;
    for (int i = 0 ; i < data->devformat.channels ; ++i) {
      stream[streamPos] = accumulator;
      ++streamPos;
      bytesWritten += 2;
    }
    data->tick++;
  }
}

int16_t *makeSinTable() {
  static int16_t table[65536];
  for (int i = 0 ; i < 65536 ; ++i) {
    table[i] = INT16_MAX * sin(i * 2 * M_PI / 65536.0);
  }
  return table;
}

int16_t *makeNoiseTable() {
  static int16_t table[65536];
  for (int i = 0 ; i < 65536 ; ++i) {
    table[i] = rand() & 0xffff;
  }
  return table;
}

void init_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    exit(1);
  }
}

void setupSound(AudioCallbackData *callbackData) {
  SDL_AudioSpec aspec, receivedAspec;
  aspec.freq = 44100;
  aspec.format = AUDIO_S16;
  aspec.channels = 2;
  aspec.samples = 2048;
  aspec.callback = audio_callback;
  aspec.userdata = callbackData;
  if (SDL_OpenAudio(&aspec, &receivedAspec) != 0) {
    fprintf(stderr, "Opening audio failed: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }
  callbackData->devformat = receivedAspec;
}

int main(int argc, char ** argv) {
  /*
  int opt;
  while ((opt = getopt(argc, argv, "")) != -1) {
    switch (opt) {
    default:
      fprintf(stderr,
	      "Usage: %s [-f] [-w] [-s 1280x720]\n"
	      "    -f: fullscreen\n"
	      "    -w: windowed\n"
	      "    -s: screen resolution\n",
	      argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  */

  init_sdl();

  AudioCallbackData callbackData(2);
  setupSound(&callbackData);

  Oscillator sinOscillator(makeSinTable());
  NoiseOscillator noiseOscillator(makeNoiseTable());
  callbackData.oscillator[0] = &sinOscillator;
  callbackData.oscillator[0]->volume = (int)(0.0 * 65535);
  callbackData.setFrequency(0, 440);
  callbackData.oscillator[1] = &noiseOscillator;
  callbackData.oscillator[1]->volume = (int)(0.50 * 65535);
  callbackData.setFrequency(1, 440);

  SDL_PauseAudio(0);

  SDL_Event event;
  bool running = true;

  while (running) {
    callbackData.setFrequency(1, (int)(440.0 * pow(2.0, ((callbackData.tick >> 14) - 49.0) / 12.0)));
    while (SDL_PollEvent(&event)) {
      switch( event.type ) {
      case SDL_KEYDOWN:
	if (event.key.keysym.sym == SDLK_ESCAPE)
	  running = false;
	break;
      case SDL_QUIT:
        running = false;
        break;

      default:
        break;
      }
    }
  }

  SDL_Quit();
  return 0;
}
