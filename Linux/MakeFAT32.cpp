#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <algorithm>

// A small, sparse FAT32 disk with two mirrored FATs and a primary MBR partition.
constexpr uint32_t sector_size=512, spc=2, reserved=32, fat_sects=513,
                   clusters=65536, part=2048, first=reserved+2*fat_sects,
                   total=first+clusters*spc;
using Sector=std::array<uint8_t,sector_size>;
using Cluster=std::array<uint8_t,sector_size*spc>;

static void Put16(uint8_t *p,uint16_t n)
{
  p[0]=n;
  p[1]=n>>8;
}

static void Put32(uint8_t *p,uint32_t n)
{
  Put16(p,n);
  Put16(p+2,n>>16);
}

template<class Bytes>
static void Write(std::ofstream &out,uint64_t sector,const Bytes &bytes)
{
  out.seekp(sector*sector_size);
  out.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
  if (!out)
    throw std::runtime_error("Cannot write FAT32 fixture");
}

static uint64_t ClusSector(uint32_t c)
{
  return part+first+(c-2)*spc;
}

static void DirEntry(uint8_t *p,std::string_view name,uint8_t attr,
                     uint32_t clus,uint32_t size)
{
  if (name.size()!=11)
    throw std::runtime_error("Invalid fixture short name");
  std::copy(name.begin(),name.end(),p);
  p[11]=attr;
  Put16(p+20,clus>>16);
  Put16(p+22,(12<<11)|(34<<5)|28);
  Put16(p+24,((2024-1980)<<9)|(2<<5)|29);
  Put16(p+26,clus);
  Put32(p+28,size);
}

static void LongEntry(uint8_t *p,std::string_view name,std::string_view alias,
                      unsigned ord,bool last)
{
  constexpr uint8_t offsets[]{1,3,5,7,9,14,16,18,20,22,24,28,30};
  p[0]=ord|(last?0x40:0);
  p[11]=0x0F;
  uint8_t sum=0;
  for (uint8_t ch:alias)
    sum=(sum>>1)+(sum&1?0x80:0)+ch;
  p[13]=sum;
  for (unsigned i=0;i<13;i++) {
    size_t index=(ord-1)*13+i;
    Put16(p+offsets[i],index<name.size()?uint8_t(name[index]):
          index==name.size()?0:0xFFFF);
  }
}

static void Data(std::ofstream &out,std::initializer_list<uint32_t> chain,
                 unsigned tag)
{
  uint32_t pos=0;
  for (auto c:chain) {
    Cluster bytes;
    for (auto &b:bytes) {
      b=pos*37+(pos>>8)+tag;
      pos++;
    }
    Write(out,ClusSector(c),bytes);
  }
}

int main(int argc,char **argv)
{
  try {
    std::string_view mode=argc==3?argv[2]:"";
    if (argc!=2 && (argc!=3 || (mode!="invalid" && mode!="writable"))) {
      std::cerr<<"Usage: MakeFAT32 disk.img [invalid|writable]\n";
      return 2;
    }
    std::ofstream out(argv[1],std::ios::binary|std::ios::trunc);
    out.seekp(uint64_t(part+total)*sector_size-1);
    out.put(0);
    Sector mbr{};
    mbr[450]=0x0C;
    mbr[447]=mbr[451]=0xFE;
    mbr[448]=mbr[449]=mbr[452]=mbr[453]=0xFF;
    Put32(mbr.data()+454,part);
    Put32(mbr.data()+458,total);
    Put16(mbr.data()+510,0xAA55);
    Write(out,0,mbr);
    Sector boot{};
    boot[0]=0xEB;boot[1]=0x58;boot[2]=0x90;
    std::copy_n("MSWIN4.1",8,boot.data()+3);
    Put16(boot.data()+11,sector_size);
    boot[13]=mode=="invalid"?3:spc;
    Put16(boot.data()+14,reserved);
    boot[16]=2;boot[21]=0xF8;
    Put16(boot.data()+24,63);Put16(boot.data()+26,255);
    Put32(boot.data()+28,part);Put32(boot.data()+32,total);
    Put32(boot.data()+36,fat_sects);Put32(boot.data()+44,2);
    Put16(boot.data()+48,1);Put16(boot.data()+50,6);
    boot[64]=0x80;boot[66]=0x29;
    Put32(boot.data()+67,0x12345678);
    std::copy_n("FATPROBE   ",11,boot.data()+71);
    std::copy_n("FAT32   ",8,boot.data()+82);
    Put16(boot.data()+510,0xAA55);
    Write(out,part,boot);Write(out,part+6,boot);
    Sector info{};
    Put32(info.data(),0x41615252);Put32(info.data()+484,0x61417272);
    Put32(info.data()+488,0xFFFFFFFF);Put32(info.data()+492,mode=="writable"?18:0xFFFFFFFF);
    Put32(info.data()+508,0xAA550000);
    Write(out,part+1,info);Write(out,part+7,info);

    constexpr uint32_t end=0x0FFFFFFF;
    std::vector<uint8_t> fat(fat_sects*sector_size);
    constexpr std::array<uint32_t,19> links{
      0x0FFFFFF8,end,9,end,0,12,0,end,end,end,0,end,7,0,15,end,16,1,end};
    for (size_t i=0;i<links.size();i++)
      Put32(fat.data()+4*i,mode=="writable" && i>=16?0:links[i]);
    Write(out,part+reserved,fat);Write(out,part+reserved+fat_sects,fat);

    Cluster root{},root2{},sub{};
    DirEntry(root.data(),"FATPROBE   ",8,0,0);
    DirEntry(root.data()+32,"FRAG    BIN",0x20,5,2501);
    DirEntry(root.data()+64,"EMPTY   TXT",0x20,0,0);
    DirEntry(root.data()+96,"SUB        ",0x10,3,0);
    for (size_t i=4;i<31;i++)
      root[i*32]=0xE5;
    constexpr std::string_view long_name="LongNameExample.txt",alias="LONGNA~1TXT";
    LongEntry(root.data()+31*32,long_name,alias,2,true);
    LongEntry(root2.data(),long_name,alias,1,false);
    DirEntry(root2.data()+32,alias,0x20,11,333);
    LongEntry(root2.data()+64,"FILE.BIN.C","FILEBI~1C  ",1,true);
    DirEntry(root2.data()+96,"FILEBI~1C  ",0x20,14,1200);
    if (mode!="writable") {
      DirEntry(root2.data()+128,"LOOP    BIN",0x20,16,1500);
      DirEntry(root2.data()+160,"BAD     BIN",0x20,17,100);
      DirEntry(root2.data()+192,"SHORT   BIN",0x20,18,1500);
    }
    DirEntry(sub.data(),".          ",0x10,3,0);
    DirEntry(sub.data()+32,"..         ",0x10,0,0);
    DirEntry(sub.data()+64,"NEST    TXT",0x20,8,611);
    Write(out,ClusSector(2),root);Write(out,ClusSector(9),root2);
    Write(out,ClusSector(3),sub);
    Data(out,{5,12,7},1);Data(out,{8},2);Data(out,{11},3);Data(out,{14,15},4);
    if (mode!="writable") Data(out,{16,17,18},5);
    out.close();
    if (!out)
      throw std::runtime_error("Cannot close FAT32 fixture");
    return 0;
  } catch (const std::exception &error) {
    std::cerr<<error.what()<<'\n';
    return 1;
  }
}
