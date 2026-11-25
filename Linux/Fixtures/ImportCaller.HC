import I64 NativeLinkAdd(I64 value=7);
import I64 NativeLinkBias;
_import _MEMCPY U8 *NativeLinkCopy(U8 *dst,U8 *src,I64 size);
_import SYS_COMPILE_TIME I64 NativeLinkDate;

public I64 NativeLinkResult,NativeLinkDateResult;
NativeLinkResult=NativeLinkAdd+NativeLinkBias;
NativeLinkCopy(&NativeLinkDateResult,&NativeLinkDate,8);
"native link %d\n",NativeLinkResult;
