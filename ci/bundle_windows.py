from pathlib import Path
import os,re,subprocess,shutil
prefix=Path(subprocess.check_output(['cygpath','-m','/ucrt64'],text=True).strip())
dest=Path('windows-dist');dest.mkdir(exist_ok=True)
for name in ['ramd-docs.exe','ui-check.exe']:shutil.copy('build-windows/'+name,dest/name)
shutil.copy(prefix/'bin/gdk-pixbuf-query-loaders.exe',dest)
for folder in ['lib/gdk-pixbuf-2.0','share/glib-2.0/schemas','share/icons/Adwaita','share/icons/hicolor','share/mime','share/licenses']:
 src=prefix/folder
 if src.exists():shutil.copytree(src,dest/folder,dirs_exist_ok=True)
# Resolve transitive imports, including dynamically loaded image loaders.
available={p.name.lower():p for p in (prefix/'bin').glob('*.dll')}
queue=list(dest.rglob('*.dll'))+list(dest.glob('*.exe'));seen=set()
while queue:
 p=queue.pop()
 if p.name.lower() in seen:continue
 seen.add(p.name.lower())
 out=subprocess.check_output(['objdump','-p',str(p)],text=True,errors='replace')
 for name in re.findall(r'DLL Name:\s*(\S+)',out):
  src=available.get(name.lower())
  if src and not (dest/src.name).exists():shutil.copy(src,dest/src.name);queue.append(dest/src.name)
shutil.copytree('windows-lok',dest/'third-party/libreofficekit-widget',dirs_exist_ok=True)
(dest/'third-party/MSYS2-PACKAGES.txt').write_text(subprocess.check_output(['pacman','-Q'],text=True)+'\nPackage recipes and corresponding sources: https://github.com/msys2/MINGW-packages and https://repo.msys2.org/mingw/sources/\nRuntime libraries are dynamically linked and may be replaced with compatible builds.\n')
shutil.copy('README.md',dest/'README.txt')
print('Packaged',len(list(dest.rglob('*'))),'files')
