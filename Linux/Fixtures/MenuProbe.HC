U0 MenuProbe()
{
  CMenu *old=Fs->cur_menu,*m=MenuPush("Base { Run(,'a'); }");
  CMenu *n=MenuFilePush("::/MenuProbe.DD");
  CMenuEntry *open=MenuEntryFind(n,"File/Open");
  CMenuEntry *item=MenuEntryFind(n,"File/Nested/Item");
  "menu layout %d %d %d %d\n",sizeof(CMenu),sizeof(CMenuEntry),
    offset(CMenu.attr),offset(CMenuEntry.checked);
  "menu stack %d %d %d\n",n==Fs->cur_menu,n->next==m,n->task==Fs;
  "menu open %s %d %d %d\n",open->name,open->msg_code,open->arg1,open->arg2;
  "menu item %s %d %d %d\n",item->name,item->msg_code,item->arg1,item->arg2;
  MenuPop;
  "menu base %d %d\n",Fs->cur_menu==m,MenuEntryFind(m,"Base/Run")!=NULL;
  MenuPop;
  "menu restore %d\n",Fs->cur_menu==old;
}

MenuProbe;
