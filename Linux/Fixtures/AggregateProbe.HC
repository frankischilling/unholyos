extern class AggNode;
AggNode *agg_head;
class AggNode
{
  I8 value;
  AggNode *next;
};

AggNode agg_list[2]={{-3,0},{7,0}};
agg_head=&agg_list[0];
agg_head->next=&agg_list[1];

class AggItem
{
  U8 kind;
  I32 value;
  U8 name[4];
};

class AggDerived:AggItem
{
  U16 number;
} agg_derived;

class AggMixed
{
  U8 lead;
  union {
    U16 low;
    U32 full;
  };
  U8 tail;
} agg_mixed;

U32i union AggBits
{
  U8 bytes[4];
  U16 words[2];
};

AggBits agg_bits=0xA1B2C3D4;
I64 agg_order=0;
I64 AggMark(I64 n)
{
  agg_order=10*agg_order+n;
  return agg_order;
}

AggItem agg_items[2]={{1,AggMark(1),"one"},{2,AggMark(2),"two"}};
I32 agg_grid[3][2]={{10,20},{30,40},{50,60}};
U8 agg_text[4]="hi" "!";
U16 agg_omitted[];
U8 agg_zero_text[]="ABC";

I64 AggStatic()
{
  static U16 values[2]={2,3};
  return ++values[0]+10*values[1];
}

I64 AggLocal()
{
  AggDerived local;
  local.kind=17;
  local.number=257;
  return local.kind+local.number;
}

agg_derived.kind=4;
agg_derived.number=123;
agg_mixed.full=0x12345678;
agg_mixed.tail=0xEF;
"node %d %d %d %d\n",sizeof(AggNode),offset(AggNode.next),agg_head->value,agg_head->next->value;
"class %d %d %d %d %d\n",sizeof(AggItem),offset(AggItem.value),sizeof(AggDerived),
    offset(AggDerived.number),agg_derived.kind+agg_derived.number;
"union %d %d %d %04X %02X %02X %04X\n",sizeof(AggMixed),offset(AggMixed.low),
    offset(AggMixed.tail),agg_mixed.low,agg_mixed.tail,agg_bits.bytes[2],agg_bits.words[1];
"init %d %d %d %d %s %s\n",agg_order,agg_items[0].value,agg_items[1].value,
    sizeof(agg_items),agg_items[0].name,agg_items[1].name;
"array %d %d %d %s\n",sizeof(agg_grid),agg_grid[2][1],sizeof(agg_text),agg_text;
"omitted %d %d\n",sizeof(agg_omitted),sizeof(agg_zero_text);
"local %d static %d\n",AggLocal(),AggStatic();
"static %d\n",AggStatic();
