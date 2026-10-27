"""Fixed OBS-01 live-binding acceptance; no product or Gate PASS claim."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

PHASES = ['Q-SEM-STAGED','Q-SEM-RESERVED','Q-SEM-PUBLISH',
          'Q-HIST-BEFORE-RESERVE','Q-HIST-READY','Q-CMD-ADMITTED',
          'Q-SEM-DONE','Q-HIST-DONE']
ORDER = ['Q-HIST-BEFORE-RESERVE','Q-HIST-READY','Q-CMD-ADMITTED',
         'Q-SEM-STAGED','Q-SEM-RESERVED','Q-SEM-PUBLISH','Q-SEM-DONE','Q-HIST-DONE']


def check_observation(observed, passive_off=None):
    errors=[]
    def require(condition, reason):
        if not condition: errors.append(reason)
    require(observed.get('binding') is True,'live binding unavailable')
    require(set(PHASES)<=set(observed.get('phases',[])),'reachable phases missing')
    rows=observed.get('scenarios',[])
    require(len(rows)==7,'six precommit schedules and passive completion required')
    for row in rows:
        fault=row['fault'];before=row['before'];after=row['after'];events=row['events']
        require(not row['trace_overflow'] and not row['observer_error'],f'{fault}: incomplete observation')
        require(row['postcommit_fault_refused'],f'{fault}: postcommit injection must be unavailable')
        require(not row['fault_remains_armed'],f'{fault}: armed fault leaked beyond command')
        require(before['history_size']==2 and before['history_cursor']==1 and before['can_redo'],f'{fault}: missing real redo tail')
        require([r['id'] for r in before['records']]==[f'{101:032x}'],f'{fault}: unexpected fixture prestate')
        names=[e['phase'] for e in events]
        expected_names=ORDER if fault is None else ORDER[:ORDER.index(fault)+1]
        require(names==expected_names,f'{fault}: incorrect reached-phase order {names}')
        operations={e['operation_id'] for e in events}
        require(len(operations)==1 and all(len(i)==32 and int(i,16)!=0 for i in operations),f'{fault}: full operation identity lost')
        for event in events:
            require(event['owner_id']=='71'+'00'*14+'2a' and event['owner_epoch']==1,f'{fault}: registration identity mismatch')
            require(event['document_id']==before['document_id'] and len(event['document_id'])==32,
                    f'{fault}: actual document identity mismatch')
            if event['phase'] not in ['Q-SEM-DONE','Q-HIST-DONE']:
                require(event['vector']==before,f"{fault}: mutation before commit at {event['phase']}")
            if event['generation'] is not None:
                require(event['generation']==event['vector']['generation'],f'{fault}: mixed semantic basis')
                require(event['commit_ordinal']==event['vector']['ordinal'],f'{fault}: mixed clock basis')
        if fault is not None:
            require(row['accepted'] is False,f'{fault}: failed precommit command reported accepted')
            require(before==after,f'{fault}: canonical/manifest/index/ledger/generation/clock/History changed')
            require([e['phase'] for e in events if e['rejection_selected']]==[fault],f'{fault}: rejection was not one-shot at the selected real stage')
        else:
            require(row['accepted'] is True,'passive command rejected')
            require(after['generation']==before['generation']+1 and after['ordinal']==before['ordinal']+1,'passive commit count mismatch')
            require(len(after['ledger'])==len(before['ledger'])+1,'passive ledger effect mismatch')
            require(after['history_size']==2 and after['history_cursor']==2 and not after['can_redo'],'passive History transition mismatch')
            require([r['id'] for r in after['records']]==[f'{101:032x}',f'{103:032x}'],'passive object identities mismatch')
            require(all(r['kind']==8 and r['width']==100 and r['height']==100 for r in after['records']),'literal Sticky dimensions/kind mismatch')
            require(after['manifest']==before['manifest'],'Sticky insert unexpectedly changed resource manifest')
            if len(events)==8:
                sem_done={**before,**{k:after[k] for k in ['records','indices','ledger','manifest','generation','epoch','ordinal']}}
                require(events[-2]['vector']==sem_done,'canonical-done boundary was not fully published before History')
                require(events[-1]['vector']==after,'History-done boundary is not the final logical transition')
            if passive_off is not None:
                def public_vector(v):return {k:value for k,value in v.items() if k not in ['history_entries','history_emitted']}
                require(row['accepted']==passive_off['accepted'],'OFF/ON acceptance differs')
                require(public_vector(before)==passive_off['before'] and public_vector(after)==passive_off['after'],
                        'passive OFF/ON canonical and public History vectors differ')
    isolation=observed.get('isolation',{})
    for key in ['B_accepted_while_A_armed','A_fault_preserved_while_B_ran','cancel_cleared','A_accepted_after_cancel',
                'close_cleared','detach_cleared','B_accepted_after_A_closed']:
        require(isolation.get(key) is True,'instance/lifetime isolation failed: '+key)
    for who,identity in [('A','71'),('B','72')]:
        for event in isolation.get(who+'_events',[]):
            require(event['owner_id']==identity+'00'*14+'2a' and not event['rejection_selected'],
                    'fault or registration leaked between real owners')
    for key in ['live_arm','destroyed_arm_refused','no_post_destroy_callback','observer_reference_released']:
        require(observed.get('lifetime',{}).get(key) is True,'destroyed registration lifetime failed: '+key)
    return errors


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--program',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--passive-off',type=Path)
    args = p.parse_args()
    r = subprocess.run([str(args.program.resolve())],capture_output=True,text=True,encoding='utf-8',errors='replace')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_suffix('.stdout.log').write_text(r.stdout,encoding='utf-8')
    args.output.with_suffix('.stderr.log').write_text(r.stderr,encoding='utf-8')
    if r.returncode:
        result = {'status':'ENVIRONMENT_OR_PROGRAM_FAILURE','exit_code':r.returncode}
    else:
        observed = json.loads(r.stdout)
        missing = sorted(set(PHASES)-set(observed['phases']))
        passive_off=json.loads(args.passive_off.read_text(encoding='utf-8'))['passive'] if args.passive_off else None
        errors=check_observation(observed,passive_off) if observed.get('binding') else ['live binding unavailable']
        result = {'status':'LIVE_HOOK_BINDING_ACCEPTED' if not errors else 'EXPECTED_BINDING_MISSING' if not observed.get('binding') else 'HOOK_OBSERVATION_FAILURE',
                  'missing_phases':missing,'errors':errors,'passive_OFF_compared':passive_off is not None,
                  'observed':observed,'product_PASS':False}
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(result['status'])
    return 0 if result['status']=='LIVE_HOOK_BINDING_ACCEPTED' else 1


if __name__ == '__main__': sys.exit(main())
