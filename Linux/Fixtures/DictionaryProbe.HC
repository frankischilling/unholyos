U0 DictionaryProbe()
{
  I64 result=0;
  U8 *word=ACDWordPtAt("Albatros"),*defs=ACDDefsGet("ALBATROSS"),
     *first=ACDDefGet("albatross"),*second=ACDDefGet("apple",2),
     *missing=ACDDefGet("albatross",999),*absent=ACDDefsGet("ZzzzNotAWord");
  if (word && *word==ACD_WORD_CHAR && !StrCmp(word+1,"Albatross"))
    result|=1;
  if (defs && *defs==ACD_WORD_CHAR && !StrCmp(defs+1,"Albatross"))
    result|=2;
  if (first && StrMatch("A web-footed bird",first)==first)
    result|=4;
  if (second && *second)
    result|=8;
  if (!missing && !absent)
    result|=16;
  if (acd.word_lst && acd.num_words==95293 && acd.word_lst_size>acd.num_words &&
      acd.has_words && acd.has_defs && sizeof(CAutoCompleteDictGlbls)==114)
    result|=32;
  else
    "Dictionary state %d %d %d %d %d\n",acd.word_lst_size,acd.num_words,
      acd.has_words,acd.has_defs,sizeof(CAutoCompleteDictGlbls);
  Free(defs);
  Free(first);
  Free(second);
  Free(missing);
  Free(absent);
  "Dictionary probe %X\n",result;
}

DictionaryProbe;
