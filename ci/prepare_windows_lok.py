"""Build the official GTK widget standalone; preserve upstream MPL sources."""
from pathlib import Path
import re,urllib.request
base='https://raw.githubusercontent.com/LibreOffice/core/libreoffice-24.2.7.2/'
out=Path('windows-lok');out.mkdir(exist_ok=True)
def get(remote,local):
 p=out/local;p.parent.mkdir(parents=True,exist_ok=True)
 data=urllib.request.urlopen(base+remote,timeout=60).read();p.write_bytes(data);return data.decode()
for name in ['LibreOfficeKit.h','LibreOfficeKit.hxx','LibreOfficeKitInit.h','LibreOfficeKitEnums.h','LibreOfficeKitTypes.h','LibreOfficeKitGtk.h']:
 get('include/LibreOfficeKit/'+name,'include/LibreOfficeKit/'+name)
for name in ['lokdocview.cxx','tilebuffer.cxx','tilebuffer.hxx']:
 get('libreofficekit/source/gtk/'+name,name)
keys=get('offapi/com/sun/star/awt/Key.idl','upstream/Key.idl')
get('include/vcl/keycodes.hxx','upstream/keycodes.hxx');get('include/vcl/event.hxx','upstream/event.hxx')
get('COPYING.MPL','COPYING.MPL');get('NOTICE','NOTICE')
# The widget uses only fixed-width scalar types and protocol key/mouse constants
# from these internal headers. No LibreOffice C++ ABI is linked.
compat='''#pragma once
#include <cstdint>
using sal_uInt8=uint8_t;
using sal_uInt16=uint16_t;
using sal_uInt32=uint32_t;
#define SAL_DLLPUBLIC_EXPORT
constexpr int MOUSE_LEFT=1,MOUSE_MIDDLE=2,MOUSE_RIGHT=4;
constexpr int KEY_SHIFT=0x1000,KEY_MOD1=0x2000,KEY_MOD2=0x4000;
namespace css { namespace awt { namespace Key {
'''
compat+='\n'.join('constexpr int '+name+'='+value+';' for name,value in re.findall(r'const short (\w+)\s*=\s*(\d+);',keys))
compat+='\n}}}\n';(out/'ramd-lok-compat.h').write_text(compat)
p=out/'lokdocview.cxx';s=p.read_text().replace('#include <sal/types.h>','#include "ramd-lok-compat.h"').replace('#include <com/sun/star/awt/Key.hpp>','').replace('#include <vcl/event.hxx>','');p.write_text(s)
p=out/'tilebuffer.cxx';s=p.read_text().replace('#include <o3tl/unit_conversion.hxx>','').replace('o3tl::toTwips(fInput / zoom, o3tl::Length::px)','(fInput / zoom) * 15.0f').replace('o3tl::convert(fInput * zoom, o3tl::Length::twip, o3tl::Length::px)','(fInput * zoom) / 15.0f');p.write_text(s)
(out/'RAMD-MODIFICATIONS.txt').write_text('LibreOffice GTK widget from libreoffice-24.2.7.2, MPL 2.0.\nRAMD standalone changes: protocol constants replace internal headers; 96-DPI twip conversion is inlined. All modified sources are included.\nUpstream: '+base+'\n')
