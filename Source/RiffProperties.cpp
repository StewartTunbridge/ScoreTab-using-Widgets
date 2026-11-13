////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RIFF PROPERTIES FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormRiffProperties: public _Form
  {
    public:
      _Label *lName;
      _EditPopup *eName;
      _Section *sTempo;
        _Knob *kTempo;
        _Button *bTempoAll;
      _Section *sTimeSigKey;
        _Edit *eTimeA;
        _DropList *dlTimeB;
        _DropList *dlKey;
        _Button *bTimeSigKeyAll;
      _Section *sStrings;
        _Button *StringEnable [MaxStrings];
        _Slider *StringTuning [MaxStrings];
      //_Button *bCopy, *bPaste;
      _MenuPopup *Menu;
      //
      _FormRiffProperties (_Point Pos);
     ~_FormRiffProperties (void);
      void Fill (void);
  };

_FormRiffProperties *fRiffProperties = NULL;

void ActionRiffPropertiesChange (_Container *Container)
  {
    _FormRiffProperties *f;
    char *s1;
    //
    f = (_FormRiffProperties *) Container->Form;
    s1 = fRiffProperties->eName->TextGet ();
    if (StrLen (s1) > 0)
      StrSetItem (&RiffNames, fMain->tRiff->Selected, s1);
    //
    RiffCur->Tempo = f->kTempo->ValueGet ();
    StrAssignCopy (&RiffCur->TimeSigA, f->eTimeA->TextGet ());
    RiffCur->TimeSigB = Exp2 (f->dlTimeB->Selected);
    RiffCur->Key = f->dlKey->Selected - 6;
    Changed = true;
    Redraw = true;
  }

void ActionTempoAll (_Button *Button)
  {
    int i;
    //
    for (i = 0; i < MaxRiffs; i++)
      Riffs [i].Tempo = RiffCur->Tempo;
  }

void ActionTimeSigKeyAll (_Button *Button)
  {
    int i;
    //
    for (i = 0; i < MaxRiffs; i++)
      if (&Riffs [i] != RiffCur)
        {
          StrAssignCopy (&Riffs [i].TimeSigA, RiffCur->TimeSigA);
          Riffs [i].TimeSigB = RiffCur->TimeSigB;
          Riffs [i].Key = RiffCur->Key;
        }
  }

void StringButtonTextSet (_Slider *String)
  {
    _Button *b;
    int Val, Oct;
    char St [32], *s;
    //
    b = (_Button *) String->DataContainer;
    s = St;
    Val = String->ValueGet ();
    Oct = 4;
    while (Val >= 12)
      {
        Val -= 12;
        Oct++;
      }
    while (Val < 0)
      {
        Val += 12;
        Oct--;
      }
    StrCat (&s, NoteName [Val]);
    IntToStr (&s, Oct);
    *s = 0;
    b->TextSet (St);
  }

void StringSet (_Slider *String, int Val)
  {
    _Button *b;
    //
    b = (_Button *) String->DataContainer;
    b->DownSet (Val != None);
    String->EnabledSet (Val != None);
    if (Val != None)
      {
        String->ValueSet (Val);
        StringButtonTextSet (String);
      }
  }

void ActionRiffStringSlider (_Slider *Slider)
  {
    StringButtonTextSet (Slider);
    RiffCur->Strings [Slider->DataInt] = Slider->ValueGet ();
    Changed = true;
    Redraw = true;
  }

void ActionRiffStringButton (_Button *Button)
  {
    _Slider *Slider;
    //
    Slider = (_Slider *) Button->DataContainer;
    Slider->EnabledSet (Button->Down);
    if (!Button->Down)   // string disabled
      RiffCur->Strings [Slider->DataInt] = None;
    else
      RiffCur->Strings [Slider->DataInt] = Slider->ValueGet ();
    Changed = true;
    Redraw = true;
  }

void ActionRiffPopup (_MenuPopup *Menu)
  {
    _FormRiffProperties *f;
    char *St, *s;
    int l;
    //
    f = (_FormRiffProperties *) Menu->Form;
    if (Menu->Selected == 0)   // Copy
      {
        St = s = NULL;
        while (true)
          {
            l = StrCat (&s, "T\t");
            l += RiffPropertiesToString (&s, RiffCur);
            if (St)
              break;
            St = s = (char *) malloc (l + 1);
          }
        *s = 0;
        ClipboardSet (f->Window, St);
        free (St);
      }
    else if (Menu->Selected == 1)   // Paste
      if (ClipboardGet (f->Window))
        {
          s = ClipboardData;
          if (StrMatch (&s, "T\t"))
            {
              StringToRiffProperties (&s, RiffCur);
              f->Fill ();
            }
          else
            Beep ();
        }
  }

void _FormRiffProperties::Fill (void)
  {
    int i;
    char *s;
    //
    s = StrGetItem (RiffNames, fMain->tRiff->Selected);
    eName->TextSet (s);
    free (s);
    kTempo->ValueSet (RiffCur->Tempo);
    eTimeA->TextSet (RiffCur->TimeSigA);
    dlTimeB->Selected = Log2 (RiffCur->TimeSigB);
    dlTimeB->Invalidate ();
    dlKey->Selected = RiffCur->Key + 6;
    dlKey->Invalidate ();
    for (i = 0; i < MaxStrings; i++)
      StringSet (StringTuning [i], RiffCur->Strings [i]);
    Redraw = true;
  }

_FormRiffProperties::_FormRiffProperties (_Point Pos) : _Form ("Riff Properties", {Pos.x, Pos.y, 400, 224}, waAlwaysOnTop)
  {
    const int Gap = 4;
    const int Col = 128;
    int th;
    int y;
    int i;
    char *s;
    //
    Container->FontSet (NULL);
    th = Container->Font->YAdvance + 8;
    Container->Bitmap = BitmapLoad (Window, "ScoreTab.bmp");
    //
    y = Gap;
    lName = new _Label (Container, {Gap, y, 0, 24}, "Name");
    s = StrGetItem (fMain->tRiff->TextGet (), fMain->tRiff->Selected);
    eName = new _EditPopup (Container, {Gap + 50, y, -4, 24}, s, ActionRiffPropertiesChange);
    free (s);
    y += 24 + Gap;
    //
    Menu = new _MenuPopup (Container, {0, y, 0, 0}, "\e4 Copy\t\e5 Paste", (_Action) ActionRiffPopup);
    //
    sTempo = new _Section (Container, {Gap, y, Col, 80}, "\abTempo (bpm)\ab", cGreenDark);
    kTempo = new _Knob (sTempo, {Gap, th + Gap, 40, 50}, "%%", (_Action) ActionRiffPropertiesChange, 20, 200);
    kTempo->ValueSet (RiffCur->Tempo);
    bTempoAll = new _Button (sTempo, {Gap + kTempo->Rect.Width + Gap, th + Gap, 48, 48}, "All Riffs", (_Action) ActionTempoAll);
    //
    y += sTempo->Rect.Height + Gap;
    sTimeSigKey = new _Section (Container, {Gap, y, Col, th + 4 * Gap + 2 * 24}, "\abTime Sig & Key\ab", cBlueDark);
    eTimeA = new _Edit (sTimeSigKey, {Gap, th + Gap, 48, 24}, RiffCur->TimeSigA, ActionRiffPropertiesChange);
    dlTimeB = new _DropList (sTimeSigKey, {Gap, th + Gap + 24 + Gap, 48, 24}, "1\t2\t4\t8\t16", ActionRiffPropertiesChange);
    dlTimeB->Selected = Log2 (RiffCur->TimeSigB);
    dlKey = new _DropList (sTimeSigKey, {Gap + eTimeA->Rect.Width + Gap, th + Gap, 64, 24}, (char *) KeyNames, ActionRiffPropertiesChange);
    dlKey->Selected = RiffCur->Key + 6;
    bTimeSigKeyAll = new _Button (sTimeSigKey, {dlKey->Rect.x, dlTimeB->Rect.y, -Gap, 24}, "All Riffs", (_Action) ActionTimeSigKeyAll);
    //
    sStrings = new _Section (Container, {Gap + Col + Gap, sTempo->Rect.y, -Gap, -Gap}, "\abStrings\ab", cRedDark);
    y = th + Gap;
    for (i = MaxStrings - 1; i >= 0; i--)
      {
        StringEnable [i] = new _Button (sStrings, {Gap, y, 28, 16}, NULL, (_Action) ActionRiffStringButton);
        StringEnable [i]->Toggle = true;
        StringTuning [i] = new _Slider (sStrings, {Gap + 28 + Gap, y, -Gap, 16}, NULL, (_Action) ActionRiffStringSlider, -18, 16);
        StringTuning [i]->DataInt = i;
        StringTuning [i]->DataContainer = StringEnable [i];
        StringEnable [i]->DataContainer = StringTuning [i];
        y += 16 + Gap;
      }
    //
    Fill ();
    //
    //bCopy = new _Button (Container, {0, -24, 64, 0}, "Copy", ActionRiffCopy);
    //bPaste = new _Button (Container, {64, -24, 64, 0}, "Paste", ActionRiffPaste);
  }

_FormRiffProperties::~_FormRiffProperties ()
  {
    fRiffProperties = NULL;
  }

