//---------------------------------------------------------------------------

#ifndef NoteBufferH
#define NoteBufferH
//---------------------------------------------------------------------------
#endif

typedef unsigned char byte;

typedef enum {wSine, wTriangle, wSquare, wRandom} TWaveForm;
typedef enum {mNone, mPitch, mFilter, mAmp} TModulateMode;

typedef struct
  {
    int Attack;
    int Decay;
    int Sustain;
    int Release;
  } TEnvelope;

typedef struct
  {
    int Freq; // In 1/10 of hertz
    byte Depth; // %
    TModulateMode Mode;
  } TModulate;

typedef struct
  {
    TWaveForm WaveForm;
    byte Skew;
    short Octave;
    byte MixLeft;
    byte MixRight;
    TEnvelope Amplitude;
    TEnvelope Filter;
    TModulate Modulate;
    byte Strum;
    byte Dither;
    //
    byte MIDIInstrument;   // Patch 1-128
    bool MIDIPercussion;   // True => Use channel 10
    bool Mute;
  } _Synth;

extern int Maximum;

extern void NoteBufferInit (void);

extern int NoteLength (int Tempo /*bpm*/, int Duration /*milliBeats*/);   // Result in Samples (Each sample is 2 x int16 (left, right))
extern void NoteBuffer (byte Volume, int Tempo, int Note, int Duration /*milliBeats*/, _Synth *Synth, short *Buffer, int BufferIndex, int BufferSize /*int16 indexes*/);

extern int SynthToStr (char **St, _Synth *Synth);
extern void StrToSynth (char **St, _Synth *Synth);

extern void SynthInit (_Synth *Synth);

