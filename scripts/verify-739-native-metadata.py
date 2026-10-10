"""Validate all four signed mod descriptors in the final executable, before shipping."""
import argparse
from pathlib import Path
import struct


def verify(path):
    data = path.read_bytes()
    def unpack(fmt, offset):
        return struct.unpack_from(fmt, data, offset)
    assert data[:2] == b'MZ', 'Missing DOS header'
    pe, = unpack('<I', 60)
    assert data[pe:pe+4] == b'PE\0\0', 'Missing PE header'
    machine, count = unpack('<HH', pe+4)
    assert machine == 0x8664, 'Final image is not x64'
    optional = pe+24
    assert unpack('<H', optional)[0] == 0x20b, 'Image is not PE32+'
    optional_size, = unpack('<H', pe+20)
    base, = unpack('<Q', optional+24)
    sections = []
    for index in range(count):
        offset = optional+optional_size+index*40
        name = data[offset:offset+8].split(b'\0', 1)[0].decode('ascii')
        size, rva, raw_size, raw = unpack('<IIII', offset+8)
        flags, = unpack('<I', offset+36)
        assert raw+raw_size <= len(data), 'Truncated '+name
        sections.append((name,rva,size,raw,raw_size,flags))
    def section_at(rva, size=1):
        return next(s for s in sections if s[1] <= rva and rva+size <= s[1]+s[2])
    def read(rva, size):
        name,start,_,raw,raw_size,_ = section_at(rva,size)
        assert rva-start+size <= raw_size, 'Unbacked metadata in '+name
        return data[raw+rva-start:raw+rva-start+size]
    def cstring(rva):
        s = section_at(rva)
        return read(rva, s[1]+s[2]-rva).split(b'\0',1)[0].decode('ascii')
    export_rva, _ = unpack('<II', optional+112)
    export = read(export_rva,40)
    functions_count,names_count,functions_rva,names_rva,ordinals_rva = struct.unpack_from('<IIIII',export,20)
    functions = struct.unpack('<'+'I'*functions_count,read(functions_rva,4*functions_count))
    names = struct.unpack('<'+'I'*names_count,read(names_rva,4*names_count))
    ordinals = struct.unpack('<'+'H'*names_count,read(ordinals_rva,2*names_count))
    exports = {cstring(name):functions[ordinal] for name,ordinal in zip(names,ordinals)}
    descriptors = [('','modmeta'),('cosmetics_','cosmeta'),('controller_ui_','cuimeta'),('luau_runtime_','luumeta')]
    ranges,contexts = [],[]
    for prefix,expected_section in descriptors:
        label = prefix+'mod_meta'
        size,begin,end = struct.unpack('<I4xQQ', read(exports[label],24))
        assert size >= 24 and begin != 0 and end > begin and begin%8 == 0, label+' invalid descriptor'
        begin -= base; end -= base
        section = section_at(begin,end-begin)
        assert section[0] == expected_section, label+' points outside its own metadata section'
        assert section[5]&0xc0000000 == 0xc0000000, label+' records are not readable/writable'
        assert all(end <= a or begin >= b for a,b in ranges), 'Overlapping mod metadata'
        ranges.append((begin,end))
        context = exports[prefix+'mod_ctx']
        assert context not in contexts and section_at(context,8)[5]&0x80000000, 'Shared or read-only SDK context'
        contexts.append(context)
        for fn in ['initialize','update','shutdown']:
            assert section_at(exports[prefix+'mod_'+fn])[5]&0x20000000, 'Non-executable '+prefix+fn
        records = read(begin,end-begin)
        cursor = 0; headers = []; record_count = 0
        while cursor < len(records):
            assert len(records)-cursor >= 8, label+' truncated record'
            if struct.unpack_from('<Q',records,cursor)[0] == 0:
                cursor += 8; continue
            length,kind,_ = struct.unpack_from('<HBB',records,cursor)
            assert length >= 8 and length%8 == 0 and length <= len(records)-cursor, label+' invalid record length'
            record = records[cursor:cursor+length]
            minimum = {1:8,2:80,3:80,4:24,5:32,6:16,7:24}.get(kind,8)
            assert length >= minimum, label+' truncated typed record'
            if kind == 1:
                headers.append(struct.unpack_from('<I',record,4)[0])
            elif kind in [2,3]:
                assert b'\0' in record[16:80], label+' unterminated service ID'
            elif kind == 6:
                assert b'\0' in record[16:], label+' unterminated hook name'
            elif kind in [5,7]:
                assert record[minimum:].count(b'\0') >= 2, label+' unterminated member hook names'
                if kind == 7:
                    materialize, = struct.unpack_from('<Q',record,8)
                    assert materialize and section_at(materialize-base)[5]&0x20000000, label+' invalid member-pointer materializer'
            cursor += length; record_count += 1
        assert headers == [1], label+' needs exactly one ABI v1 header'
        print('PASS .739 final PE',label+':',record_count,'records in',expected_section,'with independent writable context')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('pe',type=Path)
    args = parser.parse_args()
    verify(args.pe)
