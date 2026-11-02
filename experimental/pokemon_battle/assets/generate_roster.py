#!/usr/bin/env python3
"""Generate code-only species bindings from verified private asset reports."""
import argparse
import json
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets',type=Path,required=True)
    a=p.parse_args();here=Path(__file__).parent
    roster=json.loads((here/'roster.json').read_text())['species']
    moves=json.loads((here/'moves.json').read_text())['moves']
    symbols={m['id']:m['symbol'] for m in moves}
    (here.parent/'field/move_enum.inc').write_text(',\n'.join(f"    {m['symbol']}={m['id']}" for m in moves)+'\n')
    (here.parent/'field/move_data.inc').write_text('\n'.join(
        '    {Move::%s,"%s",%s,%s,%s,%s,%s,%s,Visual::%s,Status::%s},'%
        (m['symbol'],m['name'],m['pp'],m['legacy_clip'],str(m['damaging']).lower(),m['type'],m['power'],m['accuracy'],m['visual'],m['status'])
        for m in moves)+'\n')
    rows=[]
    for s in roster:
        folder=f"{s['id']:03d}-{s['asset']}"
        source=json.loads((a.assets/'original'/folder/'model.json').read_text())
        clips=next(x for x in source['animations'] if x['name'].endswith('_Idle'))
        bounds=clips['sampled_poses'][0]
        scale=s['field_height']/(bounds['max'][2]-bounds['min'][2])
        offset=-bounds['min'][2]+(s['field_height']*.35/scale if s['locomotion']['hover'] else 0)
        gait=json.loads((a.assets/'animated'/folder/'locomotion.json').read_text())['authored_clips']
        pack=json.loads((a.assets/'runtime'/f"{s['id']:03d}.json").read_text())
        assert pack['max_blender_position_error']<.0002
        indices=[s['clips'][k] for k in ['faint','hurt','physical','special','gesture']]
        assert pack['clips'][indices[0]]['name'].endswith('_Faint')
        assert pack['clips'][indices[1]]['name'].endswith('_Damage')
        def seq(items):return '{'+','.join(map(str,items))+'}'
        def num(x):return f'{x:.8f}f'
        rows.append('    {%d,"%s","%s",%s,%s,%d,%d,%s,%s,%s,%s,%s,%s,%s},'%(
            s['id'],s['name'],s['asset'],seq(s['types']),seq(s['base_stats']),s['catch_rate'],s['base_exp'],
            seq('Move::'+symbols[mid] for mid in s['field_moves']),seq(indices),num(scale),num(offset),num(s['field_height']),
            num(gait[0]['stride_model_units']*scale),num(gait[1]['stride_model_units']*scale)))
    (here.parent/'field/roster_data.inc').write_text('// Species facts and verified animation bindings. No retail assets.\n'+'\n'.join(rows)+'\n')
    print('Generated',len(roster),'species and',len(moves),'moves')


if __name__=='__main__':main()
