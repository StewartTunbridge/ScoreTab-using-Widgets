////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SYNTHESIZERS CONTROL FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ENVELOPE VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _Envelope: public _Section
  {
    public:
      _Action Action;
      _Slider *ADSR [4];   // Attack, Decay, Sustain, Release
      int Changed;   // Which SliderLabel changed (0..3)
      _Envelope (_Container *Parent, _Rect Rect, char *Title, int ColourTitle, _Action Action_ = NULL);
  };

void ActionEnvelope (_Slider *Slider)
  {
    _Envelope *Env;
    //
    Env = (_Envelope *) Slider->Parent;
    Env->Changed = Slider->DataInt;
    if (Env->Action)
      Env->Action (Env);
  }

const char *Desc [] = {"Attack\n%%", "Decay\n%%", "Sustain\n%%", "Release\n%%"};

_Envelope::_Envelope (_Container *Parent, _Rect Rect, char *Title, int ColourTitle, _Action Action_)
  : _Section (Parent, Rect, Title, ColourTitle)
  {
    const int Gap = 4;
    int th;
    int i, w, x;
    //
    Action = Action_;
    th = FontFind ()->YAdvance;
    x = Gap;
    w = (Rect.Width - Gap) / 4 - Gap;
    for (i = 0; i < 4; i++)
      {
        ADSR [i] = new _Slider (this, {Gap + i * w, th + 8, w, -Gap}, (char *) Desc [i], (_Action) ActionEnvelope, 0, 1000);
        ADSR [i]->Decimals = 1;
        ADSR [i]->Sections = 10;
        ADSR [i]->DataInt = i;
        x += w;
      }
    Changed = -1;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WAVEFORM SCOPE COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

const int XScale [] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, -1};

class _Scope: public _Container
  {
    public:
      int ScaleX, ScaleY;
      short *Sound;
      int SoundSize;
      //
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
      _Scope (_Container *Parent, _Rect Rect);
     ~_Scope ();
  };

_Scope::_Scope (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    Colour = cBlack;
    ScaleX = 0;   // Index into XScale
    ScaleY = 1;
    Sound = NULL;
    SoundSize = 0;
  }

_Scope::~_Scope ()
  {
    free (Sound);
  }

bool _Scope::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    return false;
  }

void _Scope::DrawCustom (void)
  {
    int h;
    _Point p;
    int i, x;
    int Val;
    //
    SizeVirtual.x = SoundSize / 2 / XScale [ScaleX];
    // Center horizontal line
    h = Rect.Height / 2;
    // Hor & Vertical lines
    i = 0;
    while (h - i >= 0)
      {
        DrawLine (Scroll.x, h + i, Scroll.x + Rect.Width, h + i, cGreenDark, 1);
        DrawLine (Scroll.x, h - i, Scroll.x + Rect.Width, h - i, cGreenDark, 1);
        i += 16;
      }
    x = 0;
    while (x < Rect.Width)
      {
        DrawLine (Scroll.x + x, 0, Scroll.x + x, Rect.Height, cGreenDark, 1);
        x += 16;
      }
    p = {0, h};
    i = 0;
    for (x = 0; x < SizeVirtual.x; x++)
      if (i * 2 + 1 < SoundSize)
        {
          Val = (Sound [i * 2 + 0] + Sound [i * 2 + 1]) * h * ScaleY / 0x7FFF;
          DrawLine (p.x, p.y, x, h - Val, cYellow);
          p = {x, h - Val};
          i += XScale [ScaleX];
        }
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SYNTHS DEFINITIONS FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormSynths: public _Form
  {
    public:
      _Tabs *tSynths;
      _MenuPopup *mSynths;
      _Container *cSettings;
        _Section *sMix;
          _CheckBox *cbEnabled;
          _Slider *sMixLeft, *sMixBoth, *sMixRight;
        _Envelope *EnvelopeAmplitude;
        _Envelope *EnvelopeFilter;
        _Section *sWave;
          _ButtonRadio *bSine, *bTriangle, *bSquare, *bNoise;
          _Knob *kOctave, *kSkew;
        _Section *sModulation;
          _ButtonRadio *bPitch, *bFilter, *bAmplitude;
          _Knob *kModFrequency, *kModDepth;
        _Section *sStyle;
          _Knob *kStrum, *kDither;
        //_Knob *kMasterVolume;
        _Section *sMIDI;
          _EditNumber *eMIDIInstrument;
          _CheckBox *cbMIDIPercussion;
        _MenuPopup *Menu;
      //
      _ButtonArrow *bPlay, *bUp, *bDown, *bLeft, *bRight;
      _Scope *Scope;
      //
      _EditPopup *eName;
      //
      _FormSynths (_Point Pos);
     ~_FormSynths ();
      void LoadForm (_Synth *Synth);
      void ReadForm (_Synth *Synth);
  };

_FormSynths *fSynths = NULL;

void ActionSynthSelect (_Tabs *Synth)
  {
    SynthTab = Synth->Selected;
    ((_FormSynths *) (Synth->Form))->LoadForm (&Synths [SynthTab]);
  }

inline int ColToItem (int Col)
  {
    return 1 + 2 * Col;
  }

void ActionSynthTabDrag (int From, int To)
  {
    int i;
    //
    DebugAddP ("Tab Drag", {From, To}); // ####
    // Update SynthNames & Synths []
    StrAssignCopy (&SynthNames, fSynths->tSynths->TextGet ());
    SwapBytes (&Synths [From], &Synths [To], sizeof (_Synth));
    // Update
    for (i = 0; i < NumProjectItems; i++)
      if (To > From)
        {
          StrItemMove (Project [i], ColToItem (From), ColToItem (To) + 1);
          StrItemMove (Project [i], ColToItem (From), ColToItem (To) + 1);
        }
      else
        {
          StrItemMove (Project [i], ColToItem (From) + 1, ColToItem (To));
          StrItemMove (Project [i], ColToItem (From) + 1, ColToItem (To));
        }
    SynthNamesChanged = true;
  }

void ActionSynthDelete (int Result, void *Parameter)
  {
    int n;
    //
    if (Result == 0)   // YES DELETE
      {
        StrRemoveItem (SynthNames, SynthTab);
        SynthNamesChanged = true;
        fSynths->tSynths->TextSet (SynthNames);
        n = SynthTab;
        while (n + 1 < MaxSynths)
          {
            Synths [n] = Synths [n + 1];
            n++;
          }
        if (SynthTab)
          fSynths->tSynths->Selected = fSynths->tSynths->Selected - 1;
        fSynths->tSynths->Invalidate ();
        SynthTab = -1;
        ActionSynthSelect (fSynths->tSynths);
      }
  }

void ActionSynthMenu (_MenuPopup *Menu)
  {
    char *St, *s;
    //
    switch (Menu->Selected)
      {
        case 0: // Add Synth
          s = St = (char *) malloc (StrLen (SynthNames) + 32);
          StrCat (&s, SynthNames);
          StrCat (&s, "\tSynth");
          NumToStr (&s, StrCount (SynthNames, '\t') + 2);
          *s = 0;
          StrAssign (&SynthNames, St);
          SynthNamesChanged = true;
          fSynths->tSynths->TextSet (SynthNames);
          break;
        case 1: // Delete Synth
          if (StrCount (fSynths->tSynths->TextGet (), '\t') > 0)   // more than 1 Synth defined
            {
              MessageResponseBox ({300, 100}, "Delete this Synth?", "Yes\tNo", ActionSynthDelete, NULL);
              fMessageResponseBox->Container->Colour = cRedLight;
              fMessageResponseBox->Container->ColourGrad = -1;
            }
          else
            Beep ();
          break;
        case 2: // Rename
          s = StrGetItem (SynthNames, SynthTab);
          fSynths->eName->TextSet (s);
          free (s);
          fSynths->eName->VisibleSet (true);
          fSynths->eName->FocusSet ();
          fSynths->EventLock = fSynths->eName;
          break;
      }
  }

void ActionSynthChange (_Container *Container)
  {
    fSynths->ReadForm (&Synths [SynthTab]);
    Changed = true;
  }

void ActionSynthEnv (_Envelope *Env)
  {
    TEnvelope *e;
    //
    e = &Synths [SynthTab].Amplitude;
    if (Env->DataInt)
      e = &Synths [SynthTab].Filter;
    e->Attack = Env->ADSR [0]->ValueGet ();   // we could use ADSR [Changed] if TEnvelope used an array
    e->Decay = Env->ADSR [1]->ValueGet ();
    e->Sustain = Env->ADSR [2]->ValueGet ();
    e->Release = Env->ADSR [3]->ValueGet ();
    Changed = true;
  }

void ActionSynthWave (_Button *Button)
  {
    Synths [SynthTab].WaveForm = (TWaveForm) Button->DataInt;
    Changed = true;
  }

void ActionSynthMod (_Button *Button)
  {
    TModulateMode m;
    //
    m = mNone;
    if (Button->Down)
      m = (TModulateMode) Button->DataInt;
    Synths [SynthTab].Modulate.Mode = m;
    Changed = true;
  }

void ActionSynthMixBoth (_Slider *Mix)
  {
    fSynths->sMixLeft->ValueSet (Mix->ValueGet ());
    fSynths->sMixRight->ValueSet (Mix->ValueGet ());
    ActionSynthChange (NULL);
  }

void ActionSynthPlay (_ButtonArrow *Button)
  {
    short *block;
    int l;
    ///////////////////////////////////////////////////////
    l = NoteLength (RiffCur->Tempo, Duration1024 [RiffDur]) + 4 * PlayTrail;
    block = (short *) malloc (l * 4);
    memset (block, 0, l * 4);
    NoteBuffer (MasterVolume, RiffCur->Tempo, 0, Duration1024 [RiffDur], &Synths [SynthTab], block, 0, l * 2);
    //if (fSynths->Scope->Sound)
      free (fSynths->Scope->Sound);
    fSynths->Scope->Sound = (short *) malloc (l * 4);
    memcpy (fSynths->Scope->Sound, block, l * 4);
    fSynths->Scope->SoundSize = l * 2;
    fSynths->Scope->Invalidate ();
    PlayNote (0);
  }

void ActionSynthScale (_ButtonArrow *Button)
  {
    if (Button->Direction == dUp)
      fSynths->Scope->ScaleY++;
    else if (Button->Direction == dDown)
      fSynths->Scope->ScaleY = Max (fSynths->Scope->ScaleY - 1, 1);
    else if (Button->Direction == dLeft)
      {
        if (XScale [fSynths->Scope->ScaleX + 1] > 0)
          fSynths->Scope->ScaleX++;
      }
    else if (Button->Direction == dRight)
      if (fSynths->Scope->ScaleX > 1)
        fSynths->Scope->ScaleX--;
    fSynths->Scope->Invalidate ();
  }

void ActionSynthPopup (_MenuPopup *Menu)
  {
    char St [128], *s;
    //
    //DebugAdd ("ActionSynthPopup");  // ####
    if (Menu->Selected == 0)   // Copy
      {
        s = St;
        StrCat (&s, "I\t");
        SynthToStr (&s, &Synths [SynthTab]);
        *s = 0;
        ClipboardSet (Menu->Form->Window, St);
      }
    else if (Menu->Selected == 1)   // Paste
      {
        if (ClipboardGet (Menu->Form->Window))
          {
            s = ClipboardData;
            if (StrMatch (&s, "I\t"))
              {
                StrToSynth (&s, &Synths [SynthTab]);
                fSynths->LoadForm (&Synths [SynthTab]);
              }
            else
              Beep ();
          }
      }
  }

bool SynthsEventPreview (_Form *Form, _Event *Event)
  {
    if (fSynths->eName->IsVisible ())
      {
        if (Event->Type == etKeyDown && (Event->Key == KeyEnter || Event->Key == '\e'))
          {
            fSynths->eName->VisibleSet (false);
            if (Event->Key == KeyEnter)
              {
                StrSetItem (&SynthNames, SynthTab, fSynths->eName->TextGet ());
                fSynths->tSynths->TextSet (SynthNames);
                SynthNamesChanged = true;
              }
            return true;
          }
      }
    return false;
  }

_FormSynths::_FormSynths (_Point Pos) : _Form ("Synthesizers Setup", {Pos.x, Pos.y, 500, 450}, waResizable | waAlwaysOnTop)
  {
    const int bw = 70;
    const int bh = 20;
    const int kw = 40;
    const int kh = 68;
    const int slw = 32;
    //
    const int Gap = 4;
    const int ew = 200;
    const int eh = 148;
    const int ww = bw + 2 * kw + 4 * Gap;
    const int wh = 104;
    const int mw = ww;
    const int mh = wh - 10;
    const int sw = 2 * kw + 2 * Gap;
    const int sh = 94;
    //
    int th;
    int x, y, xx, yy;
    //
    Container->FontSet (NULL);
    th = Container->Font->YAdvance;
    Container->Bitmap = BitmapLoad (Window, "ScoreTab.bmp");
    //
    tSynths = new _Tabs (Container, {0, 0, 0, 32}, SynthNames, (_Action) ActionSynthSelect, ActionSynthTabDrag);
    mSynths = new _MenuPopup (tSynths, {0}, "\eQ Add Synth\t_\eR Delete Synth\t\eS Rename Synth", (_Action) ActionSynthMenu);
    //
    cSettings = new _Container (Container, {Gap, 32 + Gap, -Gap, 2 * eh + Gap});
    Menu = new _MenuPopup (cSettings, {0}, "\e4 Copy\t\e5 Paste", (_Action) ActionSynthPopup);
    xx = 0;
    yy = 0;
    //
    sMix = new _Section (cSettings, {xx, yy, slw * 9 / 4 + 2 * Gap, 0}, "\abMix\ab", ColourAdjust (cOrange, 100));
    cbEnabled = new _CheckBox (sMix, {-Gap - 12, Gap, 12, 16}, NULL, ActionSynthChange);
    x = Gap;
    y = th + 8;
    sMixLeft = new _Slider (sMix, {x, y, slw, -Gap}, "Left\n%%", ActionSynthChange, 0 , 100);
    sMixLeft->Sections = 10;
    x += slw;
    sMixBoth = new _Slider (sMix, {x, y, slw / 4, -Gap}, " \n ", (_Action) ActionSynthMixBoth, 0 , 100);
    sMixBoth->Sections = 10;
    x += slw / 4;
    sMixRight = new _Slider (sMix, {x, y, slw, -Gap}, "Right\n%%", ActionSynthChange, 0 , 100);
    sMixRight->Sections = 10;
    xx += sMix->Rect.Width + Gap;
    //
    EnvelopeAmplitude = new _Envelope (cSettings, {xx, yy, ew, eh}, "\abAmplitude Envelope\ab", cBlue, (_Action) ActionSynthEnv);
    EnvelopeAmplitude->DataInt = 0;
    EnvelopeFilter = new _Envelope (cSettings, {xx, yy + eh + Gap, ew, eh}, "\abFilter Envelope\ab", ColourAdjust (cBlue, 150), (_Action) ActionSynthEnv);
    EnvelopeFilter->DataInt = 1;
    xx += ew + Gap;
    //
    sWave = new _Section (cSettings, {xx, yy, ww, wh}, "\abWave Shape\ab", cGreenDark);
    x = Gap;
    y = th + 8;
    bSine = new _ButtonRadio (sWave, {x, y, bw, bh}, "Sine", (_Action) ActionSynthWave);
    bSine->DataInt = wSine;
    y += bh;
    bTriangle = new _ButtonRadio (sWave, {x, y, bw, bh}, "Triangle", (_Action) ActionSynthWave);
    bTriangle->DataInt = wTriangle;
    y += bh;
    bSquare = new _ButtonRadio (sWave, {x, y, bw, bh}, "Square", (_Action) ActionSynthWave);
    bSquare->DataInt = wSquare;
    y += bh;
    bNoise = new _ButtonRadio (sWave, {x, y, bw, bh}, "Noise", (_Action) ActionSynthWave);
    bNoise->DataInt = wRandom;
    y += bh;
    x += bw + Gap;
    y = th + 8;
    kOctave = new _Knob (sWave, {x, y, kw, kh}, "Octave\n%%", (_Action) ActionSynthChange, -8, 8);
    x += kw;
    kSkew = new _Knob (sWave, {x, y, kw, kh}, "Skew\n%%", (_Action) ActionSynthChange, 0, 100);
    yy += wh + Gap;
    //
    sModulation = new _Section (cSettings, {xx, yy, mw, mh}, "\abModulation\ab", cRedDark);
    x = Gap;
    y = th + 8;
    bPitch = new _ButtonRadio (sModulation, {x, y, bw, bh}, "Pitch", (_Action) ActionSynthMod);
    bPitch->Toggle = true;
    bPitch->DataInt = mPitch;
    y += bh;
    bFilter = new _ButtonRadio (sModulation, {x, y, bw, bh}, "Filter", (_Action) ActionSynthMod);
    bFilter->Toggle = true;
    bFilter->DataInt = mFilter;
    y += bh;
    bAmplitude = new _ButtonRadio (sModulation, {x, y, bw, bh}, "Amplitude", (_Action) ActionSynthMod);
    bAmplitude->Toggle = true;
    bAmplitude->DataInt = mAmp;
    x += bw + Gap;
    y = th + 8;
    kModFrequency = new _Knob (sModulation, {x, y, kw, kh}, "Freq\n%% Hz", ActionSynthChange, 0, 500);
    kModFrequency->Decimals = 1;
    x += kw;
    kModDepth = new _Knob (sModulation, {x, y, kw, kh}, "Depth\n%%%", ActionSynthChange, 0, 100);
    yy += mh + Gap;
    //
    sStyle = new _Section (cSettings, {xx, yy, sw, sh}, "\abStyle\ab", 0xC0B000); //cTealDark);
    x = Gap;
    y = th + 8;
    kStrum = new _Knob (sStyle, {x, y, kw, kh}, "Strum\n%%", ActionSynthChange, 0, 100);
    x += kw;
    kDither = new _Knob (sStyle, {x, y, kw, kh}, "Dither\n%%", ActionSynthChange, 0, 100);
    xx += sw + Gap;
    //
    sMIDI = new _Section (cSettings, {xx, yy, mw-sw-Gap}, "\abMIDI\ab", cGoldDark);
    x = Gap;
    y = th + 12;
    eMIDIInstrument = new _EditNumber (sMIDI, {x, y, -Gap, bh}, NULL, ActionSynthChange, 1, 128);
    y += bh;
    cbMIDIPercussion = new _CheckBox (sMIDI, {x, y, -Gap, bh}, "Drums", ActionSynthChange);
    //
    eName = new _EditPopup (tSynths, {Gap, 0, -Gap, 0}, NULL);
    eName->VisibleSet (false);
    //kMasterVolume = new _Knob (cSettings, {xx + sw, yy, 80, 90}, "Master %%", (_Action) ActionMaster, 0, 100);
    //kMasterVolume->ValueSet (MasterVolume);
    //
    yy = cSettings->Rect.y + cSettings->Rect.Height + Gap;
    x = Gap;
    y = 0;
    bPlay = new _ButtonArrow (Container, {x, yy + y, 32, 32}, NULL, (_Action) ActionSynthPlay, dRight); y += 32;
    bUp = new _ButtonArrow (Container, {x, yy + y, 16, 16}, NULL, (_Action) ActionSynthScale, dUp); x += 16;
    bDown = new _ButtonArrow (Container, {x, yy + y, 16, 16}, NULL, (_Action) ActionSynthScale, dDown); x -= 16; y += 16;
    bLeft = new _ButtonArrow (Container, {x, yy + y, 16, 16}, NULL, (_Action) ActionSynthScale, dLeft); x += 16;
    bRight = new _ButtonArrow (Container, {x, yy + y, 16, 16}, NULL, (_Action) ActionSynthScale, dRight);
    Scope = new _Scope (Container, {Gap + 32 + Gap, yy, -Gap, -Gap});
    //
    //
    SynthTab = -1;
    ActionSynthSelect (tSynths);
    //
    EventPreview = SynthsEventPreview;
  }

 _FormSynths::~_FormSynths ()
   {
     fSynths = NULL;
   }

void _FormSynths::LoadForm (_Synth *Synth)
  {
    sMixLeft->ValueSet (Synth->MixLeft);
    sMixRight->ValueSet (Synth->MixRight);
    cbEnabled->DownSet (!Synth->Mute);
    //
    EnvelopeAmplitude->ADSR [0]->ValueSet (Synth->Amplitude.Attack);
    EnvelopeAmplitude->ADSR [1]->ValueSet (Synth->Amplitude.Decay);
    EnvelopeAmplitude->ADSR [2]->ValueSet (Synth->Amplitude.Sustain);
    EnvelopeAmplitude->ADSR [3]->ValueSet (Synth->Amplitude.Release);
    EnvelopeFilter->ADSR [0]->ValueSet (Synth->Filter.Attack);
    EnvelopeFilter->ADSR [1]->ValueSet (Synth->Filter.Decay);
    EnvelopeFilter->ADSR [2]->ValueSet (Synth->Filter.Sustain);
    EnvelopeFilter->ADSR [3]->ValueSet (Synth->Filter.Release);
    //
    bSine->DownSet (Synth->WaveForm == wSine);
    bTriangle->DownSet (Synth->WaveForm == wTriangle);
    bSquare->DownSet (Synth->WaveForm == wSquare);
    bNoise->DownSet (Synth->WaveForm == wRandom);
    kOctave->ValueSet (Synth->Octave);
    kSkew->ValueSet (Synth->Skew);
    //
    bPitch->DownSet (Synth->Modulate.Mode == mPitch);
    bFilter->DownSet (Synth->Modulate.Mode == mFilter);
    bAmplitude->DownSet (Synth->Modulate.Mode == mAmp);
    kModFrequency->ValueSet (Synth->Modulate.Freq);
    kModDepth->ValueSet (Synth->Modulate.Depth);
    //
    kStrum->ValueSet (Synth->Strum);
    kDither->ValueSet (Synth->Dither);
    //
    eMIDIInstrument->ValueSet (Synth->MIDIInstrument);
    cbMIDIPercussion->DownSet (Synth->MIDIPercussion);
  }

void _FormSynths::ReadForm (_Synth *Synth)
  {
    Synth->MixLeft = sMixLeft->ValueGet ();
    Synth->MixRight = sMixRight->ValueGet ();
    Synth->Mute = !cbEnabled->Down;
    //
    Synth->Octave = kOctave->ValueGet ();
    Synth->Skew = kSkew->ValueGet ();
    //
    Synth->Modulate.Freq = kModFrequency->ValueGet ();
    Synth->Modulate.Depth = kModDepth->ValueGet ();
    //
    Synth->Strum = kStrum->ValueGet ();
    Synth->Dither = kDither->ValueGet ();
    //
    Synth->MIDIInstrument = eMIDIInstrument->Value;
    Synth->MIDIPercussion = cbMIDIPercussion->Down;
  }
