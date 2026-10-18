from pathlib import Path
import sys,zipfile,xml.etree.ElementTree as ET
p=Path(sys.argv[1])
with zipfile.ZipFile(p/'edited.docx') as z:
    doc=ET.fromstring(z.read('word/document.xml'))
    text=''.join(doc.itertext())
    assert 'RAMD_EDITOR_TYPED' in text and 'café' in text and 'español' in text
    assert doc.find('.//{http://schemas.openxmlformats.org/wordprocessingml/2006/main}tbl') is not None
assert (p/'edited.pdf').read_bytes().startswith(b'%PDF-')
print('PASS: rich-text content, accents, table and PDF signature')
