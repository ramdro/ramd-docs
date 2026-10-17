from pathlib import Path
import sys, zipfile, xml.etree.ElementTree as ET
folder = Path(sys.argv[1])
with zipfile.ZipFile(folder / 'roundtrip.docx') as archive:
    document = ET.fromstring(archive.read('word/document.xml'))
    assert 'RAMD_ROUNDTRIP_2026' in ''.join(document.itertext())
assert (folder / 'roundtrip.pdf').read_bytes().startswith(b'%PDF-')
print('PASS: Word document content and PDF signature')
