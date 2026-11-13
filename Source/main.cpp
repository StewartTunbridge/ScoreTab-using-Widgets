///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS MUSIC NOTATION PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2010, 2026 Stewart Tunbridge, Pi Micros
//
//
// TODO:
//   Create from NoteTabEditor + other Widgets projects
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "1.1";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"
#include "../Widgets/ColourSelect.hpp"
#include "../Widgets/MessageBox.hpp"
#include "../Widgets/MessageResponseBox.hpp"

#include "NoteBuffer.h"

extern void DebugAddR (const char *St, _Rect r);


////////////////////////////////////////////////////////////////////////////////
//
// GLOBALS
//
////////////////////////////////////////////////////////////////////////////////

#define MaxStrings 8
#define MaxRiffs 200
#define MaxSynths 200
#define MaxProject 200

//#define fNote    0x01
//#define fRest    0x02
#define fDotted  0x04
#define fDotDotted  0x08
#define fTie 0x10

#define None IntMin

typedef struct
  {
    int fret [MaxStrings]; //per string, [0] is low E, 0 = fret 0 (nut) .. None = no note
    int duration; //None = end, 0 = 2 x whole, 1 = whole, 2 = half, 3 = quarter, 4 = 1/8 ...
    byte flags;
  } _Chord;

typedef struct
  {
    int   x1, x2;
    int   Strings [MaxStrings];
    int  Tempo;   // bpn
    int   Key;   // 0 = C
    char *TimeSigA;   // Multiple Numerators (typically "4" or "3" or "4 4 4 3" etc)
    int   TimeSigB;   // Denominator (typically 4)
    _Chord *Chords;
    int   ChordsSize;
  } _Riff;

char *RiffNames;
_Riff Riffs [MaxRiffs];

char *SynthNames;
bool SynthNamesChanged;
_Synth Synths [MaxSynths];

int NumProjectItems;
char *Project [MaxProject] = {NULL};

char *Memo = NULL;

int SynthTab = -1;   // Current Selected Synthesizer
int MasterVolume = 20;   // Current sound volume


// Notes
// =====
// 0 = middle C
// 12 = C next octave
// -12 = C previous octave

//const int  Scale [] = {0,     0,    1,     1,    2,     3,     3,    4,     4,    5,     5,    6};
//const bool Sharp [] = {false, true, false, true, false, false, true, false, true, false, true, false};
const int SemiToneToTone [12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
const int ToneToSemiTone [8]  = {0, 2, 4, 5, 7, 9, 11, 12};  // Maps Tones to Semitones, with an extra C at the end
const int Duration1024 [] = {2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1};   // milli Beats: Converts duration into 1024'ths
const int StringNote_ [] = {-8, -3, 2, 7, 11, 16, None, None};

const int YMin = -13;
const int YMax = 21;

typedef enum {aNone, aSharp, aFlat, aNatural} TActType;

_Riff *RiffCur;
int RiffDur;
word RiffFlags;
int RiffY;

bool Redraw = false;
bool SelectionChange = true;
bool Changed;
bool Rendering;


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SUPPORT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void ChordInit (_Chord *Chord)
  {
    int i;
    ////////////////////////////////////////////////////////////////////////////
    for (i = 0; i < MaxStrings; i++)
      Chord->fret [i] = None;
    Chord->duration = None;
    Chord->flags = 0;
  }

void ChordsSizeCheck (_Riff *Riff, int Size)
  {
    _Chord *ChordsNew;
    ////////////////////////////////////////////////////////////////////////////
    if (Size > Riff->ChordsSize)
      {
        Size |= 7;
        ChordsNew = (_Chord *) malloc (Size * sizeof (_Chord));
        memcpy (ChordsNew, Riff->Chords, Riff->ChordsSize * sizeof (_Chord));
        free (Riff->Chords);
        Riff->Chords = ChordsNew;
        while (Riff->ChordsSize < Size)
          ChordInit (&Riff->Chords [Riff->ChordsSize++]);
      }
  }

void RiffInit (_Riff *Riff)
  {
    int j;
    //////////////////////////////////////////////////////////
    Riff->x1 = 0;
    Riff->x2 = 0;
    for (j = 0; j < MaxStrings; j++)
      Riff->Strings [j] = StringNote_ [j];   // Set default tuning
    Riff->Tempo = 120;
    StrAssignCopy (&Riff->TimeSigA, "4");
    Riff->TimeSigB = 4;
    free (Riff->Chords);
    Riff->Chords = NULL;
    Riff->ChordsSize = 0;
    //NotesSizeCheck (Riff, 1);
  }

void RiffsInit (void)
  {
    int i;
    //////////////
    for (i = 0;i < MaxRiffs; i++)
      RiffInit (&Riffs [i]);
    RiffCur = &Riffs [0];
  }

void SynthsInit (void)
  {
    int i;
    ///////////////////////////////////////////////////
    for (i = 0; i < MaxSynths; i++)
      SynthInit (&Synths [i]);
  }

void ProjectInit (void)
  {
    int i;
    ///////////////////////////////////////////////////
    for (i = 0; i < MaxProject; i++)
      {
        free (Project [i]);
        Project [i] = NULL;
      }
  }

int NotesInRiff (_Riff *Riff)
  {
    int i;
    //////////
    i = 0;
    while (true)
      {
        if (i >= Riff->ChordsSize)
          break;
        if (Riff->Chords [i].duration == None)
          break;
        i++;
      }
    return i;
  }

int TimeInNote (_Chord *Note)   // returns milliBeats
  {
    int Dur;
    ///////////////////////////////////////////
    Dur = 0;
    if (Note->duration != None)
      {
        Dur = Duration1024 [Note->duration];
        if (Note->flags & fDotDotted)
          Dur += (Dur >> 1) + (Dur >> 2);
        else if (Note->flags & fDotted)
          Dur += Dur >> 1;
      }
    return Dur;
  }

int TimeInRiff (_Riff *Riff, int Index)   // returns milli-Wholes
  {
    int i, Tim, TimTotal;
    //////////
    TimTotal = 0;
    i = 0;
    while (true)
      {
        if (i >= Riff->ChordsSize)
          break;
        if (i == Index)
          break;
        Tim = TimeInNote (&Riff->Chords [i]);
        if (Tim == 0)
          break;
        TimTotal += Tim;
        i++;
      }
    return TimTotal;
  }

int Log2 (int Val)
  {
    int Res;
    //
    Res = 0;
    while (Val > 1)
      {
        Val >>= 1;
        Res++;
      }
    return Res;
  }

int Exp2 (int Val)
  {
    return Bit [Val];
  }
                //   C  G     D     A     E     B     F#
word KeySharps [] = {0, 0x08, 0x09, 0x19, 0x1B, 0x3B, 0x3F};   // Which Tones are "sharped" per Key
                //   c  F     Bb    Eb    Ab    Db    Gb
word KeyFlats  [] = {0, 0x40, 0x44, 0x64, 0x66, 0x76, 0x77};

const char *NoteName [12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char *KeyNames = "G flat\tD flat\tA flat\tE flat\tB flat\tF\tC\tG\tD\tA\tE\tB\tF#";

int SemiToneAdjust (int SemiTone, word Augment, word Diminish)
  {
    if (Augment)
      SemiTone++;
    if (Diminish)
      SemiTone--;
    return SemiTone;
  }

#define ArraySize(Array) (sizeof (Array) / sizeof (Array [0]))

int FindNoteInKey (int SemiTone, word Sharps, word Flats)
  {
    int i;
    //
    for (i = 0; i < ArraySize (ToneToSemiTone); i++)
      if (SemiTone == SemiToneAdjust (ToneToSemiTone [i], Sharps & Bit [i % 7], Flats & Bit [i % 7]))
        return i;
    return -1;
  }

int MapSemiToneToTone (_Riff *Riff, int SemiTone, TActType *Act)   // Finds place of the semitone in the current scale setting Act
  {
    int Octave, Tone;
    word Sharps, Flats;
    ///////////////////////////////
    *Act = aNone;
    Octave = 0;
    // Eliminate Octave movements
    while (SemiTone >= 12)
      {
        SemiTone -= 12;
        Octave++;
      }
    while (SemiTone < 0)
      {
        SemiTone += 12;
        Octave--;
      }
    Sharps = 0x00;
    if (Riff->Key > 0)   // sharp key specified
      Sharps = KeySharps [Riff->Key];
    Flats = 0x00;
    if (Riff->Key < 0)   // flat key specified
      Flats = KeyFlats [-Riff->Key];
    Tone = FindNoteInKey (SemiTone, Sharps, Flats);
    if (Tone < 0)   // Not in Key
      {
        Tone = FindNoteInKey (SemiTone, 0, 0);   // Test for Natural note
        if (Tone >= 0)
          *Act = aNatural;
        else   // Not a Natural
          {
            Tone = FindNoteInKey (SemiTone, 0xFFFF, 0);
            if (Tone >= 0)
              *Act = aSharp;
          }
      }
    return Tone + (Octave * 7);
  }

int MapToneToSemiTone (int Tone, int Key)
  {
    int Octave;
    int SemiTone;
    ///////////////////////////////
    Octave = 0;
    while (Tone >= 7)
      {
        Tone -= 7;
        Octave++;
      }
    while (Tone < 0)
      {
        Tone += 7;
        Octave--;
      }
    SemiTone = ToneToSemiTone [Tone];
    if (Key > 0)   // sharp key specified
      if (KeySharps [Key] & Bit [Tone])
        SemiTone++;
    if (Key < 0)   // sharp key specified
      if (KeyFlats [-Key] & Bit [Tone])
        SemiTone--;
    return SemiTone + (Octave * 12);
  }

typedef enum {fsFind, fsAdd, fsRemove} TFindStringOperation;

bool FindString (int NotesIndex, int Semi, TFindStringOperation Op) //int *String, int *Fret)   // String < 0 => no possible string
  {
    int String;
    int Fret;
    bool Result;
    _Chord *Note;
    ///////////////////////////////////////
    ChordsSizeCheck (RiffCur, NotesIndex + 1);
    Note = &RiffCur->Chords [NotesIndex];
    Result = false;
    String = MaxStrings - 1;
    while (true)
      {
        if (String < 0)
          break;
        if (RiffCur->Strings [String] != None)
          {
            if (Note->fret [String] == None)   // String is free
              {
                Fret = Semi - RiffCur->Strings [String];
                if (Fret >= 0)
                  if (Op == fsAdd)
                    {
                      Note->fret [String] = Fret;
                      Changed = true;
                      Result = true;
                      break;
                    }
              }
            else   // String is not free
              {
                Fret = Semi - RiffCur->Strings [String];
                if (Fret == Note->fret [String])   // Same note found
                  {
                    if (Op == fsFind)
                      {
                        Result = true;
                        break;
                      }
                    else if (Op == fsRemove)
                      {
                        Note->fret [String] = None;
                        Changed = true;
                        Result = true;
                      }
                    else if (Op == fsAdd)   // don't add if already there
                      {
                        Result = true;
                        break;
                      }
                  }
              }
          }
        String--;
      }
    return Result;
  }

void TidyFrets (int NotesIndex, int Offset)   // Optimize notes for playability (insert in descending order)
  {
    int i, j;
    int NoteSet [MaxStrings];
    bool Mess;
    _Chord *Note;
    ///////////////////////////////////////
    Note = &RiffCur->Chords [NotesIndex];
    // Read notes into NoteSet
    for (j = 0; j < MaxStrings; j++)
      NoteSet [j] = -1000;
    j = 0;
    for (i = 0; i < MaxStrings; i++)
      if (Note->fret [i] != None)
        {
          NoteSet [j++] = RiffCur->Strings [i] + Note->fret [i] + Offset;
          Note->fret [i] = None;
        }
    // Sort
    do
      {
        Mess = false;
        for (i = 0; i < MaxStrings - 1; i++)
          if (NoteSet [i] < NoteSet [i + 1])
            {
              j = NoteSet [i];
              NoteSet [i] = NoteSet [i + 1];
              NoteSet [i + 1] = j;
              Mess = true;
            }
      }
    while (Mess);
    // Reinsert
    j = 0;
    while (true)
      {
        if (j == MaxStrings)
          break;
        if (NoteSet [j] == -1000)
          break;
        FindString (NotesIndex, NoteSet [j], fsAdd);
        j++;
      }
  }

bool BeamOK (int Time, int duration)
  {
    return true;
  }

bool Rest (_Chord *Chord)
  {
    bool Result;
    int i;
    //
    Result = true;
    for (i = 0; i < MaxStrings; i++)
      if (Chord->fret [i] != None)
        Result = false;
    return Result;
  }

typedef struct
  {
    word  NoteSet;
    char *Name;
  } _ChordPattern;

#define N1  Bit[0]
#define N2  Bit[2]
#define N3d Bit[3]
#define N3  Bit[4]
#define N4  Bit[5]
#define N5d Bit[6]
#define N5  Bit[7]
#define N5a Bit[8]
#define N6  Bit[9]
#define N7d Bit[10]
#define N7  Bit[11]

const _ChordPattern ChordPatterns [] =
  {
    {0x0000, NULL},         // rest
    {N1, NULL},             // single note
    {N1|N3|N5, ""},         // major
    {N1|N3d|N5, "m"},       // minor
    {N1|N4|N5, "sus4"},     // sus4 (suspended) same as base+5 sus2
    {N1|N5, "5"},           // 5 (no 3rd)
    {N1|N3, "3"},           // no 5th
    {N1|N3d, "m3"},         // minor no 5th
    {N1|N2|N5, "sus2"},     // sus2 (minor minor) same as base-5 sus4
    {N1|N3|N5|N7, "maj7"},  // major 7th
    {N1|N3d|N5|N7d, "m7"},  // minor 7th
    {N1|N3d|N5|N7, "mMaj7"}, // minor major-7th
    {N1|N2, "2"},           // sus2 (minor minor)
    {N1|N2|N3|N5, "+2"},    // 2
    {N1|N3|N5|N6, "+6"},    // 6th: same as base-3 m7
    {N1|N3|N5|N7d, "7"},    // 7th
    {N1|N5|N7d, "57"},      // 7th (-3rd)
    {N1|N5|N7, "5maj7"},    // major 7th (-3rd)
    {N1|N3|N7d, "37"},      // 7th (-5th)
    {N1|N3d|N7d, "3m7"},    // minor 7th (-5th)

    {N1|N3|N5a, "aug"},     // Augmented 5th
    {N1|N3d|N5a, "maug"},   // Minor Augmented 5th
    {N1|N5a, "5aug"},       // Augmented 5th (-3rd): same as base+8 no 5th
    {N1|N3|N5a|N7d, "aug7"}, // Augmented 5th 7th
    {N1|N3|N5a|N7, "augMaj7"}, // Augmented 5th major 7th

    {N1|N3|N5d, "b5"},           // Dimished 5th
    {N1|N3d|N5d, "mb5"},         // Minor Dimished 5th
    {N1|N5d, "5b5"},             // Dimished 5th (-3rd)
    {N1|N3|N5d|N7d, "7b5"},      // Dimished 5th 7th
    {N1|N3d|N5d|N7d, "m7b5"},    // Minor Dimished 5th 7th  (half dimished)
    {N1|N3d|N5d|N7, "mMaj7b5"},  // Minor Diminidhed 5th major 7th
    {N1|N2|N5d, "sus2 5-"},      // sus2 Diminshed 5th

    //{n1|n3d|n5d|n7d, "dim"}, ???????????????????? half dim???

    {N1|N5|N6, "56"}      // 6th (-3rd)
  };

int BitsIn (word Val)
  {
    int Result;
    //
    Result = 0;
    while (Val)
      {
        Val >>= 1;
        Result++;
      }
    return Result;
  }

word FindChordNoteSetOld = -1;
int  FindChordBase = 0;   // Chord finder: Last base used
bool FindChordStepped = false;

int RotateLeft12 (int Mask, int Move)
  {
    while (Move < 0)
      Move += 12;
    while (Move > 12)
      Move -= 12;
    while (Move--)
      {
        Mask <<= 1;
        if (Mask & Bit [12])
          Mask = (Mask | 0x0001) & ~Bit [12];
      }
    return Mask;
  }

int StrFindChordName (char **St, word NoteSet)
  {
    int Base;
    int Chord;
    //_Point NameSize;
    int Res;
    //AnsiString Result;
    //
    //Result = "";
    Res = 0;
    if (NoteSet != FindChordNoteSetOld)
      {
        FindChordNoteSetOld = NoteSet;
        //SetBkMode (Canvas->Handle, TRANSPARENT);
        // Search for matching chord
        Base = 0;//FindChordBase;
        Chord = 0;   // major
        while (true)
          {
            if (RotateLeft12 (ChordPatterns [Chord].NoteSet, Base) == NoteSet)   // Found
              {
                if (ChordPatterns [Chord].Name)
                  {
                    // Display Chord Name
                    Res += StrCat (St, NoteName [Base]);
                    Res += StrCat (St, ChordPatterns [Chord].Name);
                    FindChordBase = Base;
                  }
                break;
              }
            // Next Chord then next Base Note
            Base++;
            if (Base >= 12)   // Not found and base
              {
                Base = 0;
                Chord++;
                if (Chord >= ArraySize (ChordPatterns))   // Not found
                  {
                    Res += StrCat (St, "??");
                    break;
                  }
              }
          }
      }
    /*NameSize = TextExtent (Result);
    if (FindChordStepped)
      {
        y += NameSize.cy;
        FindChordStepped = false;
      }
    else
      if (NameSize.cx >= NoteWidth)
        FindChordStepped = true;
    Canvas->TextOutA (x, y, Result);*/
    return Res;
  }

void TimeToBars (_Riff *Riff, int Time, int *Bars, int *milliBeats)
  {
    //int Index;
    char *p;
    int a, t;
    //
    *Bars = 0;
    //Index = 1;
    StrError = false;
    p = Riff->TimeSigA;
    while (true)
      {
        StepSpace (&p);
        if (*p == 0)
          p = Riff->TimeSigA;;
        a = StrGetNum (&p);
        if (a == 0)
          a = 1;
        if (StrError)
          break;
        t = 1024 * a / Riff->TimeSigB;
        if (Time < t)
          break;
        (*Bars)++;
        Time -= t;
      }
    if (milliBeats)
      *milliBeats = Time;
  }

/*bool TimeSigAme (_Riff *Riff, int Time1, int Time2)
  {
    int b1, b2;
    //
    TimeToBars (Riff, Time1, &b1, NULL);
    TimeToBars (Riff, Time2, &b2, NULL);
    return (b1 == b2);
  }*/

void Insert (void)
  {
    int x;
    //
    ChordsSizeCheck (RiffCur, RiffCur->ChordsSize + 1);
    for (x = RiffCur->ChordsSize - 1; x >= RiffCur->x1; x--)
      RiffCur->Chords [x] = RiffCur->Chords [x - 1];
    ChordInit (&RiffCur->Chords [RiffCur->x1]);
    RiffCur->Chords [RiffCur->x1].duration = RiffDur;
  }

int ValueToStr (char **St, int Val)
  {
    if (Val == None)
      return StrCat (St, '-');
    return IntToStr (St, Val);
  }

int StrGetValue (char **St)
  {
    StepSpace (St);
    if (**St == '-' && (*St) [1] < ' ')
      {
        (*St)++;
        return None;
      }
    return StrGetNum (St);
  }

int NoteToStr (char **St, _Chord * Note)
  {
    int l;
    int i;
    /////////
    l = 0;
    for (i = 0; i < MaxStrings; i++)
      {
        l += ValueToStr (St, Note->fret [i]);
        l += StrCat (St, '\t');
      }
    l += NumToStr (St, Note->duration);
    l += StrCat (St, '\t');
    l += NumToStr (St, Note->flags);
    return l;
  }

void StrToTNote (char **St, _Chord *Note)
  {
    int i;
    /////////
    for (i = 0; i < MaxStrings; i++)
      Note->fret [i] = StrGetValue (St);
    Note->duration = StrGetValue (St);
    Note->flags = StrGetValue (St);
  }

int RiffPropertiesToString (char **St, _Riff *Riff)
  {
    int Res, n;
    //
    Res = 0;
    for (n = 0; n < MaxStrings; n++)
      {
        Res += ValueToStr (St, Riff->Strings [n]);
        Res += StrCat (St, '\t');
      }
    Res += NumToStr (St, Riff->Tempo);
    Res += StrCat (St, '\t');
    Res += StrCat (St, Riff->TimeSigA);
    Res += StrCat (St, '\t');
    Res += NumToStr (St, Riff->TimeSigB);
    Res += StrCat (St, '\t');
    Res += NumToStr (St, Riff->Key);
    return Res;
  }

void StringToRiffProperties (char **St, _Riff *Riff)
  {
    int i;
    char *St_;
    //
    for (i = 0; i < MaxStrings; i++)
      {
        Riff->Strings [i] = StrGetValue (St);
        (*St)++;   // Skip tab
      }
    Riff->Tempo = StrGetNum (St);
    (*St)++;   // Skip tab
    St_ = StrPos (*St, '\t');
    StrAssign (&Riff->TimeSigA, StrMake (*St, St_ - *St));
    *St = St_ + 1;   // Skip tab
    Riff->TimeSigB = StrGetNum (St);
    (*St)++;   // Skip tab
    Riff->Key = StrGetNum (St);
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PROJECT LOAD / SAVE
//
////////////////////////////////////////////////////////////////////////////////////////////////////

char *Filename = NULL;

bool ProjectOpen (void)   // Reads the Project
  {
    int f, n;
    char *Buffer;
    int BufferSize;
    char Type;
    int NumRiff, NumSynths;
    char *Str, *Str_, *Str__, *Str___;
    bool OK;
    ////////////////////////////////////////////////////////////////////////////
    OK = false;
    StrAddExtension (&Filename, ".ntp");
    f = FileOpen (Filename, foRead);
    if (f >= 0)
      {
        // Clear Project
        StrAssign (&RiffNames, NULL);
        StrAssign (&SynthNames, NULL);
        StrAssign (&Memo, NULL);
        RiffsInit ();
        SynthsInit ();
        ProjectInit ();
        // Read file into Buffer
        BufferSize = FileSize (f);
        Buffer = (char *) malloc (BufferSize + 1);
        FileRead (f, (byte *) Buffer, BufferSize);
        Buffer [BufferSize] = 0;   // "terminate" Buffer
        FileClose (f);
        // Go through Buffer
        NumRiff = 0;
        NumSynths = 0;
        NumProjectItems = 0;
        n = 0;
        Str = Buffer;
        while (true)
          {
            if (*Str == 0)   // end of buffer
              break;
            Str_ = StrPos (Str, '\n');   // break off a line
            if (Str_)
              {
                *Str_ = 0;   // Terminate line
                if (Str_ [-1] == '\r')   // remove cr
                  Str_ [-1] = '\0';
                Str_++;
              }
            Type = *Str++;
            if (IsAlpha (Type) && *Str++ == '\t')
              // Riff Descriptor
              if (Type == 'T' && NumRiff < MaxRiffs)
                {
                  Str__ = StrPos (Str, '\t');
                  if (Str__)
                    {
                      Str___ = StrMake (Str, Str__ - Str);
                      StrSetItem (&RiffNames, NumRiff, Str___);
                      free (Str___);
                      Str = Str__ + 1;   // step Name and tab
                      StringToRiffProperties (&Str, &Riffs [NumRiff]);
                      NumRiff++;
                      n = 0;
                    }
                }
              // Note within a Riff
              else if (Type == 'N')
                {
                  ChordsSizeCheck (&Riffs [NumRiff - 1], n + 2);
                  StrToTNote (&Str, &Riffs [NumRiff - 1].Chords [n]);
                  n++;
                }
              // Synthesizer (Instrument)
              else if (Type == 'I' && NumSynths < MaxSynths)
                {
                  Str__ = StrPos (Str, '\t');
                  if (Str__)
                    {
                      Str___ = StrMake (Str, Str__ - Str);
                      StrSetItem (&SynthNames, NumSynths, Str___);
                      free (Str___);
                      Str = Str__ + 1;   // step Name and tab
                      StrToSynth (&Str, &Synths [NumSynths]);
                      NumSynths++;
                    }
                }
              // Project Elements
              else if (Type == 'S')   // Project - New format
                {
                  if (NumProjectItems + 1 < MaxProject)
                    StrAssignCopy (&Project [NumProjectItems++], Str);
                }
              // Notes Item
              else if (Type == 'X')
                {
                  StrAdd (&Memo, Str);
                  StrAdd (&Memo, "\n");
                }
            Str = Str_;   // Next Line
          }
        FileClose (f);
        OK = true;
        Changed = false;
      }
    return OK;
  }

bool ProjectSave (void)   // Saves the Project
  {
    int File;
    char *s0, *s1, *s2;
    int Rif, Note, Syn, Str;
    int Len;
    char *St, *s;
    ////////////////////////////////////////////////////
    FileBackup (Filename);
    File = FileOpen_ (Filename, foWrite);
    if (File >= 0)
      {
        // Save Riffs
        s0 = NULL;
        StrAssignCopy (&s0, RiffNames);
        s1 = s0;
        Rif = 0;
        while (true)
          {
            s2 = StrPos (s1, '\t');
            if (s2)
              *s2++ = 0;
            s = St = NULL;
            Len = 0;
            while (true)
              {
                Len += StrCat (&s, "T\t");
                Len += StrCat (&s, s1);
                Len += StrCat (&s, '\t');
                Len += RiffPropertiesToString (&s, &Riffs [Rif]);
                if (St == NULL)
                  St = s = (char *) malloc (Len + 1);
                else
                  break;
              }
            *s = 0;
            FileWriteLine (File, St);
            free (St);
            //
            Note = 0;
            while (true)
              {
                ChordsSizeCheck (&Riffs [Rif], Note + 1);
                if (Riffs [Rif].Chords [Note].duration == None)
                  break;
                s = St = NULL;
                Len = 0;
                while (true)
                  {
                    Len += StrCat (&s, "N\t");
                    Len += NoteToStr (&s, &Riffs [Rif].Chords [Note]);
                    if (St == NULL)
                      St = s = (char *) malloc (Len + 1);
                    else
                      break;
                  }
                *s = 0;
                FileWriteLine (File, St);
                free (St);
                Note++;
              }
            if (s2 == NULL)   // no more
              break;
            s1 = s2;   // Next Rif
            Rif++;
          }
        free (s0);
        // Save Synthesizers
        s0 = SynthNames;
        Syn = 0;
        while (true)
          {
            St = s = NULL;
            Len = 0;
            s1 = StrPos (s0, '\t');
            while (true)
              {
                Len += StrCat (&s, "I\t");
                if (s1)
                  Len += StrCat (&s, s0, s1 - s0);
                else
                  Len += StrCat (&s, s0);
                Len += StrCat (&s, '\t');
                Len += SynthToStr (&s, &Synths [Syn]);
                if (St == NULL)
                  St = s = (char *) malloc (Len + 1);
                else
                  break;
              }
            *s = 0;
            FileWriteLine (File, St);
            free (St);
            if (!s1)
              break;
            s0 = s1 + 1;
            Syn++;
          }
        // Save Project
        for (Str = 0; Str < NumProjectItems; Str++)
          {
            St = NULL;
            StrAdd (&St, "S\t");
            StrAdd (&St, Project [Str]);
            FileWriteLine (File, St);
            free (St);
          }
        // Save Notes
        s1 = Memo;
        while (s1 && *s1)
          {
            St = NULL;
            StrAdd (&St, "X\t");
            s2 = StrPos (s1, '\n');
            if (s2 == NULL)
              StrAdd (&St, s1);
            else
              StrAdd (&St, s1, s2 - s1);
            FileWriteLine (File, St);
            free (St);
            s1 = s2;
            if (s1)
              s1++;
          }
        // Done: Close
        FileClose (File);
        Changed = false;
        return true;
      }
    return false;
  }

void Copy (_Form *Form)
  {
    char *Clip, *c;
    int Len;
    int x1, x2;
    ////////
    Clip = c = NULL;
    Len = 0;
    while (true)
      {
        x1 = Min (RiffCur->x1, RiffCur->x2);
        x2 = Max (RiffCur->x1, RiffCur->x2);
        while (x1 <= x2)
          {
            if (RiffCur->Chords [x1].duration != None)
              {
                Len += StrCat (&c, "N\t");
                Len += NoteToStr (&c, &RiffCur->Chords [x1]);
                Len += StrCat (&c, '\n');
              }
            x1++;
          }
        if (c)
          break;
        Clip = c = (char *) malloc (Len + 1);
      }
    *c = 0;
    ClipboardSet (Form->Window, Clip);
    free (Clip);
  }

void Paste (_Form *Form)
  {
    int i;
    char *Clipboard, *Clip, *Clip_;
    _Chord Note;
    bool OK;
    ////////////////////////////////////////////////////////
    if (ClipboardGet (Form->Window))
      {
        Clipboard = NULL;
        StrAssignCopy (&Clipboard, ClipboardData);
        Clip = Clipboard;
        OK = true;
        i = 0;
        while (true)
          {
            Clip_ = StrPos (Clip, '\n');
            if (Clip_)
              *Clip_++ = '\0';
            if (StrMatch (&Clip, "N\t"))
              {
                Insert ();
                StrToTNote (&Clip, &Note);
                RiffCur->Chords [RiffCur->x1] = Note;
                RiffCur->x1++;
              }
            else
              OK = false;
            if (Clip_ == NULL || *Clip_ == '\0' || !OK)
              break;
            Clip = Clip_;
            i++;
          }
        if (!OK)
          Beep ();
        free (Clipboard);
        Changed = true;
      }
  }

void Delete (void)
  {
    int x1, x2, n;
    ////////
    x1 = RiffCur->x1;
    x2 = RiffCur->x2;
    if (x1 > x2)
      SwapBytes (&x1, &x2, sizeof (x1));
    while (x1 < RiffCur->ChordsSize)
      {
        if (x2 + 1 < RiffCur->ChordsSize)
          RiffCur->Chords [x1] = RiffCur->Chords [x2 + 1];
        else
          ChordInit (&RiffCur->Chords [x1]);
        x1++;
        x2++;
      }
    n = NotesInRiff (RiffCur);
    if (RiffCur->x1 >= n)
      RiffCur->x1 = n;
    if (RiffCur->x2 >= n)
      RiffCur->x2 = n;
    RiffCur->x2 = RiffCur->x1;
    Changed = true;
  }

/*
//short *SoundBuffer = NULL;
void *SoundHandle = NULL;
int SoundBufferSize;

int SoundPlayThread (void *Param)
  {
    //
    SoundHandle = SoundOpen ();
    SoundPlay (SoundHandle, (short *) Param, SoundBufferSize);
    SoundHandle = NULL;
    free (Param);
    //SoundBuffer = NULL;
    return 0;
  }
*/

//void *SoundHandle = NULL;

void SoundPlay_ (short *Buffer, int BufferSize)
  {
    //SoundHandle = SoundOpen ();
    SoundPlay (Buffer, BufferSize, true);
    //if (SoundHandle == NULL)
    //  {
        //if (SoundBuffer)
        //  free (SoundBuffer);
        //SoundBuffer = Buffer;
        //SoundBufferSize = BufferSize;
        //StartThread (SoundPlayThread, Buffer);
        //usleep (1000);
    //  }
    //else
    //  free (Buffer);
  }

void SoundStop_ (void)
  {
    //if (SoundHandle)
    //  SoundStop (SoundHandle);
    SoundStop ();
  }

const int PlayTrail = 44100 * 2;

void PlayNote (int Semi)
  {
    short *block;
    int l;
    ///////////////////////////////////////////////////////
    //if (OptionsForm->cbHearPreviewNote->Checked)
    if (SynthTab >= 0)
      {
        l = NoteLength (RiffCur->Tempo, Duration1024 [RiffDur]) + PlayTrail;   // samples
        block = (short *) malloc (l * 8);
        memset (block, 0, l * 8);
        NoteBuffer (MasterVolume, RiffCur->Tempo, Semi, Duration1024 [RiffDur], &Synths [SynthTab], block, 0, l * 4);
        SoundPlay_ (block, l);   // 16 bits x 2 channels
      }
  }

int TimeInNoteDotted (_Chord **Chord)
  {
    int Res;
    bool Tie;
    int i;
    //
    Res = 0;
    while (true)
      {
        Res += TimeInNote (*Chord);
        Tie = false;
        for (i = 0; i < MaxStrings; i++)
          if ((*Chord)->fret [i] != None)
            Tie = (*Chord)->flags & fTie;
        (*Chord)++;
        if (!Tie)   // not Tied
          break;
        if ((*Chord)->duration == None)
          break;
      }
    return Res;
  }

char *RenderError = NULL;
int RiffBufferTranspose = 0;

int RiffBuffer (_Riff *Riff, int Start, _Synth *Synth, short *Buffer, int BufferIndex, int BufferSize, bool Enabled)   // returns size in short
  {
    _Chord *Note, *Note_;
    int Semi, Dur;
    int i, j, k;
    ////////////////////////////////////////////////////////////////////////////
    Note = &Riff->Chords [Start];
    while (Note && Note->duration != None)
      {
        Note_ = Note;
        Dur = TimeInNoteDotted (&Note);
        j = 0;
        if (Buffer)
          for (i = 0; i < 8; i++)
            if (Note_->fret [i] != None)
              {
                Semi = Riff->Strings [i] + Note_->fret [i];
                k = 0;
                if (Synth->Dither)   // Randomly shuffle note about
                  k = 2 * (rand () % (Synth->Dither * 50)) - Synth->Dither * 50;   // must be even
                if (Enabled)
                  NoteBuffer (MasterVolume, Riff->Tempo, Semi + RiffBufferTranspose, Dur, Synth, Buffer, BufferIndex + j + k, BufferSize);
                j += Synth->Strum * 50;
              }
        BufferIndex += 2 * NoteLength (Riff->Tempo, Dur);
      }
    return BufferIndex;
  }

int ProjectBuffer (int LineStart, int LineEnd, short **Buffer)
  {
    int Line;   // Place in Script to play
    char *Str, *Str_;
    int Synth, SynthCount;
    int Item;
    int Count, Riff;
    //short *block;
    int IndexSectionStart, IndexSectionEnd;
    int IndexScriptMax;
    int Index;
    int l;
    //////////////////////////////////////////
    IndexScriptMax = 0;
    SynthCount = StrCount (SynthNames, '\t') + 1;
    *Buffer = NULL;
    while (true)
      {
        Index = 0;
        Line = LineStart;
        while (Line <= LineEnd)
          {
            if (RenderError)
              break;
            Str = Project [Line];
            if (Str && *Str)
              {
                IndexSectionStart = Index;
                IndexSectionEnd = Index;
                Synth = 0;
                // Transcripted?
                RiffBufferTranspose = 0;
                Str_ = StrGetItem (Str, 0, &l);
                if (l)   // Transpose field not empty
                  if (*Str_ == '[')   // transpose value
                    {
                      Str_++;
                      RiffBufferTranspose = StrGetNum (&Str_);
                    }
                //Str = StrGetItem (Str, 1, &l);
                Item = 1;
                while (true)
                  {
                    // Get Count (if present)
                    Count = 1;
                    Str_ = StrGetItem (Str, Item++, &l);
                    if (l)
                      Count = StrGetNum (&Str_);
                    // Get Riff
                    Riff = -1;
                    Str_ = StrGetItem (Str, Item++, &l);
                    if (l)
                      {
                        Riff = StrFindItem (&RiffNames, Str_);
                        if (Riff < 0)
                          {
                            StrAdd (&RenderError, "\ab\a1Error! Undefined Riff - ");
                            StrAdd (&RenderError, Str_, l);
                            StrAdd (&RenderError, "\ab");
                            DebugAdd (RenderError); // ####
                          }
                      }
                    // Place in buffer
                    if (Riff >= 0 && Count > 0)
                      {
                        Index = IndexSectionStart;
                        while (Count--)
                          Index = RiffBuffer (&Riffs [Riff], 0, &Synths [Synth], *Buffer, Index, IndexScriptMax, !Synths [Synth].Mute);
                        if (Index > IndexSectionEnd)
                          IndexSectionEnd = Index;
                        if (Index > IndexScriptMax)
                          IndexScriptMax = Index;
                      }
                    // Next Synthesizer
                    if (Synth++ >= SynthCount)
                      break;
                  }
                Index = IndexSectionEnd;
              }
            Line++;
          }
        if (*Buffer)
          break;
        IndexScriptMax += PlayTrail;
        *Buffer = (short *) malloc (IndexScriptMax * 2);
        memset (*Buffer, 0, IndexScriptMax * 2);
      }
    return IndexScriptMax;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SCORE VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

const _Point Cleff [] =
  {
    {-4, -32},
    {-8, -30},
    {-7, -24},
    {0, -22},
    {6, -23},
    {9, -30},
    {7, -35},
    {2, -38},
    {-3, -39},
    {-8, -38},
    {-14, -34},
    {-17, -27},
    {-16, -20},
    {-12, -15},
    {-5, -9},
    {1, -3},
    {5, 4},
    {4, 12},
    {0, 17},
    {-3, 11},
    {-5, 4},
    {-5, -3},
    {4, -46},
    {4, -50},
    {0, -54},
    {-4, -55},
    {-9, -54},
    {-11, -50},
    {-10, -46},
    {-7, -45},
    {-5, -46},
    {-4, -49},
    {-6, -52},
    {-8, -52},
    {-9, -50},
    {-8, -48},
    {-6, -48},
    {IntMax, IntMax}
  };

const _Point QuarterRest [] =
  {
    {0, -4},
    {9, -13},
    {3, -19},
    {4, -22},
    {11, -28},
    {4, -27},
    {2, -30},
    {4, -34},
    {0, -32},
    {-1, -30},
    {-1, -26},
    {2, -24},
    {4, -24},
    {-1, -17},
    {6, -10},
    {0, -4},
    {IntMax, IntMax}
  };

class _Score: public _Container
  {
    public:
      int StaveSpacing;
      int TabSpacing;
      int NoteWidth;
      int StaveOffset;
      int TabOffset;
      int xOffset;
      int LineWidth;
      bool TabFocus;
      int ColourLine;
      int ChordNameOffset;
      //
      void SetSizes (void);
      int TabPosn (_Riff *Riff, int String);
      int PosnToString (_Riff *Riff, int Posn);
      int StavePosn (int Note);
      int StaveHeight (_Riff *Riff);
      void DrawLine2 (int x1, int y1, int x2, int y2);
      void Polygon (_Point Origin, int Scale, _Point *Poly, int Width);
      void DrawAccidental (int x, int y, TActType ActType);
      void DrawKeySignature (_Riff *Riff, int x0, int y0);
      void DrawStave (_Riff *Riff, int *Chord, int *Time, int x1, int x2, int y0);
      void DrawDots (_Chord *Chord, int x0, int y0);
      void DrawRest (byte duration, int x, int y);
      void DrawChord (_Riff *Riff, _Chord *Chord, int x0, int y0, int *beam);
      void AutoHorizScroll (void);
      bool ProcessEventKeysCommon (int Key);
      //
      _Score (_Container *Parent, _Rect Rect);
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
  };

_Score::_Score (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    FontSet (NULL, 0);
    TabIn = true;
    TabOut = false;
    TabFocus = false;
  }

void _Score::SetSizes (void)
  {
    int Size;
    //
    Size = FontFind ()->YAdvance;
    //Size = Size * 8 / 10;
    StaveSpacing = Size * 2 / 3;
    TabSpacing = Size;
    NoteWidth = Size * 2; // 24
    StaveOffset = Size * 4; //5; // 65
    TabOffset = Size * 12; // 150
    xOffset = Size * 8; // 90
    LineWidth = Size / 12;
    if (LineWidth == 0)
      LineWidth = 1;
  }

int _Score::TabPosn (_Riff *Riff, int String)
  {
    int y;
    int s;
    //
    y = TabOffset;
    s = MaxStrings;
    while (true)
      {
        s--;
        if (s == String)
          break;
        if (s < 0)
          break;
        if (Riff->Strings [s] != None)
          y += TabSpacing;
      }
    return y;
    //return (TabOffset + (MaxStrings - String - 1) * TabSpacing);
  }

int _Score::PosnToString (_Riff *Riff, int Posn)
  {
    int s;
    //
    s = 0;
    while (true)
      {
        if (s >= MaxStrings)
          return -1;
        if (Abs (Posn - TabPosn (Riff, s)) < TabSpacing)
          return s;
        s++;
      }
  }

int _Score::StavePosn (int Note)
  {
    return (10 - Note) * StaveSpacing / 2 + StaveOffset;     ///*- StaveSpacing / 2*/
  }

int _Score::StaveHeight (_Riff *Riff)
  {
    return TabOffset + (MaxStrings + 3) * TabSpacing;
  }

void _Score::DrawLine2 (int x1, int y1, int x2, int y2)
  {
    DrawLine (x1, y1, x2, y2, ColourLine);
    DrawLine (x1+1, y1, x2+1, y2, ColourLine);
  }

void _Score::Polygon (_Point Origin, int Scale, _Point *Poly, int Width)
  {
    _Point *a;
    //
    a = Poly++;
    while (Poly->x != IntMax)
      {
        DrawLine (Origin.x + a->x * Scale / 10, Origin.y - a->y * Scale / 10, Origin.x + Poly->x * Scale / 10, Origin.y - Poly->y * Scale / 10, ColourLine, Width);
        a = Poly++;
      }
  }

void _Score::DrawAccidental (int x, int y, TActType ActType)
  {
    y -= StaveSpacing / 2;   // bring up from center
    switch (ActType)
      {
        case aNone:
          break;
        case aSharp:
          //Vertical Lines
          DrawLine2 (x + (StaveSpacing / 4), y, x + (StaveSpacing / 4), y + (StaveSpacing));
          DrawLine2 (x + (2 * StaveSpacing / 3), y, x + (2 * StaveSpacing / 3), y + (StaveSpacing));
          //Horizontal Lines
          DrawLine2 (x, y + (StaveSpacing / 4), x + StaveSpacing, y + (StaveSpacing / 4));
          DrawLine2 (x, y + (3 * StaveSpacing / 4), x + StaveSpacing, y + (3 * StaveSpacing / 4));
          break;
        case aFlat:
          DrawLine2 (x + (StaveSpacing / 4), y, x + (StaveSpacing / 4), y + (StaveSpacing));
          DrawLine2 (x + (StaveSpacing / 4), y + StaveSpacing / 2, x + (3 * StaveSpacing / 4), y + StaveSpacing / 2);
          DrawLine2 (x + (3 * StaveSpacing / 4), y + StaveSpacing / 2, x + (StaveSpacing / 4), y + StaveSpacing);
          break;
        case aNatural:
          // Verticals
          DrawLine2 (x + (StaveSpacing / 4), y, x + (StaveSpacing / 4), y + (3 * StaveSpacing / 4));
          DrawLine2 (x + (3 * StaveSpacing / 4), y + StaveSpacing / 4, x + (3 * StaveSpacing / 4), y + StaveSpacing);
          // Horizontals
          DrawLine2 (x + (StaveSpacing / 4), y + StaveSpacing / 4, x + (3 * StaveSpacing / 4), y + StaveSpacing / 4);
          DrawLine2 (x + (StaveSpacing / 4), y + 3 * StaveSpacing / 4, x + (3 * StaveSpacing / 4), y + 3 * StaveSpacing / 4);
          break;
      }
  }

int KeyPosSharp [] = {10, 7, 11, 8, 5, 9};
int KeyPosFlat [] = {6, 9, 5, 8, 4, 7};

void _Score::DrawKeySignature (_Riff *Riff, int x0, int y0)
  {
    int Key;
    //
    // Sharps
    for (Key = 0; Key < Riff->Key; Key++)
      {
        DrawAccidental (x0, y0 + StavePosn (KeyPosSharp [Key]), aSharp);
        x0 += NoteWidth / 6;
      }
    // Flats
    for (Key = 0; Key > Riff->Key; Key--)
      {
        DrawAccidental (x0, y0 + StavePosn (KeyPosFlat [-Key]), aFlat);   //    aNatural
        x0 += NoteWidth / 6;
      }
  }

int TimeToStr (char **St, _Riff *Riff, int Time)
  {
    int n0, n1, n2;
    int Res;
    ///////////////////////////////////////////
    Res = 0;
    TimeToBars (Riff, Time, &n0, &n1);
    // Display whole bar number
    if ((n0 > 0) || (Time == 0))
      Res += NumToStr (St, n0);
    if (n1)
      {
        // Simplify fraction n1 / 1024 => n1 / n2
        n2 = 1024;
        while ((n1 > 0) && ((n1 % 2) == 0))
          {
            n1 >>= 1;
            n2 >>= 1;
          }
        // Display as fraction
        Res += StrCat (St, "\a-");
        Res += NumToStr (St, n1);
        Res += StrCat (St, '/');
        Res += NumToStr (St, n2);
        Res += StrCat (St, "\a+");
      }
    return Res;
  }

void _Score::DrawStave (_Riff *Riff, int *Chord, int *Time, int x1, int x2, int y0)
  {
    int j, k, x, y;
    int xx;
    char St [16], *s;
    int beam;
    _Font *Font_;
    int fs;
    //
    Font_ = FontFind ();
    fs = Font_->Size;
    //
    ChordNameOffset = 0;
    xx = x1;
    // Draw Cleff
    ColourLine = cBlueDark;
    Polygon ({x1 + 2 * StaveSpacing, y0 + StaveOffset + StaveSpacing / 2}, StaveSpacing, (_Point *) Cleff, LineWidth * 2);
    // Draw Tempo
    s = St;
    StrCat (&s, "\eM=");
    NumToStr (&s, Riff->Tempo);
    *s = 0;
    TextOutAligned ({x1, y0 + 8, 99, 99}, St, aLeft, aTop);
    xx += NoteWidth;// + StaveSpacing;
    // Draw Stave lines
    for (j = 0; j < 5; j++)
      DrawLine (x1, y0 + StaveOffset + j * StaveSpacing, x2, y0 + StaveOffset + j * StaveSpacing, cBlue);
    // Draw TAB lines
    Font_->Colour = cGreenDark;
    FontSizeSet (Font_, fs * 2 / 3);
    for (k = 0; k < MaxStrings; k++)
      {
        y = y0 + TabPosn (Riff, k);
        if (Riff->Strings [k] != None)
          {
            DrawLine (x1, y, x2, y, cGreen);
            TextOutAligned ({x1 + NoteWidth, y - fs, 99, 2 * fs}, (char *) NoteName [(Riff->Strings [k] + 120) % 12], aLeft, aCenter);
          }
      }
    FontSizeSet (Font_, fs);
    Font_->Style |= fsBold;
    y = y0 + TabOffset;
    TextOutAligned ({x1+2, y, 99, 99}, "T", aLeft, aTop); y += TabSpacing;
    TextOutAligned ({x1+2, y, 99, 99}, "A", aLeft, aTop); y += TabSpacing;
    TextOutAligned ({x1+2, y, 99, 99}, "B", aLeft, aTop); y += TabSpacing;
    // Left end line
    DrawLine (x1, y0 + StaveOffset, x1, y0 + TabPosn (Riff, 0), cGrayDark);
    // Draw Key Signature
    DrawKeySignature (Riff, xx, y0);
    xx += NoteWidth;
    // Draw Time Signature
    Font_->Colour = cBlueDark;
    FontSizeSet (Font_, fs * 7 / 4);
    TextOutAligned ({xx, y0 + StaveOffset - StaveSpacing / 2, 99, 99}, Riff->TimeSigA, aLeft, aTop);
    s = St;
    NumToStr (&s, Riff->TimeSigB);
    *s = 0;
    TextOutAligned ({xx, y0 + StaveOffset - StaveSpacing / 2 + 2 * StaveSpacing, 99, 99}, St, aLeft, aTop);
    FontSizeSet (Font_, fs);
    Font_->Style &= ~fsBold;
    xx = x1 + xOffset;
    // Draw Notes + TAB
    FindChordNoteSetOld = -1;
    beam = 0;
    Font_->Colour = cGreenDark;
    while (true)
      {
        // are we done?
        if (*Chord >= Riff->ChordsSize)
          break;
        if (Riff->Chords [*Chord].duration == None)
          break;
        if (xx >= x2)
          break;
        // Draw Bar line if required
        j = *Time;
        *Time = *Time + Duration1024 [Riff->Chords [*Chord].duration];
        if (Riff->Chords [*Chord].flags & fDotted)
          *Time += Duration1024 [Riff->Chords [*Chord].duration] >> 1;
        else if (Riff->Chords [*Chord].flags & fDotDotted)
          *Time += (Duration1024 [Riff->Chords [*Chord].duration] >> 1) + (Duration1024 [Riff->Chords [*Chord].duration] >> 2);
        int b1, b2;
        int f1, f2;
        TimeToBars (Riff, j, &b1, &f1);
        TimeToBars (Riff, *Time, &b2, &f2);
        if (b1 != b2)   // New Bar
          {
            x = xx + NoteWidth / 2; //xOffset + *Chord * NoteWidth + (NoteWidth - (NoteWidth >> 2));
            DrawLine (x, y0 + StaveOffset, x, y0 + TabPosn (RiffCur, 0), f2 ? cRed : cGrayDark);
            Font_->Colour = ColourAdjust (cGray, 35);
            s = St;
            TimeToStr (&s, Riff, *Time);
            *s = 0;
            TextOutAligned ({x - StaveSpacing / 2, y0 + StaveOffset - 2 * StaveSpacing, 99, 99}, St, aLeft, aTop);

          }
        // Draw Notes / Rests and TAB
        Font_->Colour = cGreenDark;
        if (beam == 0)
          {
            if (BeamOK (*Time, Riff->Chords [*Chord].duration))   // note within a same sized beat
              if (*Chord + 1 < Riff->ChordsSize)   // not last note
                if (xx + NoteWidth < x2)   // can fit next note
                  if (Riff->Chords [*Chord].duration == Riff->Chords [*Chord + 1].duration)   // next note same length
                    if (Riff->Chords [*Chord].flags == Riff->Chords [*Chord + 1].flags)   // with same attributes
                      if (!Rest (&Riff->Chords [*Chord + 1]))   // and not a rest
                        beam = 1;
          }
        else if (beam == 1)
          beam = 2;
        DrawChord (Riff, &Riff->Chords [*Chord], xx, y0, &beam); // was xOffset + *Chord * NoteWidth
        if (beam == 2)
          beam = 0;
        // Go onto next note
        xx += NoteWidth;
        (*Chord)++;
      }
    Font_->Style &= ~fsBold;
    Font_->Colour = -1;
  }

void _Score::DrawDots (_Chord *Chord, int x0, int y0)
  {
    int r;
    //
    if (Chord->flags & (fDotted | fDotDotted))
      {
        r = StaveSpacing / 2;
        DrawCircle (x0 + 2 * r, y0, r / 2, cRed, cRed);
        if (Chord->flags & fDotDotted)
          DrawCircle (x0 + 3 * r, y0, r / 2, cRed, cRed);
      }
  }

void _Score::DrawRest (byte duration, int x, int y)
  {
    int y1, y2;
    int r;
    //////////////////////////////
    r = StaveSpacing / 2;
    switch (duration)
      {
        case 0: // 2/1
                y1 = y + StavePosn (8);
                DrawRectangle ({x - r, y1, StaveSpacing, StaveSpacing}, -1, -1, cBlack);
                break;
        case 1: // 1/1
                y1 = y + StavePosn (8);
                DrawRectangle ({x - r, y1, StaveSpacing, StaveSpacing / 2 + 1}, -1, -1, cBlack);
                break;
        case 2: // 1/2
                y = y + StavePosn (7);
                DrawRectangle ({x - r, y, StaveSpacing, StaveSpacing / 2 + 1}, -1, -1, cBlack);
                break;
        case 3: // 1/4
                ColourLine = cBlack;
                Polygon ({x, y + StaveOffset + StaveSpacing / 2}, StaveSpacing, (_Point *) QuarterRest, LineWidth * 2);
                break;
        default: // 1/8, 1/16, 1/32, 1/65
                y1 = y + StavePosn (10) + StaveSpacing / 4;
                while (duration >= 4)
                  {
                    r = StaveSpacing / 4;
                    DrawCircle (x, y1 + r, r, -1, cBlack);
                    y2 = y1 + StaveSpacing / 2;
                    DrawLine (x, y2, x + StaveSpacing / 2, y2, cBlack, LineWidth);
                    DrawLine (x + (StaveSpacing / 3) + StaveSpacing / 2, y1, x + (5 * StaveSpacing / 12), y1 + (2 * StaveSpacing), cBlack, LineWidth);
                    y1 += StaveSpacing;
                    x -= StaveSpacing / 4;
                    duration--;
                  }
                break;
      }
  }

void _Score::DrawChord (_Riff *Riff, _Chord *Chord, int x0, int y0, int *beam)
  {
    int Min, Max;
    int i, n, y, xStem, StemDir;
    int r;
    TActType Act;
    int a;
    int c;
    int delta;
    static int beamX = 0;
    static int beamY = 0;
    static int beamStemDir = 0;
    word NoteSet;
    char St [32], *s;
    ////////////////////////////////////////////////////////////////////////////
    //draw note ellipse/s
    Min = IntMax; //MAXINT;
    Max = -99999; //MININT;
    r = StaveSpacing / 2;
    NoteSet = 0;
    for (i = 0; i < MaxStrings; i++)
      if (Riff->Strings [i] != None)
        {
          if (Chord->fret [i] != None)
            {
              // Draw TAB
              y = y0 + TabPosn (Riff, i);
              FontFind ()->Colour = cGreenDark;
              s = St;
              NumToStr (&s, Chord->fret [i]);
              *s = 0;
              TextOutAligned ({x0 - NoteWidth, y - TabSpacing / 2, 2 * NoteWidth, 99}, St, aCenter, aTop);
              // Draw Notation Chord
              n = Riff->Strings [i] + Chord->fret [i];
              NoteSet |= Bit [(n + 100 * 12) % 12];
              n = MapSemiToneToTone (Riff, n, &Act);
              if (n < Min)
                Min = n;
              if (n > Max)
                Max = n;
              y = y0 + StavePosn (n);
              // Ledger Lines
              c = StaveSpacing * 2 / 3;
              while (n < 2 || n > 10)
                {
                  if ((n & 1) == 0)
                    DrawLine (x0 - c, y0 + StavePosn (n),x0 + c, y0 + StavePosn (n), cBlue, LineWidth);
                  Bump (n, 5);
                }
              // Draw Note Base (or for stew's sake: note blob)
              // Draw Accidental
              ColourLine = cBlack;
              DrawAccidental (x0 - 2 * StaveSpacing, y, Act);
              // Draw Oval
              DrawCircle (x0, y, r, cBlack, Chord->duration <= 2 ? -1 : cBlack, LineWidth);
              DrawCircle (x0 + LineWidth, y, r, cBlack, -1, LineWidth);
              // Draw lines for semi-demi-breve (8 beats)
              if (Chord->duration == 0)
                {
                  DrawLine (x0-r-2, y-r, x0-r-2, y+r, cBlack, LineWidth);
                  DrawLine (x0+r+LineWidth+2, y-r, x0+r+LineWidth+2, y+r, cBlack, LineWidth);
                }
              // Draw Dot(s)
              DrawDots (Chord, x0, y);
              // Draw Tie
              if (Chord->flags & fTie)
                DrawLine (x0 + 2 * r, y /*- StaveSpacing*/, x0 + NoteWidth - 2 * r, y /*- StaveSpacing*/, cRed, LineWidth * 2);
            }
        }
    s = St;
    StrFindChordName (&s, NoteSet);
    *s = 0;
    TextOutAligned ({x0 - NoteWidth, y0 + TabPosn (Riff, 0) + ChordNameOffset, 2 * NoteWidth, 2 * TabSpacing}, St, aCenter, aCenter);
    ChordNameOffset = TabSpacing - ChordNameOffset;  // FontFind ()->Size
    if (Min == IntMax)   // No note: draw rest
      {
        DrawRest (Chord->duration, x0, y0);
        DrawDots (Chord, x0, y0 + StaveOffset + 2 * StaveSpacing);
        if (Chord->flags & fTie) // ####
          DrawLine (x0, y0 + StaveOffset, x0 + StaveSpacing, y0 + StaveOffset + 4 * StaveSpacing, cGreen, 2);
        *beam = 0;
      }
    else   // Note exists: Draw Note Stem
      if (Chord->duration >= 2)
        {
          StemDir = (Min > 6) ? -1 : 1;
          if ((*beam == 2) && (Chord->duration >= 4))
            StemDir = beamStemDir;
          if (StemDir == -1) //Draw Stem Down
            {
              y = y0 + StavePosn (Min - 6);
              xStem = x0 - r;
              DrawLine (xStem, y0 + StavePosn (Max), xStem, y, cBlack, LineWidth);
            }
          else //Draw Stem Up
            {
              y = y0 + StavePosn (Max + 6);
              xStem = x0 + r + LineWidth;
              DrawLine (xStem, y, xStem, y0 + StavePosn (Min), cBlack, LineWidth);
            }
          // Quaver Flags
          if (*beam == 0)
            {
              a = Chord->duration - 3;
              while (a-- > 0)
                {
                  DrawLine (xStem, y, xStem + StaveSpacing * 2 / 3, y + StaveSpacing * 2 / 3 * StemDir, cBlack, LineWidth * 2);
                  y += StemDir * StaveSpacing / 2;
                }
            }
          else if (*beam == 1)
            {
              beamX = xStem;
              beamY = y;
              beamStemDir = StemDir;
            }
          else if (*beam == 2)
            {
              a = Chord->duration - 3;
              while (a-- > 0)
                {
                  DrawLine (xStem, y, beamX, beamY, cBlack, LineWidth * 2);
                  delta = StemDir * StaveSpacing / 2;
                  y += delta;
                  beamY += delta;
                  //Canvas->Pen->Width = LineWidth;
                }
              beamX = 0;
              beamY = 0;
              beamStemDir = 0;
            }
        }
  }

#define ScoreGap 4

void _Score::DrawCustom (void)
  {
    int i;
    int Time;
    int cx, cy, cw, ch;
    ////////////////////////////////////////////////////////////////////////////
    SetSizes ();
    // Draw Selection
    if (RiffCur->x1 != RiffCur->x2)
      //DrawRectangle ({ScoreGap + xOffset + Min (RiffCur->x1, RiffCur->x2) * NoteWidth - NoteWidth / 2, 4, (Abs (RiffCur->x1 - RiffCur->x2) + 1) * NoteWidth, TabPosn (RiffCur, 0) + 2 * TabSpacing}, cRed, cRed);
      DrawRectangle ({ScoreGap + xOffset + Min (RiffCur->x1, RiffCur->x2) * NoteWidth - NoteWidth / 2, 0, (Abs (RiffCur->x1 - RiffCur->x2) + 1) * NoteWidth, SizeVirtual.y}, -1, -1, cSelected);
    // Determine Cursor coordinates
    if (TabFocus)
      {
        cx = ScoreGap + xOffset + (RiffCur->x1 * NoteWidth);
        cy = TabPosn (RiffCur, MaxStrings - 1 - RiffY);
        cw = TabSpacing;
        ch = TabSpacing;
      }
    else   // Score Focus
      {
        cx = ScoreGap + xOffset + (RiffCur->x1 * NoteWidth);
        cy = StaveOffset + (RiffY * StaveSpacing / 2);
        cw = StaveSpacing;
        ch = StaveSpacing;
      }
    // Determine paint box width
    SizeVirtual.x = xOffset + (NotesInRiff (RiffCur) + 1) * NoteWidth;
    // Draw the stave and TAB
    i = 0;
    Time = 0;
    DrawStave (RiffCur, &i, &Time, ScoreGap, /*Rect.Width*/ xOffset + RiffCur->ChordsSize * NoteWidth, 0);
    // Draw selected note rectangle
    if (Form->Focused == this)
      DrawRectangle ({cx - cw/2, cy - ch/2, cw + 2, ch + 2}, cRed, cRed);
    // Set Virtual size
    SizeVirtual.y = TabPosn (RiffCur, 0) + TabSpacing + 36; //FontFind ()->YAdvance;
  }

void _Score::AutoHorizScroll (void)
  {
    int x, x2;
    /////////////////////////////////////
    //Auto Horoz. Scroll
    x2 = xOffset + (RiffCur->x1 * NoteWidth);
    x = Scroll.x;
    while (x2 + NoteWidth > x + Rect.Width)
      x += NoteWidth;
    while (x2 < x)
      x -= NoteWidth;
    Scroll.x = x;
  }

bool _Score::ProcessEventKeysCommon (int Key)
  {
    switch (Key & KeyMax)
      {
        case '\t':
          TabFocus = !TabFocus;
          break;
        /*case '\b':
          if (RiffCur->x1)
            {
              RiffCur->x1--;
              RiffCur->x2 = RiffCur->x1;
              Delete ();
            }
          break;*/
        case KeyIns:
          Insert ();
          break;
        case KeyDel:
          Delete ();
          break;
        case KeyRight:
          if (RiffCur->Chords [RiffCur->x1].duration != None)
            RiffCur->x1++;
          break;
        case KeyLeft:
          if (RiffCur->x1 > 0)
            RiffCur->x1--;
          break;
        case KeyUp:
          if (TabFocus)
            {
              if (RiffY > 0 && RiffCur->Strings [MaxStrings - 1 - (RiffY - 1)] != None)
                RiffY--;
            }
          else
            if (RiffY > YMin)
              RiffY--;
          break;
        case KeyDown:
          if (TabFocus)
            {
              if (RiffY + 1 < MaxStrings && RiffCur->Strings [MaxStrings - 1 - (RiffY + 1)] != None)
                RiffY++;
            }
          else
            if (RiffY < YMax)
              RiffY++;
          break;
        case KeyHome:
          RiffCur->x1 = 0;
          Scroll.x = 0;
          break;
        case KeyEnd:
          RiffCur->x1 = NotesInRiff (RiffCur);
          break;
        case 'A' - '@':   // ^A
          RiffCur->x1 = 0;
          RiffCur->x2 = NotesInRiff (RiffCur);
          Key |= KeyShift;
          break;
        case 'X'-'@': // ^X - Cut
          Copy (Form);
          Delete ();
          break;
        case 'C'-'@': // ^Copy
          Copy (Form);
          break;
        case 'V'-'@': // ^V Paste
          Paste (Form);
          break;
        default:
          return false;
      }
    if (~Key & KeyShift)
      RiffCur->x2 = RiffCur->x1;
    AutoHorizScroll ();
    return true;
  }

bool _Score::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    int OldX1, OldX2;
    char Key;
    bool Res;
    int Semi;
    bool OK;
    int String;
    int Val;
    int *StringValue;
    static bool NewEntry = true;
    //
    Res = true;
    OldX1 = RiffCur->x1;
    OldX2 = RiffCur->x2;
    Event->X += Scroll.x;
    Event->Y += Scroll.y;
    ChordsSizeCheck (RiffCur, RiffCur->x1 + 2);
    if ((Event->Type == etMouseDown || Event->Type == etMouseMove) && Event->MouseKeys == KeyMouseLeft)
      {
        RiffCur->x1 = Limit ((Event->X - Offset.x - xOffset + NoteWidth / 2) / NoteWidth, 0, NotesInRiff (RiffCur));
        if (Event->Y - Offset.y > TabOffset - TabSpacing)   // Tab Sectionm
          {
            //Val = Limit ((Event->Y - Offset.y - TabOffset + 6) / TabSpacing, 0, MaxStrings - 1);
            Val = MaxStrings - 1 - PosnToString (RiffCur, Event->Y - Offset.y);
            if (Val >= 0)
            //if (RiffCur->Strings [MaxStrings - 1 - Val] != None)
              {
                RiffY = Val;
                TabFocus = true;
              }
          }
        else   // Score Selection
          {
            TabFocus = false;
            RiffY = (Event->Y - Offset.y - StaveOffset + (StaveSpacing / 4)) / (StaveSpacing / 2);
            RiffY = Limit (RiffY, YMin, YMax);
          }
        if (~Event->ShiftState & KeyShift && Event->Type == etMouseDown)
          RiffCur->x2 = RiffCur->x1;
        Redraw = true;
      }
    else if (Event->Type == etKeyDown && (Event->Key & KeyMax))
      {
        Key = Event->Key & KeyMax;
        if (ProcessEventKeysCommon (Event->Key))
          NewEntry = true;
        else
          if (TabFocus)   // Tab Focused
            {
              RiffY = Limit (RiffY, 0, MaxStrings - 1);
              while (RiffCur->Strings [MaxStrings - 1 - RiffY] == None)
                RiffY = (RiffY + 1) % MaxStrings;
              String = MaxStrings - 1 - RiffY;
              StringValue = &RiffCur->Chords [RiffCur->x1].fret [String];
              Val = *StringValue;
              if (IsDigit (Key))
                {
                  //Val = RiffCur->Chords [RiffCur->x1].fret [String];
                  if (NewEntry || Val == None)
                    Val = 0;
                  NewEntry = false;
                  Val = Val * 10 + (Key - '0');
                  if (Val > 24)
                    {
                      Val = 24;
                      NewEntry = true;
                    }
                  *StringValue = Val;
                  RiffCur->Chords [RiffCur->x1].duration = RiffDur;
                }
              else if (Key == ' ')
                *StringValue = None;
              else if (Key == '+' && Val != None && Val < 24)
                (*StringValue)++;
              else if (Key == '-' && Val > 0)
                (*StringValue)--;
              else if (Key == '\b')
                if (NewEntry)
                  *StringValue = None;
                else
                  if (*StringValue < 10)
                    *StringValue = None;
                  else
                    *StringValue /= 10;
              else
                Beep ();
            }
          else   // Score Focused
            {
              if ((Key == KeyEnter) || (Key == ' '))
                {
                  Semi = MapToneToSemiTone (10 - RiffY, RiffCur->Key);
                  OK = FindString (RiffCur->x1, Semi, fsFind);
                  if (!OK && Key != KeyEnter)
                    OK = FindString (RiffCur->x1, Semi + 1, fsFind);
                  if (OK)
                    if (Key == ' ')
                      {
                        if (!FindString (RiffCur->x1, Semi, fsRemove))
                          FindString (RiffCur->x1, Semi + 1, fsRemove);
                        TidyFrets (RiffCur->x1, 0);
                        //Event->Key = 0;
                      }
                  if (!OK)
                    if (Key == KeyEnter)
                      {
                        PlayNote (Semi);
                        FindString (RiffCur->x1, Semi, fsAdd);
                        TidyFrets (RiffCur->x1, 0);
                        //Event->Key = 0;
                      }
                  if (RiffCur->Chords [RiffCur->x1].duration == None)
                    RiffCur->Chords [RiffCur->x1].duration = RiffDur;
                  //%%%%if (OptionsForm->cbAutoAdvanceCursor->Checked)
                    {
                      if (RiffCur->x1 < NotesInRiff (RiffCur))
                        RiffCur->x1++;
                      RiffCur->x2 = RiffCur->x1;
                      AutoHorizScroll ();
                    }
                }
              else
                Beep ();
            }
        Redraw = true;
      }
    else
      Res = false;
    if (RiffCur->x1 != OldX1 || RiffCur->x2 != OldX2)
      SelectionChange = true;
    return Res;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FORM MAIN
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormMain: public _Form
  {
    public:
      //_Container *MenuBar;
      //_Menu *mFile, *mEdit, *mView, *mOptions, *mHelp;
      _Container *Toolbar;
        _Button *bNew, *bOpen, *bSave, *bMIDIImport, *bMIDIExport;
        _Button *bCut, *bCopy, *bPaste, *bDelete;
        _Button *bLonger, *bShorter, *bSplit, *bReverse;
        _Button *bSharpen, *bFlaten, *bStringHigher, *bStringLower, *bStringReallocate;
        _Button *bPlayRiff, *bPlayProject, *bPlayToFile, *bStop;
        _Button *bSynths;
        _Button *bMemo;
        _Button *bRiffProperties;
      _Container *NoteTags;
        _Button *bNoteTags [16];   // These are filled from a table
        _Slider *sMaster;
      _Tabs *tRiff;
        _ButtonArrow *bZoomUp, *bZoomDown;
        _MenuPopup *mRiff;
      _Score *Score;
        _MenuPopup *mScore;
      _Splitter *Splitter;
      _GridView *gProject;
        _SelectionList *SelectionList;
        _MenuPopup *mProject;
      _Label *lStatus;
      //
      _MenuPopup *pmMain;
      //
      _FormMain (char *Title);
     ~_FormMain ();
      //
      void UpdateButtons (void);
      void UpdateRiffTags (void);
      void UpdateStatus (void);
  };

_FormMain *fMain;
bool Exit;

#include "MIDI.cpp"
#include "RiffProperties.cpp"
#include "Synths.cpp"
#include "Memo.cpp"


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////


void ActionMenu (_Menu *Menu)
  {
    DebugAddInt ("Menu", Menu->Selected);
  }

void ActionNew_ (int Result, void *Parameter)
  {
    if (Result == 0)
      {
        if (fMemo)
          delete fMemo;
        RiffsInit ();
        SynthsInit ();
        ProjectInit ();
        StrAssignCopy (&RiffNames, "Riff1");
        StrAssignCopy (&SynthNames, "Synth1");
        StrAssignCopy (&Memo, NULL);
        fMain->tRiff->Selected = 0;
        if (fSynths)
          fSynths->Die ();
        if (fRiffProperties)
          fRiffProperties->Die ();
        Redraw = true;
        fMain->gProject->Invalidate ();
        Changed = false;
      }
  }

void ActionNew (_Container *Button)
  {
    if (Changed)
      {
        MessageResponseBox ({400, 100}, "Warning!!!\v\a1\abCLEAR ENTIRE PROJECT\n Are you sure?\ab", "\a1Yes\a1\tNo", ActionNew_);
        fMessageResponseBox->Container->Colour = cRedLight;
        fMessageResponseBox->Container->ColourGrad = cOrangeLight;
      }
    else
      ActionNew_ (0, NULL);
  }

void DiscardResume (_ActionResultInt ActionDiscardResume)
  {
    MessageResponseBox ({300, 100}, "Warning\v\a+\a+\abUnsaved Changes\ab\a-\a-", "\a1Discard\a0\tKeep Composing", ActionDiscardResume);
    fMessageResponseBox->Container->Colour = cRedLight;
  }

void TitleSet (char *Filename)
  {
    char *St;
    //
    St = NULL;;
    StrAssignCopy (&St, "Score Tab [");
    StrAdd (&St, Revision);
    StrAdd (&St, "] - ");
    StrAdd (&St, StrFindFilename (Filename));
    WindowSetTitle (fMain->Window, St);
    free (St);
  }
void ActionOpenResult (char *File, void *Parameter)
  {
    if (fSynths)
      fSynths->Die ();
    if (fRiffProperties)
      fRiffProperties->Die ();
    if (fMemo)
      fMemo->Die ();
    if (File)
      {
        StrAssignCopy (&Filename, File);
        if (ProjectOpen ())
          {
            fMain->tRiff->Selected = 0;
            fMain->gProject->Rows = NumProjectItems + 1;
            fMain->gProject->Columns = StrCount (SynthNames, '\t') + 3;
            fMain->gProject->Invalidate ();
            Redraw = true;
            TitleSet (File);
          }
      }
  }

void ActionOpenDiscardResume (int Result, void *Parameter)
  {
    if (Result == 0)   // Discard
      FileSelect ("Open ScoreTab Project", ".ntp", false, ActionOpenResult);
  }

void ActionOpen (_Container *Button)
  {
    if (Changed)
      DiscardResume (ActionOpenDiscardResume);
    else
      ActionOpenDiscardResume (0, NULL);
  }

void ActionSaveResult (char *File, void *Parameter)
  {
    if (File)
      {
        if (fMemo)
          delete fMemo;
        StrAssignCopy (&Filename, File);
        ProjectSave ();
        TitleSet (File);
      }
  }

void ActionSave (_Container *Button)
  {
    FileSelect ("Save ScoreTab Project", ".ntp", true, ActionSaveResult);
    FileSelectSetFile (Filename);
  }

void ActionMIDIImport_ (char *Filename, void *Parameter)
  {
    if (Filename)
      {
        ActionNew_ (0, NULL);
        ImportMIDI (Filename);
        Redraw = true;
      }
  }

void ActionMIDIImport (_Container *Button)
  {
    FileSelect ("Import MIDI File", ".mid", false, ActionMIDIImport_);
  }

void ActionMIDIExport_ (char *Filename, void *Parameter)
  {
    if (Filename)
      if (!ExportMIDI (Filename))
        MessageBox ({200, 100}, "Error\v\a1Could not create file", FontSizeDefault * 2 / 3);
  }

void ActionMIDIExport (_Container *Button)
  {
    FileSelect ("Export MIDI File", ".mid", true, ActionMIDIExport_);
  }

void ActionCut (_Container *Button)
  {
    Copy (Button->Form);
    Delete ();
    Redraw = true;
  }

void ActionCopy (_Container *Button)
  {
    Copy (Button->Form);
  }

void ActionPaste (_Container *Button)
  {
    Paste (Button->Form);
    Redraw = true;
  }

void ActionDelete (_Container *Button)
  {
    Delete ();
    Redraw = true;
  }

void ActionLongerShorter  (_Container *Button)
  {
    int x;
    //
    if (fMain->Focused == fMain->Score)
      {
        x = RiffCur->x1;
        while (true)
          {
            if (Button == fMain->bLonger)
              RiffCur->Chords [x].duration = (RiffCur->Chords [x].duration - 1) & 7;
            else
              RiffCur->Chords [x].duration = (RiffCur->Chords [x].duration + 1) & 7;
            if (!Bump (x, RiffCur->x2))
              break;
          }
        Redraw = true;
        Changed = true;
      }
  }

void ActionSplit (_Container *Button)   // Split selected notes into 2. 1/2 length
  {
    int x, x1, x2, a;
    ////////////////////////////////////////////////////////
    if (fMain->Focused == fMain->Score)
      {
        x1 = RiffCur->x1;
        x2 = RiffCur->x2;
        if (x1 > x2)
          SwapBytes (&x1, &x2, sizeof (x1));
        ChordsSizeCheck (RiffCur, RiffCur->ChordsSize + x2 - x1 + 1);   // set length Chords
        for (x = x2; x >= x1; x--)   // backwards thru set
          {
            for (a = RiffCur->ChordsSize - 1 ; a > x; a--)   // move notes along
              memcpy (&RiffCur->Chords [a], &RiffCur->Chords [a - 1], sizeof (_Chord));
            RiffCur->Chords [x].duration++;
            RiffCur->Chords [x + 1].duration++;
          }
        Redraw = true;
        Changed = true;
      }
  }

void ActionReverse (_Container *Button)   // Reverse selected notes
  {
    int x1, x2;
    //
    if (fMain->Focused == fMain->Score)
      {
        x1 = RiffCur->x1;
        x2 = RiffCur->x2;
        if (x1 > x2)
          SwapBytes (&x1, &x2, sizeof (x1));
        while (x1 < x2)
          SwapBytes (&RiffCur->Chords [x1++], &RiffCur->Chords [x2--], sizeof (_Chord));
        Redraw = true;
        Changed = true;
      }
  }

void ActionSharpFlat (_Container *Button)
  {
    int x1, x2, x;
    int String;
    int Val, Val_;
    bool ZeroFret;
    int i;
    //
    if (fMain->Focused == fMain->Score)
      {
        x1 = RiffCur->x1;
        x2 = RiffCur->x2;
        // Single note shift
        if ((x1 == x2) && (fMain->Score->TabFocus))
          {
            String = MaxStrings - 1 - RiffY;
            Val = RiffCur->Chords [x1].fret [String];
            Val_ = Val + Button->DataInt;
            if (Val != None)   // is a note
              if (Val_ <= 24)
                RiffCur->Chords [x1].fret [String] = Val_;
          }
        else
          {
            // Multiple note shift
            // Determine if a zero fret string needs to be flatened
            ZeroFret = false;
            x = x1;
            if (Button->DataInt < 0)   // Make Flat
              while (true)
                {
                  for (i = 0; i < MaxStrings; i++)
                    if (RiffCur->Chords [x].fret [i] == 0)
                      ZeroFret = true;
                  if (!Bump (x, x2))
                    break;
                }
            // Shift the notes
            x = x1;
            while (true)
              {
                if (ZeroFret)   // Need to reallocate strings
                  TidyFrets (x, Button->DataInt);
                else   // Adjusts current strings
                  for (i = 0; i < MaxStrings; i++)
                    if (RiffCur->Chords [x].fret [i] != None)   // is a note
                      RiffCur->Chords [x].fret [i] += Button->DataInt;
                if (!Bump (x, x2))
                  break;
              }
          }
        Redraw = true;
        Changed = true;
      }
  }

void ActionStringUpDown (_Container *Button)
  {
    int x1, x2;
    int x, i;
    ////////////////////////////////////////////////////////
    if (fMain->Focused == fMain->Score)
      {
        x1 = RiffCur->x1;
        x2 = RiffCur->x2;
        if (x1 > x2)
          SwapBytes (&x1, &x2, sizeof (x1));
        for (x = x1; x <= x2; x++)
          if (Button == fMain->bStringHigher)
            {
              for (i = 7; i >= 1; i--)
                if (RiffCur->Strings [i] != None)
                  if (RiffCur->Chords [x].fret [i] == None)
                    if (RiffCur->Chords [x].fret [i - 1] != None)
                      if (RiffCur->Chords [x].fret [i - 1] + RiffCur->Strings [i - 1] - RiffCur->Strings [i] >= 0)
                        {
                          RiffCur->Chords [x].fret [i] = RiffCur->Chords [x].fret [i - 1] + RiffCur->Strings [i - 1] - RiffCur->Strings [i];
                          RiffCur->Chords [x].fret [i - 1] = None;
                        }
            }
          else   // String Lower
            for (i = 0; i < 7; i++)
              if ((RiffCur->Strings [i] != None) && (RiffCur->Strings [i + 1] != None))
                if (RiffCur->Chords [x].fret [i] == None)
                  if (RiffCur->Chords [x].fret [i + 1] != None)
                    //if (Riffs [RiffCur][x].fret [i - 1] + StringNote [i - 1] - StringNote [i] >= 0)
                      {
                        RiffCur->Chords [x].fret [i] = RiffCur->Chords [x].fret [i + 1] + RiffCur->Strings [i + 1] - RiffCur->Strings [i];
                        RiffCur->Chords [x].fret [i + 1] = None;
                      }
        Redraw = true;
        Changed = true;
      }
  }

void ActionStringReallocate (_Container *Button)
  {
    int x, x1, x2;
    ////////////////////////////////////////////////////////
    if (fMain->Focused == fMain->Score)
      {
        x1 = RiffCur->x1;
        x2 = RiffCur->x2;
        if (x1 > x2)
          SwapBytes (&x1, &x2, sizeof (x1));
        for (x = x1; x <= x2; x++)
          TidyFrets (x, 0);
        Redraw = true;
        Changed = true;
      }
  }

int Tick;

void TimerStart (void)
  {
    StrAssign (&RenderError, NULL);
    Rendering = true;
    Tick = ClockMS ();
    Maximum = 0;
  }

int ColToDat (int Col)
  {
    if (Col == 1)
      return 0;
    return (Col - 2) * 2 + 1;
  }

void ProjectCopy (void)
  {
    _Point s0, s1;
    int dx0, dx1;
    int y;
    char *Res;
    char *Dat0, *Dat1;
    //
    s0 = fMain->gProject->Selected0;
    s1 = fMain->gProject->Selected1;
    dx0 = ColToDat (Min (s0.x, s1.x));
    dx1 = ColToDat (Max (s0.x, s1.x) + 1);
    Res = NULL;
    for (y = Min (s0.y, s1.y); y <= Max (s0.y, s1.y); y++)
      {
        Dat0 = StrGetItem (Project [y - 1], dx0, NULL);
        Dat1 = StrGetItem (Project [y - 1], dx1, NULL);
        if (Dat1)
          StrAdd (&Res, Dat0, Dat1 - Dat0 - 1);
        else
          StrAdd (&Res, Dat0);
        StrAdd (&Res, "\n");
      }
    ClipboardSet (fMain->Window, Res);
  }

char *StrGetData (char *Str)
  {
    while (true)
      {
        if (*Str == '\0')
          break;
        if (*Str == '\t')
          break;
        if (*Str == '\n')
          break;
        Str++;
      }
    return Str;
  }

void ProjectPaste (void)
  {
    char *c, *c_, *Dat;
    int x0, y0, x;
    //
    if (ClipboardGet (fMain->Window))
      {
        c = ClipboardData;
        x0 = ColToDat (fMain->gProject->Selected0.x);
        y0 = fMain->gProject->Selected0.y;
        x = x0;
        while (true)
          {
            c_ = StrGetData (c);
            Dat = StrMake (c, c_ - c);
            StrSetItem (&Project [y0 - 1], x, Dat);
            free (Dat);
            c = c_ + 1;   // step past separator
            if (*c_ == '\0')
              break;
            if (*c_ == '\t')
              x++;
            else if (*c_ == '\n')
              {
                y0++;
                x = x0;
              }
          }
        Changed = true;
        fMain->gProject->Invalidate ();
      }
  }

void ProjectDelete ()
  {
    _Point s0, s1;
    int x, y;
    //
    s0 = fMain->gProject->Selected0;
    s1 = fMain->gProject->Selected1;
    for (y = Min (s0.y, s1.y); y <= Max (s0.y, s1.y); y++)
      for (x = ColToDat (Min (s0.x, s1.x)); x < ColToDat (Max (s0.x, s1.x) + 1); x++)
        StrSetItem (&Project [y - 1], x, NULL);
    Changed = true;
    fMain->gProject->Invalidate ();
  }

void ProjectCut ()
  {
    ProjectCopy ();
    ProjectDelete ();
  }

void TimerStop (int NumSamples)
  {
    char St [128], *s;
    int sec;
    ////////////////////////////////////////////////////////////////////////////////////////////
    s = St;
    if (RenderError)
      StrCat (&s, RenderError);
    else
      {
        StrCat (&s, "Render Time \ab");
        IntToStrDecimals (&s, ClockMS () - Tick, 3);
        StrCat (&s, " Sec\ab.   Play Time \ab");
        sec = NumSamples / 44100;
        NumToStr (&s, sec / 60);
        StrCat (&s, ':');
        NumToStr (&s, sec % 60);
        StrCat (&s, "\ab.   ");
        if (Maximum > 0x7FFF)
          StrCat (&s, "\a1");
        StrCat (&s, "Level \ab");
        NumToStr (&s, Maximum * 100 / 0x7FFF);
        StrCat (&s, "%\ab");
        if (Maximum > 0x7FFF)
          StrCat (&s, "   \ab** OVERFLOW **\ab\a1");
      }
    *s = 0;
    fMain->lStatus->TextSet (St);
    Rendering = false;
  }

void ActionPlayRiff (_Container *Button)
  {
    short *block;
    int SizeWord;
    int OffsetWord;
    int Start;
    ////////////////////////////////////////////////////////////////////////////
    TimerStart ();
    Start = 0;
    RiffBufferTranspose = 0;
    block = NULL;
    SizeWord = 0;
    while (true)
      {
        OffsetWord = 0;
        SizeWord = RiffBuffer (RiffCur, Start, &Synths [SynthTab], block, OffsetWord, SizeWord, true) + PlayTrail;
        if (block)
          break;
        block = (short *) malloc (SizeWord * 2);
        memset (block, 0, SizeWord * 2);
      }
    TimerStop (SizeWord / 2);
    SoundPlay_ (block, SizeWord);
  }

void ActionPlayProject (_Container *Button)
  {
    int LineStart, LineEnd;   // Script to play: Inclusive
    short *Buffer;
    int Length;
    //////////////////////////////////////////
    TimerStart ();
    LineStart = 0;
    LineEnd = NumProjectItems - 1;
    if (fMain->gProject->Selected0.y != fMain->gProject->Selected1.y)
      {
        LineStart = Min (fMain->gProject->Selected0.y, fMain->gProject->Selected1.y) - 1;   // %%%% use grid selection??
        LineEnd = Max (fMain->gProject->Selected0.y, fMain->gProject->Selected1.y) - 1;
      }
    Length = ProjectBuffer (LineStart, LineEnd, &Buffer);
    TimerStop (Length / 2);
    if (Length)
      SoundPlay_ (Buffer, Length);
  }

void ActionPlayProjectToFile_ (char *FileName, void *Parameter)
  {
    int Length;
    short *Buffer;
    //////////////////////////////////////////
    if (FileName)
      {
        Length = ProjectBuffer (0, NumProjectItems - 1, &Buffer);
        if (!SaveBuffWAV (FileName, Buffer, Length))
          MessageBox ({300, 200}, "\a1\abFile Write Failed\ab");
        free (Buffer);
      }
  }

void ActionPlayProjectToFile (_Container *Button)
  {
    FileSelect ("Save WAV file", ".wav", true, ActionPlayProjectToFile_);
  }

void ActionPause (_Container *Button)
  {
  }

void ActionStop (_Container *Button)
  {
    SoundStop_ ();
  }

void ActionSynths (_Container *Button)
  {
    if (fSynths == NULL)
      {
        fSynths = new _FormSynths (MousePos ());
        fSynths->tSynths->TextSet (SynthNames);
      }
  }

void ActionMaster (_Slider *Master)
  {
    MasterVolume = Master->ValueGet ();
  }

void ActionZoom (_ButtonArrow *Button)
  {
    _Font *Font;
    //
    Font = fMain->Score->FontFind ();
    FontSizeSet (Font, Font->Size + Button->DataInt);
    fMain->Score->Invalidate ();
  }

void ActionNoteTag (_Button *Button)
  {
    int x;
    byte FlagMask;
    _Chord *Chord;
    //
    if (Button->Down || Button->Group > 1)
      {
        RiffFlags = 0x00;
        FlagMask = 0x0;
        if (Button->DataInt < 8)
          RiffDur = Button->DataInt;
        else if (Button->DataInt == 8)
          {
            RiffFlags = fDotted;
            FlagMask = fDotDotted;
          }
        else if (Button->DataInt == 9)
          {
            RiffFlags = fDotDotted;
            FlagMask = fDotted;
          }
        else if (Button->DataInt == 10)
          RiffFlags = fTie;
        x = RiffCur->x1;
        while (true)
          {
            if (x < RiffCur->ChordsSize)
              {
                Chord = &RiffCur->Chords [x];
                if (RiffFlags == 0x00)
                  Chord->duration = RiffDur;
                else   // Flags
                  {
                    if (Button->Down)
                      Chord->flags = (Chord->flags | RiffFlags) & ~FlagMask;
                    else
                      Chord->flags &= ~RiffFlags;
                  }
              }
            if (!Bump (x, RiffCur->x2))
              break;
          }
        Changed = true;
        Redraw = true;
      }
  }

void ActionRiffSelect (_Tabs *Tabs)
  {
    DebugAddInt ("Tab Select", Tabs->Selected); // ####
    RiffCur = &Riffs [Tabs->Selected];
    // Update Main Form
    fMain->Score->AutoHorizScroll ();
    Redraw = true;
    // Update Riff Properties Form
    if (fRiffProperties)
      fRiffProperties->Fill ();
  }

void ActionTabDrag (int From, int To)
  {
    DebugAddP ("Tab Drag", {From, To}); // ####
    StrItemMove (RiffNames, From, To);
    SwapBytes (&Riffs [From], &Riffs [To], sizeof (_Riff));
    Changed = true;
  }

void ActionRiffDelete (int Result, void *Parameter)
  {
    int n;
    //
    if (Result == 0)   // YES DELETE
      {
        free (RiffCur->TimeSigA);
        RiffCur->TimeSigA = NULL;
        free (RiffCur->Chords);
        RiffCur->Chords = NULL;
        //
        n = fMain->tRiff->Selected;
        StrRemoveItem (RiffNames, n);
        if (n)
          fMain->tRiff->Selected = n - 1;
        while (n + 1 < MaxRiffs)
          {
            Riffs [n] = Riffs [n + 1];
            n++;
          }
        ActionRiffSelect (fMain->tRiff);
        Redraw = true;
      }
  }
void ActionRiffProperties (_Container *Container)
  {
    if (fRiffProperties == NULL)
      fRiffProperties = new _FormRiffProperties (MousePos ());
  }

void ActionRiffMenu (_MenuPopup *MenuPopup)
  {
    char *St, *s;
    //
    switch (MenuPopup->Selected)
      {
        case 0: // Add Riff
          s = St = (char *) malloc (StrLen (RiffNames) + 32);
          StrCat (&s, RiffNames);
          StrCat (&s, "\tRiff");
          NumToStr (&s, StrCount (RiffNames, '\t') + 2);
          *s = 0;
          StrAssign (&RiffNames, St);
          Redraw = true;
          break;
        case 1: // Delete Riff
          if (StrCount (RiffNames, '\t') > 0)   // more than 1 Riff defined
            {
              MessageResponseBox ({300, 100}, "Delete this Riff?", "Yes\tNo", ActionRiffDelete, NULL); //FontSizeDefault * 3 / 2
              fMessageResponseBox->Container->Colour = cRedLight;
              fMessageResponseBox->Container->ColourGrad = -1;
            }
          else
            Beep ();
          break;
        //case 2: // Properties
        //  ActionRiffProperties (NULL);
        //  break;
      }
  }

void ActionScoreMenu (_MenuPopup *MenuPopup)
  {
    if (MenuPopup->Selected == 0)   // Cut
      ActionCut (fMain->bCut);
    else if (MenuPopup->Selected == 1)   // Copy
      ActionCopy (fMain->bCopy);
    else if (MenuPopup->Selected == 2)   // Paste
      ActionPaste (fMain->bPaste);
    else if (MenuPopup->Selected == 3)   // Delete
      ActionDelete (fMain->bDelete);
  }

void ActionCellDraw (_GridView *Grid, int x, int y, _Rect Rect)
  {
    char St [128], *s, *Item;
    int ItemLen, n;
    //
    s = St;
    if (x > 0 || y > 0)
      if (x == 0 || y == 0)
        {
          StrCat (&s, "\ab");
          if (x == 0)
            {
              if ('A' + y - 1 > 'Z')
                StrCat (&s, (y - 1 - 26) / 26 + 'A');
              StrCat (&s, 'A' + (y - 1) % 26);
            }
          else if (y == 0)
            if (x == 1)
              StrCat (&s, "Transpose");
            else
              {
                Item = StrGetItem (SynthNames, x - 2, &ItemLen);
                StrCat (&s, Item, ItemLen);
              }
          StrCat (&s, "\ab");
        }
      else if (y - 1 < MaxProject)   // y > 0
        if (x == 1)   // Transpose
          {
            Item = StrGetItem (Project [y - 1], 0, &ItemLen);
            StrCat (&s, Item, ItemLen);
          }
        else   // Synth Riffs
          {
            n = (x - 2) * 2 + 1;
            Item = StrGetItem (Project [y - 1], n, &ItemLen);
            StrCat (&s, Item, ItemLen);
            if (ItemLen)
              StrCat (&s, "x   ");
            Item = StrGetItem (Project [y - 1], n+1, &ItemLen);
            StrCat (&s, Item, ItemLen);
          }
    *s = 0;
    Grid->TextOutAligned (Rect, St, x == 0 ? aCenter : aLeft, aCenter);
  }

void ActionSelectTransRiffRepeat (_SelectionList *List)   // DataInt: 0->Trans 1->Repeat 2->Riff
  {
    _Point Cell;
    int i;
    int n;
    char *sSel, *sRep;
    //
    DebugAddInt ("Repeat Selected", List->Selected); //####
    if (List->Selected >= 0)
      {
        sSel = StrGetItem (List->TextGet (), List->Selected);
        Cell = fMain->gProject->Selected0;
        while (true)
          {
            i = Cell.y - 1;   // Project Line Item
            if (List->DataInt == 0)   // Trans
              StrSetItem (&Project [i], 0, sSel);
            else if (Cell.x >= 2)   // Repeat + Riff column
              {
                n = (Cell.x - 2) * 2 + 1;   // Data Field index
                if (List->DataInt == 1)   // Repeats
                  StrSetItem (&Project [i], n, sSel);
                else   // Riffs
                  {
                    StrSetItem (&Project [i], n + 1, sSel);
                    sRep = StrGetItem (Project [i], n);   // check repeat field
                    if (StrLen (sRep) == 0)
                      StrSetItem (&Project [i], n, "1");
                    free (sRep);
                  }
              }
            if (!Bump (Cell.x, fMain->gProject->Selected1.x))
              {
                Cell.x = fMain->gProject->Selected0.x;
                if (!Bump (Cell.y, fMain->gProject->Selected1.y))
                  break;
              }
          }
        free (sSel);
        fMain->gProject->Invalidate ();
        Changed = true;
      }
    List->Die ();
  }

void ActionCellSelect (_GridView *Grid, int x, int y)
  {
    if (y + 1 == Grid->Rows)
      {
        Grid->Rows++;
        Grid->Invalidate ();
      }
    NumProjectItems = Max (NumProjectItems, y);
  }

const char *Trans = "[+12]\t[+11]\t[+10]\t[+9]\t[+8]\t[+7]\t[+6]\t[+5]\t[+4]\t[+3]\t[+2]\t[+1]\t[0]\t"
                         "[-1]\t[-2]\t[-3]\t[-4]\t[-5]\t[-6]\t[-7]\t[-8]\t[-9]\t[-10]\t[-11]\t[-12]";

const char *Repeats = "1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11\t12\t13\t14\t15\t16";

char* ActionCellRead (_GridView *Grid, int x, int y)
  {
    _Rect Pos;
    //
    Pos = fMain->gProject->CellRect (fMain->gProject->CellIndexToGridPosn ({x, y}));
    DebugAddP ("ActionCellRead at", {Pos.x, Pos.y}); //####
    if (x == 1)
      {
        fMain->SelectionList = new _SelectionList (fMain->Container, {Pos.x + fMain->gProject->Rect.x, Pos.y + fMain->gProject->Rect.y}, (char *) Trans, (_Action) ActionSelectTransRiffRepeat);
        fMain->SelectionList->DataInt = 0;
      }
    if (x >= 2)
      {
        fMain->SelectionList = new _SelectionList (fMain->Container, {Pos.x + fMain->gProject->Rect.x, Pos.y + fMain->gProject->Rect.y}, RiffNames, (_Action) ActionSelectTransRiffRepeat);
        fMain->SelectionList->DataInt = 2;
      }
    fMain->EventLock = fMain->SelectionList;
    return NULL;
  }

void ActionProjectMenu (_MenuPopup *ProjectMenu)
  {
    _Point Sel;
    //
    Sel = fMain->gProject->Selected0;
    if (ProjectMenu->Selected == 0)   // Cut
      ProjectCut ();
    else if (ProjectMenu->Selected == 1)   // Copy
      ProjectCopy ();
    else if (ProjectMenu->Selected == 2)   // Paste
      ProjectPaste ();
    else if (ProjectMenu->Selected == 3)   // Delete
      ProjectDelete ();
    else if (ProjectMenu->Selected == 4)   // Trans
      {
        fMain->SelectionList = new _SelectionList (fMain->Container, ProjectMenu->SelectionList->Rect, (char *) Trans, (_Action) ActionSelectTransRiffRepeat);
        fMain->SelectionList->DataInt = 0;
        fMain->EventLock = fMain->SelectionList;
      }
    else if (ProjectMenu->Selected == 5)   // Repeats
      {
        fMain->SelectionList = new _SelectionList (fMain->Container, ProjectMenu->SelectionList->Rect, (char *) Repeats, (_Action) ActionSelectTransRiffRepeat);
        fMain->SelectionList->DataInt = 1;
        fMain->EventLock = fMain->SelectionList;
      }
    else if (ProjectMenu->Selected == 6)   // Riffs
      {
        fMain->SelectionList = new _SelectionList (fMain->Container, ProjectMenu->SelectionList->Rect, RiffNames, (_Action) ActionSelectTransRiffRepeat);
        fMain->SelectionList->DataInt = 2;
        fMain->EventLock = fMain->SelectionList;
      }
  }

void ActionQuit (int Result, void *Parameter)
  {
    if (Result == 0)   // Discard
      Exit = true;
  }

bool FormMainPreview (_Form *Form, _Event *Event)
  {
    // Make MessageBox Modal
    if (fMessageResponseBox)
      return true;
    // Guard against losing changes
    if (Event->Type == etWindowClose || (Event->Type == etKeyDown && Event->Key == 'Q' - '@'))
      {
        if (Changed)
          DiscardResume (ActionQuit);
        else
          Exit = true;
        return true;
      }
    // Tab switch 3 way
    if (Event->Type == etKeyDown && Event->Key == '\t')
      if (Form->Focused == fMain->Score && fMain->Score->TabFocus)   // Score:Tab has Focus
        {
          fMain->Score->TabFocus = false;
          fMain->gProject->FocusSet ();
          return true;
        }
    if (Form->Focused == fMain->gProject)   // Project grid has Focus
      {
        if (Event->Type == etKeyDown && Event->Key == 'X' - '@')
          ProjectCut ();
        else if (Event->Type == etKeyDown && Event->Key == 'C' - '@')
          ProjectCopy ();
        else if (Event->Type == etKeyDown && Event->Key == 'V' - '@')
          ProjectPaste ();
        else if (Event->Type == etKeyDown && Event->Key == KeyDel)
          ProjectDelete ();
        else
          return false;
        return true;
      }
    return false;
  }

char *NoteTagChar =  "0123456789A";
char *NoteTagGroup = "11111111223";

void _FormMain::UpdateButtons (void)
  {
    int i;
    _Chord *c;
    //
    for (i = 0; NoteTagChar [i] != '\0'; i++)
      bNoteTags [i]->Down = false;
    i = RiffCur->x1;
    while (true)
      {
        c = &RiffCur->Chords [i];
        if (c)
          {
            if (c->duration != None && c->duration < StrLen (NoteTagChar))
              bNoteTags [c->duration]->Down = true;
            if (c->flags & fDotted)
              bNoteTags [8]->Down = true;
            if (c->flags & fDotDotted)
              bNoteTags [9]->Down = true; //Set (true);
            if (c->flags & fTie)
              bNoteTags [10]->Down = true;
          }
        if (!Bump (i, RiffCur->x2))
          break;
      }
    for (i = 0; NoteTagChar [i] != '\0'; i++)
      bNoteTags [i]->Invalidate ();
  }

void _FormMain::UpdateStatus (void)
  {
    word Notes;
    int x, i, a, b, c;
    char St [128], *s;
    //
    s = St;
    a = TimeInRiff (RiffCur, Min (RiffCur->x1, RiffCur->x2));
    b = TimeInRiff (RiffCur, Max (RiffCur->x1, RiffCur->x2) + 1);
    c = TimeInRiff (RiffCur, -1);
    if (RiffCur->x1 == RiffCur->x2)
      {
        StrCat (&s, "Position \ab");
        TimeToStr (&s, RiffCur, a);
        StrCat (&s, " / ");
        TimeToStr (&s, RiffCur, c);
      }
    else   // Multi note Section
      {
        // Show position / selection in Riff
        StrCat (&s, "Selected \ab");
        TimeToStr (&s, RiffCur, a);
        StrCat (&s, " - ");
        TimeToStr (&s, RiffCur, b);
        StrCat (&s, " (");
        TimeToStr (&s, RiffCur, Abs (a - b));
        StrCat (&s, ')');
        // Make and Display a Chord of the Selection
        Notes = 0;
        x = RiffCur->x1;
        while (true)
          {
            for (i = 0; i < MaxStrings; i++)
              if (RiffCur->Chords [x].fret [i] != None)
                Notes |= Bit [(RiffCur->Strings [i] + RiffCur->Chords [x].fret [i] + 100 * 12) % 12];
            if (!Bump (x, RiffCur->x2))
              break;
          }
        StrCat (&s, "\ab  Chord \ab");
        i = StrFindChordName (&s, Notes);
        if (i == 0)
          StrCat (&s, '-');
      }
    StrCat (&s, "\ab");
    *s = 0;
    lStatus->TextSet (St);
  }

void _FormMain::UpdateRiffTags (void)
  {
    char *RNP, *rnp;
    int rnpl;
    int i;
    char *s;
    int l, t;
    //
    RNP = rnp = NULL;
    rnpl = 0;
    i = 0;
    while (true)
      {
        s = StrGetItem (RiffNames, i, &l);
        if (l == 0)
          if (RNP)
            break;
          else
            {
              RNP = rnp = (char *) malloc (rnpl + 1);
              rnpl = 0;
              i = 0;
            }
        else
          {
            if (rnpl)
              rnpl += StrCat (&rnp, '\t');
            rnpl += StrCat (&rnp, "\eT\ab");
            rnpl += StrCat (&rnp, s, l);
            rnpl += StrCat (&rnp, "\ab\n");
            t = TimeInRiff (&Riffs [i], Riffs [i].ChordsSize);
            rnpl += TimeToStr (&rnp, &Riffs [i], t);
            i++;
          }
      }
    *rnp = 0;
    tRiff->TextSet (RNP);
    free (RNP);
  }

const _Point FormSize = {700, 650};

_FormMain::_FormMain (char *Title): _Form (Title, {100, 100, FormSize.x, FormSize.y}, waResizable)
  {
    //const int mh = 22;   // menu height
    const int bx = 22;   // button width
    const int by = 22;   // button height
    const int ty = 40;   // Tab Height
    const int sh = 18;   // Status Height
    int x, y;
    char St [4];
    int i;
    // Make the Main Form
    Container->FontSet (NULL);
    Container->Bitmap = BitmapLoad (Window, "ScoreTab.bmp");
    y = 0;
    // Build Menu
    /*
    MenuBar = new _Container (Container, {0, 0, 0, mh});
    MenuBar->Colour = cTextBgnd;
    x = 0;
    mFile = new _Menu (MenuBar, {x, 0, 0, 0}, "File\t\e3 Rename\t\et Edit\t\et Edit Hex\t\e5 Delete\t_\eD Properties\t\e6 New File\t_\e7 New Directory\t\e4 Open\t\eC Execute\t\eC Terminal\t\eF Unmount", (_Action) ActionMenu);
    mFile->DataInt = 0;
    x += mFile->Rect.Width;
    mEdit = new _Menu (MenuBar, {x, 0, 0, 0}, "Edit\t\e0 Cut\t\e1 Copy\t\e2 Paste\t\e2 Paste Link\t\e1 Duplicate", (_Action) ActionMenu);
    mEdit->DataInt = 1;
    x += mEdit->Rect.Width;
    mView = new _Menu (MenuBar, {x, 0, 0, 0}, "View\t    Hidden Files\t_    System Files\t\eB Search\t_\eV Summary\tSettings ...\tLog ...", (_Action) ActionMenu);
    mView->DataInt = 2;
    mView->Boxed = 0x0001 | 0x0002;
    x += mView->Rect.Width;
    mOptions = new _Menu (MenuBar, {x, 0, 0, 0}, "Options\tOrganize Files\tDelete Empty Directories\tList Unmatched Files", (_Action) ActionMenu);
    mOptions->DataInt = 3;
    x += mOptions->Rect.Width;
    mHelp = new _Menu (MenuBar, {x, 0, 0, 0}, "Help\tHelp\tAbout", (_Action) ActionMenu);
    mHelp->DataInt = 4;
    y += mh;
    */
    //
    // build Toolbar
    Toolbar = new _Container (Container, {0, y, 0, by});
    x = 0;
    bNew = new _Button (Toolbar, {x, 0, bx, by}, "\e0", ActionNew); x += by;
    bOpen = new _Button (Toolbar, {x, 0, bx, by}, "\e1", ActionOpen); x += by;
    bSave = new _Button (Toolbar, {x, 0, bx, by}, "\e2", ActionSave); x += by;
    bSave->EnabledSet (false);
    bPlayToFile = new _Button (Toolbar, {x, 0, bx, by}, "\eI", ActionPlayProjectToFile); x += by;
    bMIDIImport = new _Button (Toolbar, {x, 0, bx, by}, "\eN", ActionMIDIImport); x += bx;
    bMIDIExport = new _Button (Toolbar, {x, 0, bx, by}, "\eO", ActionMIDIExport); x += bx;
    x += 8;
    //
    bCut = new _Button (Toolbar, {x, 0, bx, by}, "\e3", ActionCut); x += by;
    bCopy = new _Button (Toolbar, {x, 0, bx, by}, "\e4", ActionCopy); x += by;
    bPaste = new _Button (Toolbar, {x, 0, bx, by}, "\e5", ActionPaste); x += by;
    bDelete = new _Button (Toolbar, {x, 0, bx, by}, "\e6", ActionDelete); x += by;
    //
    bLonger = new _Button (Toolbar, {x, 0, bx, by}, "\e7", ActionLongerShorter); x += by;
    bShorter = new _Button (Toolbar, {x, 0, bx, by}, "\e8", ActionLongerShorter); x += by;
    bSplit = new _Button (Toolbar, {x, 0, bx, by}, "\e9", ActionSplit); x += by;
    bReverse = new _Button (Toolbar, {x, 0, bx, by}, "\eA", ActionReverse); x += by;
    //
    bSharpen = new _Button (Toolbar, {x, 0, bx, by}, "\eB", ActionSharpFlat); x += by;
    bSharpen->DataInt = +1;
    bFlaten = new _Button (Toolbar, {x, 0, bx, by}, "\eC", ActionSharpFlat); x += by;
    bFlaten->DataInt = -1;
    bStringHigher = new _Button (Toolbar, {x, 0, bx, by}, "\eD", ActionStringUpDown); x += by;
    bStringLower = new _Button (Toolbar, {x, 0, bx, by}, "\eE", ActionStringUpDown); x += by;
    bStringReallocate = new _Button (Toolbar, {x, 0, bx, by}, "\eF", ActionStringReallocate); x += by;
    //
    bPlayRiff = new _Button (Toolbar, {x, 0, bx, by}, "\eG", ActionPlayRiff); x += by;
    bPlayProject = new _Button (Toolbar, {x, 0, bx, by}, "\eH", ActionPlayProject); x += by;
    //bPause = new _Button (Toolbar, {x, 0, bx, by}, "\eJ", ActionPause); x += by;
    bStop = new _Button (Toolbar, {x, 0, bx, by}, "\eK", ActionStop); x += by;
    //
    x = 0;
    y += by;
    // Make Note Properties
    NoteTags = new _Container (Container, {0, y, 0, by});
    NoteTags->Bitmap = BitmapLoad (Window, "NoteTags.bmp");
    i = 0;
    while (NoteTagChar [i])
      {
        St [0] = '\e';
        St [1] = NoteTagChar [i];
        St [2] = '\0';
        bNoteTags [i] = new _Button (NoteTags, {x, 0, bx, by}, St, (_Action) ActionNoteTag);
        bNoteTags [i]->DataInt = i;
        bNoteTags [i]->Group = NoteTagGroup [i] - '0';
        bNoteTags [i]->Toggle = true; //NoteTagGroup [i] >= '2';
        x += by;
        i++;
      }
    // Right side controls
    bSynths = new _Button (Toolbar, {-64, 0, 0, 0}, "\eL Synths", ActionSynths);
    bMemo = new _Button (Toolbar, {-128, 0, 64, 0}, "\eP Memo", ActionMemo);
    sMaster = new _Slider (NoteTags, {-64, 0, 0, 0}, NULL, (_Action) ActionMaster, 0, 100);
    sMaster->ValueSet (MasterVolume);
    bRiffProperties = new _Button (NoteTags, {-128, 0, 64, 0}, "\eB Riff", ActionRiffProperties);
    y += by;
    // Make Riffs Tabs & Popup Menu
    tRiff = new _Tabs (Container, {0, y, -bx, ty}, RiffNames, (_Action) ActionRiffSelect, ActionTabDrag);
    tRiff->Colour = ColourAdjust (cBlue, 190);
    mRiff = new _MenuPopup (tRiff, {0}, "\eQ Add Riff\t\eR Delete Riff", (_Action) ActionRiffMenu);
    bZoomUp = new _ButtonArrow (Container, {-by, y, 0, ty / 2}, NULL, (_Action) ActionZoom, dUp); bZoomUp->DataInt = +1;
    bZoomDown = new _ButtonArrow (Container, {-by, y + ty / 2, 0, ty / 2}, NULL, (_Action) ActionZoom, dDown); bZoomDown->DataInt = -1;
    y += ty;
    // Make Score
    Score = new _Score (Container, {0, y, 0, 276});
    Score->Colour = tRiff->Colour;
    Score->ColourGrad = ColourAdjust (cGreen, 190);
    y += Score->Rect.Height;
    mScore = new _MenuPopup (Score, {0}, "\e3 Cut\t\e4 Copy\t_\e5 Paste\t\e6 Delete", (_Action) ActionScoreMenu);
    // Splitter
    Splitter = new _Splitter (Container, {0, y, 0, 8});
    y += Splitter->Rect.Height;
    // Project Grid
    gProject = new _GridView (Container, {0, y, 0, -sh}, NULL, 4, 4, ActionCellDraw);
    gProject->ActionCellRead = ActionCellRead;
    gProject->ActionCellSelect = ActionCellSelect;
    gProject->Title = gProject->Locked = {1, 1};
    gProject->CellWidthSet (0, 25);
    gProject->CellWidthSet (1, 40);
    gProject->CellWidthSet (2, 124);
    gProject->CellSizeDefault.x = 124;
    gProject->Options |= goSelectMulti | goAdjustColumn;
    mProject = new _MenuPopup (gProject, {0}, "\e3 Cut\t\e4 Copy\t\e5 Paste\t_\e6 Delete\tTrans\v>>\tRepeats\v>>\tRiffs\v>>", (_Action) ActionProjectMenu);
    //
    lStatus = new _Label (Container, {0, -sh, 0, 0}, NULL, aLeft, bLowered);
    //
    Score->FocusSet ();
    EventPreview = FormMainPreview;
  }

_FormMain::~_FormMain ()
  {
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN LOOP
//
////////////////////////////////////////////////////////////////////////////////////////////////////

int main_ (int argc, char *argv [])
  {
    char St [200], *s;
    _Bitmap *Icon;
    //
    DebugAddS ("===========Start ScoreTab", Revision);
    NoteBufferInit ();
    RiffsInit ();
    SynthsInit ();
    StrAssignCopy (&RiffNames, "Riff1");
    StrAssignCopy (&SynthNames, "Synth1");
    RiffDur = 3;
    chdir (PathExe);
    //
    cForm1 = 0xF0F0F0;
    cForm2 = 0xD0D0D0;
    DebugAddS ("Exe Path: ", PathExe);
    s = St;   // Build the form Title
    StrCat (&s, "Score Tab [");
    StrCat (&s, Revision);
    StrCat (&s, "] - Stewy\'s Music Scoring Sequencer");
    *s = 0;
    ButtonStyleDefault = bsSimple; //;bsFlat
    fMain = new _FormMain (St);
    ButtonStyleDefault = bsFlat;
    Icon = BitmapLoad (fMain->Window, "ScoreTabIcon.bmp");
    WindowSetIcon (fMain->Window, Icon);
    BitmapDestroy (Icon);
    //
    if (argc >= 2)
      ActionOpenResult (argv [1], NULL);
    // Main Loop
    Exit = false;
    Redraw = true;
    while (!Exit)
      {
        if (SynthNamesChanged)
          {
            SynthNamesChanged = false;
            Changed = true;
            fMain->gProject->Columns = StrCount (SynthNames, '\t') + 3;
            fMain->gProject->Invalidate ();
            DebugAddS ("SynthNames", SynthNames); //####
          }
        fMain->bSave->EnabledSet (Changed);
        if (Redraw)
          {
            Redraw = false;
            fMain->Score->Invalidate ();
            fMain->UpdateStatus ();
            fMain->UpdateRiffTags ();
          }
        if (SelectionChange)
          {
            SelectionChange = false;
            fMain->UpdateButtons ();
          }
        usleep (1000);
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete (FormList);
    free (RiffNames);
    free (SynthNames);
    free (Filename);
    free (RenderError);
    return 0;
  }


/***************************************************************
                            /*
                            e = Error = NULL;
                            el = 0;
                            while (true)
                              {
                                el += StrCat (&e, "\ab\a1Error! Undefined Riff - ");
                                el += StrCat (&e, Str_, l);
                                if (Error)
                                  break;
                                e = Error = (char *) malloc (el + 1);
                              }
                            *e = 0;
                            */
                            //StrAssign (&Error, "\ab\a1Error! Undefined Riff - ");
                            //StrAdd (&Error, Str_, l);
