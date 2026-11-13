//---------------------------------------------------------------------------

#include "lib.hpp"

#include <math.h>
//#pragma hdrstop

#include "NoteBuffer.h"
//#include "PlayRecordBuffer.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

// Notes
// 0 = middle C
// 12 = C next octave
// =====
// Duration
// 256ths of a whole
// eg:
// 256 = whole note, 128 = half, 64 = quarter/quaver etc...

int Maximum;

float NoteFrequency [13];

void NoteBufferInit (void)
  {
    int i;
    float f;
    ////////////
    f = 261.6255653005986346778499935233; // Middle C in hertz
    for (i=0; i<13; i++)
      {
        NoteFrequency [i] = f;
        f *= pow (2.0, 1.0/12.0);
      }
  }

typedef enum {sAttack, sDecay, sSustain, sRelease} TEnvelopeState;

void EnvelopeMake (TEnvelope *Envelope, bool Active, float *Amplitude, TEnvelopeState *EnvelopeState)
  {
    if (!Active)
      *EnvelopeState = sRelease;
    switch (*EnvelopeState)
      {
        case sAttack:
          if (Envelope->Attack == 1000)
            *Amplitude = 1.0;
          else
            *Amplitude += (float) (Envelope->Attack) / 10.0 / 44100.0;
          if (*Amplitude >= 1.0)
            {
              *Amplitude = 1.0;
              *EnvelopeState = sDecay;
            }
          break;
        case sDecay:
          if (Envelope->Decay == 1000)
            *EnvelopeState = sSustain; //*Amplitude = 0.0;
          else
            *Amplitude -= (float) (Envelope->Decay) / 10.0 / 44100.0;
          if (*Amplitude <= Envelope->Sustain / 1000.0)
            {
              //*Amplitude = Envelope->Sustain / 1000.0;
              *EnvelopeState = sSustain;
            }
          break;
        case sSustain:
          *Amplitude = Envelope->Sustain / 1000.0;
          break;
        case sRelease:
          if (Envelope->Release == 1000)
            *Amplitude = 0.0;
          *Amplitude -= (Envelope->Release) / 10.0 / 44100.0;
          if (*Amplitude <= 0.0)
            *Amplitude = 0.0;
          break;
      }
  }

float Power (float Val, int Exp)
  {
    float Result;
    ////////////////////////////////////////
    Result = 1.0;
    while (Exp > 0)
      {
        Result *= Val;
        Exp--;
      }
    while (Exp < 0)
      {
        Result /= Val;
        Exp++;
      }
    return Result;
  }

float Frac (float Val)
  {
    return Val - floor (Val);
  }

int NoteLength (int Tempo /*bpm*/, int Duration /*milliBeats*/)   // Result in Samples (Each sample is 2 x int16 (left, right))
  {
    if (Tempo)
      return 44100 * Duration / Tempo * 120 / 512;
    return 0;
  }

void NoteBuffer (byte Volume, int Tempo, int Note, int Duration /*mBeats*/, _Synth *Synth, short *Buffer, int BufferIndex, int BufferSize)   // Buffer*: word index
  {
    int Start;
    int k;
    int x;
    float freq;
    float multi;
    float Period;
    int tNote;
    float CyclePart, CyclePart_;
    float filterK;
    TEnvelopeState StateAmp, StateFilt;
    float Amp, Filt;
    float Amp_, Filt_;
    float LoOldY [4];   // Low Pass Filter
    int ModPeriod, ModIndex;
    float Mod;
    float ModOffset;
    int y, HiOldX, HiOldY;   // High Pass Filter (for DC Removal to reduce 16 bit overflow)
    /////////////////////////
    if (Synth == NULL)
      return;
    if (BufferIndex < 0)
      BufferIndex = 0;
    multi = 1.0;
    Note = Note + Synth->Octave * 12;
    while (Note >= 12)
      {
        multi *= 2.0;
        Note  -= 12;
      }
    while (Note < 0)
      {
        multi /= 2.0;
        Note  += 12;
      }
    //InitNoteFrequency ();
    freq = NoteFrequency [Note] * multi;
    Period = 44100.0 / freq;   // In samples
    ModPeriod = (44100.0 / (Synth->Modulate.Freq / 10.0)) + 0.5;
    tNote = 2 * NoteLength (Tempo, Duration) + BufferIndex;
    Amp = 0.0;
    StateAmp = sAttack;
    Filt = 0.0;
    StateFilt = sAttack;
    for (k = 0; k < 4; k++)
      LoOldY [k] = 0.0;
    HiOldX = 0;
    HiOldY = 0;
    Start = BufferIndex;
    ModOffset = 0.0;
    while (true)
      {
        if (BufferIndex >= BufferSize)
          break;
        // Calculate Envelopes
        EnvelopeMake (&Synth->Amplitude, BufferIndex < tNote, &Amp, &StateAmp);
        EnvelopeMake (&Synth->Filter, BufferIndex < tNote, &Filt, &StateFilt);
        if (Amp * Filt == 0.0) // ((Amp <= 0.0) || (Filt <= 0,0))
          break;
        // Where in waveform
        CyclePart = Frac (((BufferIndex - Start) >> 1) / Period);
        // Calculate Modulation - triangle wave 0.0-1.0
        ModIndex = (BufferIndex >> 1) % ModPeriod;   // Wrap around modulation period
        if (ModIndex < (ModPeriod / 2))
          Mod = (float)ModIndex / ((float)ModPeriod / 2.0);
        else
          Mod = 1.0 - ((float)ModIndex - ((float)ModPeriod / 2.0)) / ((float)ModPeriod / 2.0);
        // Apply Modulation according to Mode and Depth
        Amp_ = Amp;
        filterK = Filt * Filt;
        switch (Synth->Modulate.Mode)
          {
            case mPitch:  ModOffset += 0.005 * ((Mod - 0.5) * Synth->Modulate.Depth / 100.0);
                          CyclePart = Frac (((BufferIndex - Start) / 2) / Period + ModOffset);
                          while (CyclePart >= 1.0)
                            CyclePart = CyclePart - 1.0;
                          while (CyclePart < 0.0)
                            CyclePart = CyclePart + 1.0;
                          break;
            case mAmp:    Amp *= (1.0 - (Mod * Synth->Modulate.Depth / 100.0));
                          break;
            case mFilter: filterK = Filt * Filt * (1.0 - (Mod * Synth->Modulate.Depth / 100.0));
                          break;
          }
        // Calculate Skew
        CyclePart_ = CyclePart * 100 / (100 + Synth->Skew);
        if (CyclePart_ >= 0.5)
          CyclePart_ = 1.0 - (1.0 - CyclePart) * 100 / (101 - Synth->Skew);
        CyclePart = CyclePart_;
        // Generate Sample for CyclwIndex
        switch (Synth->WaveForm)
          {
            case wRandom: // Random
              x = (rand () & 0xFFFF) - 0x8000;
              break;
            case wSquare: // Square
              if (CyclePart < 0.5)
                x = -32000;
              else
                x = +32000;
              break;
            case wSine: // Sine Wave
              x = sin (CyclePart * (2 * M_PI)) * 32000;
              break;
            case wTriangle: // Triangle
              if (CyclePart < 0.5)
                x = CyclePart * 64000 * 2 - 32000;
              else
                x = (1.0 - CyclePart) * 64000 * 2 - 32000;
              break;
          }
        // Apply Filter
        for (k = 0; k < 4; k++)
          {
            if (filterK == 1.0)
              y = x;
            else
              y = filterK * x + (1.0 - filterK) * LoOldY [k];
            LoOldY [k] = y;
            x = y;
          }
        /*y = Filt * (HiOldY + x - HiOldX);
        HiOldX = x;
        HiOldY = y;*/
        // Apply Amplification
        x *= Amp;
        // Apply High Pass Filter (for DC removal). Noise: This is adjusted by the note
        filterK = 0.999; //0.9999;
        if (Synth->WaveForm == wRandom)
          if ((Note > 0) && (Note <= 12))
            filterK = 1.0 - (float) Note / 100.0;
        y = filterK * (HiOldY + x - HiOldX);   // Alpha = 0.999 => RC = 227mS, 1/RC = 4.4Hz
        HiOldX = x;
        HiOldY = y;
        x = y;
        // Place Value in Buffer (left / right)
        if (Buffer)
          {
            k = Buffer [BufferIndex] + x * Synth->MixLeft / 100 * Volume / 100; // %%%% multiply before devide
            if (k > Maximum)
              Maximum = k;
            if (-k > Maximum)
              Maximum = -k;
            Buffer [BufferIndex++] = k;
            k = Buffer [BufferIndex] + x * Synth->MixRight / 100 * Volume / 100;
            if (k > Maximum)
              Maximum = k;
            if (-k > Maximum)
              Maximum = -k;
            Buffer [BufferIndex++] = k;
          }
      }
  }

int EnvelopeToStr (char **St, TEnvelope *Envelope)
  {
    return
      NumToStr (St, Envelope->Attack) +
      StrCat (St, '\t') +
      NumToStr (St, Envelope->Decay) +
      StrCat (St, '\t') +
      NumToStr (St, Envelope->Sustain) +
      StrCat (St, '\t') +
      NumToStr (St, Envelope->Release);
  }

int ModulateToStr (char **St, TModulate *Modulate)
  {
    return
      NumToStr (St, Modulate->Freq) +
      StrCat (St, '\t') +
      NumToStr (St, Modulate->Depth) +
      StrCat (St, '\t') +
      NumToStr (St, Modulate->Mode);
  }

int SynthToStr (char **St, _Synth *Synth)
  {
    return
      IntToStr (St, Synth->WaveForm) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->Skew) +
      StrCat (St, '\t') +
      EnvelopeToStr (St, &Synth->Amplitude) +
      StrCat (St, '\t') +
      EnvelopeToStr (St, &Synth->Filter) +
      StrCat (St, '\t') +
      ModulateToStr (St, &Synth->Modulate) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->Octave) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->MixLeft) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->MixRight) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->Strum) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->Dither) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->MIDIInstrument) +
      StrCat (St, '\t') +
      IntToStr (St, Synth->MIDIPercussion);
  }

void StrToSynth (char **St, _Synth *Synth)
  {
    Synth->WaveForm = (TWaveForm) StrGetNum (St);
    Synth->Skew = StrGetNum (St);
    Synth->Amplitude.Attack = StrGetNum (St);
    Synth->Amplitude.Decay = StrGetNum (St);
    Synth->Amplitude.Sustain = StrGetNum (St);
    Synth->Amplitude.Release = StrGetNum (St);
    Synth->Filter.Attack = StrGetNum (St);
    Synth->Filter.Decay = StrGetNum (St);
    Synth->Filter.Sustain = StrGetNum (St);
    Synth->Filter.Release = StrGetNum (St);
    Synth->Modulate.Freq = StrGetNum (St);
    Synth->Modulate.Depth = StrGetNum (St);
    Synth->Modulate.Mode = (TModulateMode) StrGetNum (St);
    Synth->Octave = StrGetNum (St);
    Synth->MixLeft = StrGetNum (St);
    Synth->MixRight = StrGetNum (St);
    Synth->Strum = StrGetNum (St);
    Synth->Dither = StrGetNum (St);
    Synth->MIDIInstrument = StrGetNum (St);
    Synth->MIDIPercussion = StrGetNum (St);
  }

void EnvelopeInit (TEnvelope *Envelope)
  {
    Envelope->Attack = 1000;
    Envelope->Decay = 1000;
    Envelope->Sustain = 1000;
    Envelope->Release = 1000;
  }

void ModulateInit (TModulate *Modulate)
  {
    Modulate->Freq = 1;
    Modulate->Depth = 0;
    Modulate->Mode = mNone;
  }

void SynthInit (_Synth *Synth)
  {
    Synth->WaveForm = wSine;
    Synth->Skew = 0;
    Synth->Octave = 0;
    Synth->MixLeft = 50;
    Synth->MixRight = 50;
    EnvelopeInit (&Synth->Amplitude);
    EnvelopeInit (&Synth->Filter);
    Synth->Filter.Release = 0;
    ModulateInit (&Synth->Modulate);
    Synth->Strum = 0;
    Synth->Dither = 0;
    Synth->MIDIInstrument = 1;
    Synth->MIDIPercussion = false;
  }


/*
        //x = x * Filt + (sin ((float) j / (float) Period * (2 * M_PI)) * 32000) * (1.0 - Filt);

              if (tAtt == 0)
                Amp = 1.0;
              else
                Amp = (i / 2.0) / tAtt;
              //Amp += (1.0 / 44100.0) * Sound->Amplitude.Attack / 50.0;
              //Amp *= 1.0 + Sound->Amplitude.Attack / 500000.0;


              if (tDec == 0)
                Amp = 1.0;
              else
                Amp = (i / 2.0) / tAtt;
*/

        /*if (filterK == 1.0)
          x = 0;
        /*x = (1.0 - filterK) * x + filterK * xOld [0];
        x = Power ((1.0 - filterK), 4) * x
          + 4 * filterK * xOld [0]
          - 6 * Power (filterK, 2) * xOld [1]
          + 4 * Power (filterK, 3) * xOld [2]
          - Power (filterK, 4) * xOld [3];
        for (k = 6; k >= 0; k--)
          xOld [k + 1] = xOld [k];
        xOld [0] = x;
          */

            /*case wSawTooth: // Saw Tooth
              x = j * 64000 / Period - 32000;
              break;*/

        /*
        // Next Sample
        CycleIndex++;
        if (((BufferIndex - Start) * 100) / Period != Cycle)
          {
            CycleIndex = 0;
            Cycle++;
          }
        //if (CycleIndex >= Period_)
        //  CycleIndex = 0;
        */


