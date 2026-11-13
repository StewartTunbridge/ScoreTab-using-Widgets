////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MIDI FILE READ
//
////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int ReadBytes (byte *Buffer, int Size, int *Index, int DataSize)
  {
    unsigned int Val;
    int i;
    ////////////////////////////////////////////////////////////////////////////
    if (*Index + DataSize <= Size)
      {
        Val = 0;
        for (i = 0; i < DataSize; i++)
          Val = (Val << 8) + Buffer [(*Index)++];
        return Val;
      }
    return -1;
  }

word ReadWord (byte *Buffer, int Size, int *Index)
  {
    return ReadBytes (Buffer, Size, Index, 2);
  }

int ReadInt (byte *Buffer, int Size, int *Index)
  {
    return ReadBytes (Buffer, Size, Index, 4);
  }

int ReadDeltaTime (byte *Buffer, int Size, int *Index)
  {
    int Val;
    byte Byt;
    ////////////////////////////////////////////////////////////////////////////
    Val = 0;
    while (true)
      {
        if (*Index >= Size)
          return -1;
        Byt = Buffer [(*Index)++];
        Val = (Val << 7) + (Byt & 0x7F);
        if ((Byt & 0x80) == 0)   // no more bytes
          return Val;
      }
  }

bool ReadChunkHeader (byte *Buffer, int Size, int *Index, char *ChunkName, int *ChunkSize)
  {
    if (*Index + 8 <= Size)
      {
        StrCat (&ChunkName, (char *) &Buffer [*Index], 4);
        *ChunkName = 0;
        *Index += 4;
        *ChunkSize = ReadInt (Buffer, Size, Index);
        return true;
      }
    return false;
  }

/*bool ChordNoteType (_Chord *Chord,  int NoteTime, int TicksPerQuarter)
  {
    int Dur;
    int Ticks;
    int i;
    //
    if (NoteTime < TicksPerQuarter / 32)   // Less than 1/128 time
      return false;
    NoteTime += TicksPerQuarter / 32;   // Round up by 1/128 time
    Ticks = TicksPerQuarter * 8;   // 2 * Whole Ticks
    Dur = 0;   // 2 * Whole
    while (true)
      {
        if (NoteTime >= Ticks)
          {
            Chord->duration = Dur;
            if (NoteTime > Ticks * 3 / 2)
              Chord->flags |= fDotted;
            if (NoteTime > Ticks * 7 / 4)
              Chord->flags |= fDotDotted;
            return true;
          }
        Ticks /= 2;
        Dur++;
        if (Dur >= 6)
          {
            Chord->duration = None;
            for (i = 0; i < MaxStrings; i++)
              Chord->fret [i] = None;
            return false;
          }
      }
  }*/

void NoteSetLength (int NoteTime, int TicksPerQuarter, bool Advance)
  {
    int Err;
    int Dur;
    int Ticks;
    int x1;
    //
    Err = TicksPerQuarter / 32;
    if (NoteTime > Err)
      {
        x1 = RiffCur->x1;
        Ticks = TicksPerQuarter * 8;   // 2 * Whole Ticks
        Dur = 0;   // 2 * Whole
        while (true)
          {
            if (NoteTime + Err >= Ticks)   // Note fits into Ticks
              {
                RiffCur->Chords [x1].duration = Dur;
                NoteTime -= Ticks;
                if (NoteTime < Err)   // no note left
                  {
                    if (Advance)
                      RiffCur->x1 = x1 + 1;
                    ChordsSizeCheck (RiffCur, RiffCur->x1 + 1);
                    break;
                  }
                ChordsSizeCheck (RiffCur, x1 + 2);   // some note left over, insert new note
                memcpy (&RiffCur->Chords [x1 + 1], &RiffCur->Chords [x1], sizeof (_Chord));   // clone previous note
                RiffCur->Chords [x1 + 1].flags &= ~fTie;   // ... without the Tie (for now)
                RiffCur->Chords [x1].flags |= fTie;   // Tie original note
                x1++;   // next note
              }
            else   // Ticks too big, try smaller
              {
                Ticks /= 2;
                Dur++;
                /*if (Dur >= 6)
                  {
                    RiffCur->Chords [x1].duration = None;
                    RiffCur->Chords [x1].flags = 0x00;
                    break;
                  }*/
              }
          }
      }
  }

const char *MThd = "MThd";
const char *MTrk = "MTrk";

void ImportMIDI (char *Filename)
  {
    int f;
    int Size;
    byte *Buffer;
    int Index;
    //int Pass;
    //int Channels;
    bool State;
    int ChunkStart;
    char ChunkName [32];
    char Str [128], *s;
    int ChunkSize;
    word Format, NumTracks; //, TimeDivision, TimeDivisionMultiplier,
    word TimeQuarter;   // Midi Time per 1/4 note
    int Riff;
    int DTime, ATime;
    int NoteOn [MaxRiffs];
    int NoteOff [MaxRiffs];
    int NoteOffset [MaxRiffs];
    int Note;
    byte Velocity;
    byte Cmnd;
    byte MetaType;
    int MetaStart;
    byte MetaLength;
    bool EndTrack;
    ////////////////////////////////////////////////////////////////////////////
    f = FileOpen (Filename, foRead);
    if (f >= 0)   // File OK
      {
        Size = FileSize (f);
        Buffer = (byte *) malloc (Size);
        if (Buffer)
          FileRead (f, Buffer, Size);
        FileClose (f);
        if (Buffer)
          {
            //for (Pass = 1; Pass <= 2; Pass++)
              {
                //if (Pass == 2)
                //  EditChannels (&Channels);
                TimeQuarter = 192;   // for now
                Index = 0;
                for (Riff = 0; Riff < SIZEARRAY (NoteOffset); Riff++)
                  NoteOffset [Riff] = 0;
                Riff = -1;
                while (true)
                  {
                    if (!ReadChunkHeader (Buffer, Size, &Index, ChunkName, &ChunkSize))
                      break;
                    ChunkStart = Index;
                    if (StrSame_ (ChunkName, MThd))
                      {
                        Format = ReadWord (Buffer, Size, &Index);
                        NumTracks = ReadWord (Buffer, Size, &Index);
                        TimeQuarter = ReadWord (Buffer, Size, &Index);
                        s = Str; // ####
                        StrCat (&s, "Midi: Format ");
                        NumToStr (&s, Format);
                        StrCat (&s, "  NumTracks ");
                        NumToStr (&s, NumTracks);
                        StrCat (&s, "   Time Per Quarter ");
                        NumToStr (&s, TimeQuarter);
                        *s = 0;
                        DebugAdd (Str); // ####
                      }
                    else if (StrSame_ (ChunkName, MTrk))
                      {
                        Riff++;
                        RiffCur = &Riffs [Riff];
                        RiffCur->x1 = RiffCur->x2 = 0;
                        //for (Riff = 0; Riff < MaxRiffs; Riff++)
                          {
                            NoteOn [Riff] = 0;
                            NoteOff [Riff] = 0;
                          }
                        State = false;
                        ATime = 0;
                        EndTrack = false;
                        while (Index < ChunkStart + ChunkSize && !EndTrack)
                          {
                            DTime = ReadDeltaTime (Buffer, Size, &Index);
                            ATime += DTime;
                            Cmnd = Buffer [Index++];
                            /*s = Str; // ####
                            StrCat (&s, "Midi: ");
                            NumToStr (&s, DTime);
                            StrCat (&s, ": Command ");
                            NumToHex (&s, Cmnd, 2);
                            StrCat (&s, ' ');
                            *s = 0;
                            DebugAdd (Str); // ####*/
                            if ((Cmnd & 0xF0) == 0xF0)   // Meta Event
                              {
                                MetaType = Buffer [Index++];
                                MetaLength = Buffer [Index++];
                                MetaStart = Index;
                                /*s = Str;  // ####
                                StrCat (&s, "\tMetaType ");
                                NumToHex (&s, MetaType, 2);
                                StrCat (&s, ' ');
                                DataToHexAscii (&s, &Buffer [Index], MetaLength);
                                *s = 0;
                                DebugAdd (Str);*/ //####
                                if (MetaType == 0x03)
                                  {
                                    s = StrMake ((char *) &Buffer [Index], MetaLength);
                                    DebugAddS ("\tFF03", s); //####
                                    StrSetItem (&RiffNames, Riff, s);
                                    free (s);
                                  }
                                else if (MetaType == 0x2F)   // End Track
                                  EndTrack = true;
                                else if (MetaType == 0x51)   // Set Tempo
                                  RiffCur->Tempo = ReadBytes (Buffer, Size, &Index, 3);
                                else if (MetaType == 0x58)
                                  {
                                    StrAssignCopy (&RiffCur->TimeSigA, "#");
                                    RiffCur->TimeSigA [0] = '0' + Buffer [Index++];
                                    RiffCur->TimeSigB = Buffer [Index++] + 1;
                                  }
                                Index = MetaStart + MetaLength;
                              }
                            else if ((Cmnd & 0xF0) == 0x80 || (Cmnd & 0xF0) == 0x90)   // Note On/Off
                              {
                                Note = Buffer [Index++] - 48;
                                Velocity = Buffer [Index++];
                                if ((Cmnd & 0xF0) == 0x90 && Velocity > 0)   // Note On
                                  {
                                    NoteOn [Riff] = ATime;
                                    //if (ATime > NoteOff [Riff])   // a gap exists
                                    //  DebugAddInt ("\t\tGap", ATime - NoteOff [Riff]); //####
                                    // Add rests to catch NoteOff [Riff] time to ATime
                                    ChordsSizeCheck (RiffCur, RiffCur->x1 + 1);
                                    NoteSetLength (ATime - NoteOff [Riff], TimeQuarter, !State);
                                    // Add the new note
                                    while (!FindString (RiffCur->x1, Note + NoteOffset [Riff], fsAdd))
                                      NoteOffset [Riff] += 12;
                                    RiffCur->Chords [RiffCur->x1].duration = RiffDur;   // will be corrected on Note Off
                                    State = true;
                                  }
                                else   // Note Off
                                  {
                                    NoteOff [Riff] = ATime;
                                    //#### DebugAddInt ("\t\tWidth", ATime - NoteOn [Riff]);
                                    NoteSetLength (ATime - NoteOn [Riff], TimeQuarter, State);
                                    //if (ChordNoteType (&RiffCur->Chords [RiffCur->x1], ATime - NoteOn [Riff], TimeQuarter))
                                    //  if (State)
                                    //    RiffCur->x1++;
                                    State = false;
                                  }
                              }
                            else if ((Cmnd & 0xF0) == 0xC0)   // Instrument Change
                              Index++;
                            else if ((Cmnd & 0xF0) == 0xB0)   // Controller Change
                              Index += 2;
                            else
                              DebugAddHex ("MIDI Unknpwn Command", Cmnd);
                          }
                      }
                    else
                      {
                        DebugAdd ("Invalid MIDI File");
                        break;
                      }
                    Index = ChunkStart + ChunkSize;
                  }
              }
            free (Buffer);
            Redraw = true;
          }
      }
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MIDI FILE WRITE
//
////////////////////////////////////////////////////////////////////////////////////////////////////

const int TicksPerQuarter = 0xC0;

int BuffData (byte **Buf, byte *Data, int DataSize)
  {
    if (*Buf)
      {
        memcpy (*Buf, Data, DataSize);
        (*Buf) += DataSize;
      }
    return DataSize;
  }

int BuffStr (byte **Buf, const char *Str)
  {
    return BuffData (Buf, (byte *) Str, StrLen (Str));
  }

int BuffUInt (byte **Buf, int Val, unsigned int Len)
  {
    int i;
    //
    if (*Buf)
      {
        for (i = 0; i < Len; i++)
          {
            (*Buf) [Len - 1 - i] = (byte) Val;
            Val >>= 8;
          }
        (*Buf) += Len;
      }
    return Len;
  }

/*int BuffInt1 (byte **Buf, unsigned int Val)
  {
    return BuffInt (Buf, Val, 1);
  }

int BuffInt2 (byte **Buf, unsigned int Val)
  {
    return BuffInt (Buf, Val, 2);
  }

int BuffInt4 (byte **Buf, unsigned int Val)
  {
    return BuffInt (Buf, Val, 4);
  }*/

int MIDITime, MIDITime_;   // RealTime, TimeLastEvent

int BuffTime (byte **Buf)   // Output a Delta Time in MIDI format
  {
    int Res;
    int DeltaTime;
    byte x, Val [8];
    int i;
    //
    DeltaTime = MIDITime - MIDITime_;
    i = 8;
    while (true)
      {
        x = DeltaTime & 0x0000007F;
        if (i != 8)   // not the last byte
          x |= 0x80;
        Val [--i] = x;
        DeltaTime >>= 7;
        if (DeltaTime == 0)
          break;
      }
    Res = 8 - i;   // Size of the Time value
    BuffData (Buf, &Val [i], Res);
    MIDITime_ = MIDITime;
    return Res;
  }

int ChordTicks (_Chord *Chord)   // Calculate Note Time in MIDI Ticks
  {
    int Dur;
    int NoteDuration;
    //
    Dur = Chord->duration;
    if (Dur == None)
      return None;
    NoteDuration = TicksPerQuarter * 8;   // 2 whole time (duration == 0)
    while (Dur)
      {
        NoteDuration /= 2;
        Dur--;
      }
    if (Chord->flags & fDotDotted)
      NoteDuration = NoteDuration * 7 / 4;
    else if (Chord->flags & fDotted)
      NoteDuration = NoteDuration * 3 / 2;
    return NoteDuration;
  }

int BuffRiff (byte **Buf, _Riff *Riff, int Transpose, int Synth)
  {
    int Res;
    _Chord *Chord;
    int Pass;   // 0 => Note ON. 1 => Note OFF
    int s;   // String
    int Note, NoteDuration, Velocity;
    int Tie;
    int Synth_;
    //
    Synth_ = Synth;
    if (Synths [Synth].MIDIPercussion)
      Synth_ = 9;
    Res = 0;
    // Set Tempo
    Res += BuffTime (Buf);
    Res += BuffUInt (Buf, 0xFF5103, 3);
    Res += BuffUInt (Buf, 60000000 / Riff->Tempo, 3);
    Chord = Riff->Chords;
    Tie = 0;
    while (true)
      {
        NoteDuration = ChordTicks (Chord);   // Calculate Note Time in MIDI Ticks
        Velocity = (Synths [Synth].MixLeft + Synths [Synth].MixRight) * 127 / 200;
        if (NoteDuration == None)
          break;
        for (Pass = 0; Pass < 2; Pass++)
          {
            for (s = 0; s < MaxStrings; s++)
              if (Riff->Strings [s] != None)
                if (Chord->fret [s] != None)
                  {
                    Note = Riff->Strings [s] + Chord->fret [s] + Transpose;
                    if (Pass == 0)
                      if (Tie)
                        Tie--;
                      else
                        {
                          // Note ON
                          Res += BuffTime (Buf);
                          Res += BuffUInt (Buf, 0x90 + Synth_, 1);   // Note On Event
                          Res += BuffUInt (Buf, Note + 48, 1);   // Actual Note
                          Res += BuffUInt (Buf, Velocity, 1);   // Actual Velocity
                        }
                    else   // Pass == 1
                      if (Chord->flags & fTie)   // note not tied
                        Tie++;
                      else
                        {
                          // Note OFF
                          Res += BuffTime (Buf);
                          Res += BuffUInt (Buf, 0x90 + Synth_, 1);   // Note On Event
                          Res += BuffUInt (Buf, Note + 48, 1);   // Actual Note
                          Res += BuffUInt (Buf, 0x00, 1);   // Zero Velocity => Note OFF
                        }
                  }
            if (Pass == 0)
              MIDITime += NoteDuration;
            NoteDuration = 0;
          }
        Chord++;
      }
    return Res;
  }

int BuffProjectSynth (byte **Buf, int Synth, int NumSynths)   // Go thru Project Project for Synth [Track]
  {
    int Res;
    int Line;
    int TimeLine, TimeLineStart;
    int s;   // Synth - go thru all Synths to find longest part in a Line
    //int Pass;   // Pass=0 => Find lengths of all parts.  Pass=1 => Generate MIDI data
    int Transpose;
    char *Str, *Str_;
    int l, Reps, Riff;
    //
    Res = 0;
    Line = 0;
    MIDITime = MIDITime_ = 0;
    while (Line < NumProjectItems)
      {
        TimeLine = 0;
        TimeLineStart = MIDITime;
        Str = Project [Line];
        if (Str && *Str)
          {
            // Transposed?
            Transpose = 0;
            Str_ = StrGetItem (Str, 0, &l);
            if (l)   // Transpose field not empty
              if (*Str_ == '[')   // transpose value
                {
                  Str_++;
                  Transpose = StrGetNum (&Str_);
                }
            for (s = 0; s < NumSynths; s++)
              {
                if (s == Synth)
                  {
                    // Set Instrument
                    Res += BuffTime (Buf);
                    if (Synths [Synth].MIDIPercussion)
                      Res += BuffUInt (Buf, 0xC0 + 9, 1);
                    else
                      Res += BuffUInt (Buf, 0xC0 + Synth, 1);
                    Res += BuffUInt (Buf, Synths [Synth].MIDIInstrument - 1, 1);
                    //DebugAddInt ("MIDI Instrument", Synths [Synth].MIDIInstrument); // ####
                  }
                // Get Repeat and Riff for Synth
                Reps = 0;
                Str_ = StrGetItem (Str, s * 2 + 1, &l);
                if (l)
                  Reps = StrGetNum (&Str_);
                if (Reps == 0)
                  Reps = 1;
                Str_ = StrGetItem (Str, s * 2 + 2, &l);
                Riff = -1;
                if (l)   // a Riff is defined
                  Riff = StrFindItem (&RiffNames, Str_);
                if (Riff >= 0)   // Riff found
                  {
                    TimeLine = Max (TimeLine, Reps * TimeInRiff (&Riffs [Riff], -1) * 4 * TicksPerQuarter / 1024);
                    if (s == Synth && !Synths [Synth].Mute)
                      while (Reps-- > 0)
                        Res += BuffRiff (Buf, &Riffs [Riff], Transpose, Synth);
                  }
              }
            MIDITime = TimeLineStart + TimeLine;   // Move MIDITime to match longest Part of the Line
          }
        Line++;
      }
    return Res;
  }

bool ExportMIDI (char *Filename)
  {
    int nTracks;
    int Track;
    int TrackEnd [MaxSynths];
    //
    int f;
    byte *Buffer, *b;
    int Len;
    char *s;
    int Size;
    //
    Buffer = b = NULL;
    Len = 0;
    nTracks = Min (StrCount (SynthNames, '\t') + 1, MaxSynths);
    while (true)
      {
        // Header Block
        Len += BuffStr (&b, MThd);
        Len += BuffUInt (&b, 6, 4);
        Len += BuffUInt (&b, 1, 2);
        Len += BuffUInt (&b, nTracks, 2);
        Len += BuffUInt (&b, TicksPerQuarter, 2);
        //
        for (Track = 0; Track < nTracks; Track++)
          {
            // Track Block
            Len += BuffStr (&b, MTrk);
            Len += BuffUInt (&b, TrackEnd [Track] - Len - 4, 4);   // Length of this Track (not including this)
            // MIDI Channel Prefix: #0
            //Len += BuffUInt (&b, 0x00, 1);
            //Len += BuffUInt (&b, 0xFF210100 + Track, 4);
            // Time signature (not accurate)
            Len += BuffUInt (&b, 0x00, 1);
            Len += BuffUInt (&b, 0xFF5804, 3);   // 0xFF580404021808
            Len += BuffUInt (&b, 0x04021808, 4);
            // Track Name (uses Synth Name)
            s = StrGetItem (SynthNames, Track, &Size);
            if (Size)
              {
                Len += BuffUInt (&b, 0x00, 1);
                Len += BuffUInt (&b, 0xFF03, 2);
                Len += BuffUInt (&b, Size, 1);
                Len += BuffData (&b, (byte *) s, Size);
              }
            // Output Project for Synth [Track]
            Len += BuffProjectSynth (&b, Track, nTracks);
            // End Track
            Len += BuffUInt (&b, 0x00, 1);
            Len += BuffUInt (&b, 0xFF2F00, 3);
            TrackEnd [Track] = Len;
          }
        if (b)
          break;
        Buffer = b = (byte *) malloc (Len);
        Len = 0;
      }
    f = FileOpen (Filename, foWrite);
    if (f >= 0)
      {
        FileWrite (f, Buffer, Len);
        FileClose (f);
      }
    if (Buffer)
      free (Buffer);
    return f >= 0;
  }
