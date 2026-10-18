from pathlib import Path
import sys,zipfile,xml.etree.ElementTree as ET
p=Path(sys.argv[1])
with zipfile.ZipFile(p/'edited.docx') as z:
    doc=ET.fromstring(z.read('word/document.xml'))
    text=''.join(doc.itertext())
    assert 'EXTENSION_INSERT_OK' in text
    assert any(name.startswith('word/media/') for name in z.namelist()), 'Inserted image missing'
    size=doc.find('.//{http://schemas.openxmlformats.org/wordprocessingml/2006/main}pgSz')
    ns='{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
    assert int(size.get(ns+'w'))>int(size.get(ns+'h')), 'Landscape orientation missing'
    assert 'keyboard_input_ok' in text, 'Keyboard events did not reach the document'
    assert 'RAMD_EDITOR_TYPED' in text and 'café' in text and 'español' in text
    assert doc.find('.//{http://schemas.openxmlformats.org/wordprocessingml/2006/main}tbl') is not None
assert (p/'edited.pdf').read_bytes().startswith(b'%PDF-')
print('PASS: rich-text content, accents, table and PDF signature')

with zipfile.ZipFile(p/'recovery.odt') as z:
    assert 'EXTENSION_INSERT_OK' in z.read('content.xml').decode()
print('PASS: extension content, embedded image, orientation and recovery snapshot')
