#define POPUP_BASE 20

class CPopupProbe
{
  I64 value format "$$DA,A=\"Popup Value:%d\"$$\n";
};

U0 EndPopupProbe(CTask *owner)
{
  CTask *popup;
  do {
    Yield;
    popup=owner->popup_task;
  } while (!popup || !Bt(&popup->task_flags,TASKf_AWAITING_MSG));
  Kill(popup,FALSE);
}

U0 PopupProbe()
{
  CPopupProbe form;
  I64 res=0,flags[2],old_flags=sys_msg_flags[0],selected;
  CDoc *doc;
  CTask *popup=NULL;
  flags[0]=flags[1]=0;
  form.value=7;
  if (PopUpPrint("%d*%d;",6,7)==42 &&
      Fs->popup_task==NULL && Fs==sys_focus_task) res|=1;
  if (!PopUp("Sleep(20);",,&popup) && popup) {
    Sleep(100);
    if (!TaskValidate(popup)) res|=2;
  }
  doc=DocNew;
  DocPrint(doc,"$$BT,\"Menu Alpha\",LE=POPUP_BASE+11$$\n"
              "$$BT,\"Menu Beta\",LE=(POPUP_BASE*2+7),U=0$$\n");
  selected=PopUpMenu(doc);
  if (selected==47 && Fs->popup_task==NULL && Fs==sys_focus_task) res|=4;
  DocDel(doc);
  doc=DocNew;
  DocPrint(doc,"$$BT,\"Cancel menu\",LE=31$$\n");
  if (PopUpMenu(doc)==DOCM_CANCEL) res|=8;
  DocDel(doc);
  if (PopUpForm(&form) && form.value==7 && Fs->popup_task==NULL) res|=16;
  doc=DocNew;
  DocPrint(doc,"$$BT,\"Intercept end\",LE=31$$\n");
  if (Spawn(&EndPopupProbe,Fs) &&
      PopUpMenu(doc,DOF_INTERCEPT_TASK_END)==DOCM_CANCEL &&
      Fs->popup_task==NULL && !doc->owning_task) res|=256;
  DocDel(doc);
  if (OneTimePopUp(flags,70,"One-time probe\n") && flags[1]==64 &&
      !OneTimePopUp(flags,70,"Should not appear\n")) res|=32;
  if (!OneTimePopUp(flags,69,"One-time cancel\n") && flags[1]==64) res|=64;
  LBtr(sys_msg_flags,62);
  RegOneTimePopUp(62,"Registry Notice\n");
  if (Bt(sys_msg_flags,62)) {
    sys_msg_flags[0]=0;
    RegExe("Adam/SysMsgFlags");
    if (Bt(sys_msg_flags,62)) res|=128;
  }
  RegWrite("Adam/SysMsgFlags","sys_msg_flags[0]=0x%X;\n",old_flags);
  sys_msg_flags[0]=old_flags;
  "Popup probe %X\n",res;
}

PopupProbe;
