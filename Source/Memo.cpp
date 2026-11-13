////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MEMO FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _FormMemo: public _Form
  {
    public:
      _EditPopup *eMemo;
      _FormMemo (_Point Pos);
     ~_FormMemo ();
  };

_FormMemo *fMemo = NULL;

void ActionMemoChanged (_Container *Memo)
  {
    Changed = true;
  }

_FormMemo::_FormMemo (_Point Pos) : _Form ("Memo", {Pos.x, Pos.y, 400, 400}, waResizable | waAlwaysOnTop)
  {
    Container->FontSet (NULL);
    eMemo = new _EditPopup (Container, {0}, Memo, ActionMemoChanged, true);
  }

_FormMemo::~_FormMemo ()
  {
    if (Changed)
      StrAssignCopy (&Memo, eMemo->TextGet ());
    fMemo = NULL;
  }

void ActionMemo (_Container *Button)
  {
    if (fMemo == NULL)
      fMemo = new _FormMemo (MousePos ());
  }

