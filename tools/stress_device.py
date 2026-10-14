import json,time,hashlib,http.client,socket,concurrent.futures
from pathlib import Path
import requests
import argparse
parser=argparse.ArgumentParser(description='Profile ESP-TTS and test bounded photo/network contention; requires diagnostics firmware and USB host.')
parser.add_argument('--url',required=True)
parser.add_argument('--output',required=True)
args=parser.parse_args()
BASE=args.url.rstrip('/')
OUT=Path(args.output);OUT.mkdir(parents=True,exist_ok=True)
report={'events':[]};token='';temp='stress-tts-'+str(int(time.time()));created=False;original_time=None
report['temporary_album']=temp
def request(method,path,body=None,data=None,timeout=12):
    session=requests.Session();session.trust_env=False
    headers={'X-Album-Token':token}
    if body is not None:headers['Content-Type']='application/json';data=json.dumps(body).encode()
    elif data is not None:headers['Content-Type']='image/bmp'
    try:return session.request(method,BASE+path,headers=headers,data=data,timeout=timeout)
    finally:session.close()
def state():
    r=request('GET','/api/status');r.raise_for_status();return r.json()
def event(name,**values):
    item={'name':name,**values};report['events'].append(item);print(json.dumps(item,ensure_ascii=False),flush=True)
    (OUT/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def wait_idle(timeout=100):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        s=state()
        if not s['control']['busy'] and not s['announcement']['busy'] and not s['control'].get('index_pending',False):
            # Status snapshots may predate index_pending; verify live SD admission.
            if request('GET','/api/albums').status_code==200:return s
        time.sleep(1)
    raise RuntimeError('Idle timeout')
def command(path,body,expected='succeeded',timeout=100):
    deadline=time.monotonic()+timeout
    while True:
        r=request('POST',path,body)
        if r.status_code!=409 or time.monotonic()>=deadline:break
        time.sleep(1) # Explicit rejection means the command was not accepted.
    r.raise_for_status();job=r.json()['job_id']
    while time.monotonic()<deadline:
        s=state();c=s['control']
        if c['job_id']==job and c['state'] in ('succeeded','failed'):
            assert c['state']==expected,(path,c['state'],c.get('error'));return s
        time.sleep(1)
    raise RuntimeError('Command timeout '+path)
def preview(kind='hour'):
    before=state()['announcement']['completed_test_id']
    command('/api/announcement-test',{'kind':kind,'volume_percent':40})
    return before
def wait_preview(before):
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        s=state();a=s['announcement']
        if a['completed_test_id']>before:
            assert a['last_test_result']=='succeeded',a.get('last_test_error');return s
        time.sleep(1)
    raise RuntimeError('Preview timeout')
def diag():
    r=request('GET','/api/diagnostics');r.raise_for_status();return r.json()
def photo_url(album,name):return '/api/photo?album='+requests.utils.quote(album,safe='')+'&name='+requests.utils.quote(name,safe='')
try:
    before=state();token=before['control']['token'];report['version']=before['device']['version']
    report['baseline_albums']=before['album']['albums'];report['baseline_photos']=before['album']['photos'];report['periods']=before['announcement']['pause_periods'];report['dedup']=before['announcement']['last_processed_epoch']
    assert before['announcement']['engine']=='esp-tts' and diag()['available']
    assert before['network']['usb_host'],'Network fault testing requires USB host'
    original_time={'schema_version':1,'servers':before['network']['ntp_servers'],'interval_seconds':before['network']['sync_interval_seconds'],'retry_seconds':before['network']['retry_seconds']}
    wait_idle();request('POST','/api/diagnostics/reset',{}).raise_for_status();time.sleep(5)
    event('profiling_started')
    old=preview();s=wait_preview(old);event('baseline_preview',result=s['announcement']['last_test_result'],synthesis_ms=s['announcement']['test_synthesis_ms'])
    profile=diag();(OUT/'baseline-profile.json').write_text(json.dumps(profile,indent=2))
    current=state()['album'];parts=current['current_photo'].rsplit('/',1);source=photo_url(current['current_album'],parts[1])
    r=request('GET',source,timeout=60);r.raise_for_status();bmp=r.content;digest=hashlib.sha256(bmp).hexdigest();(OUT/'source.bmp').write_bytes(bmp)
    command('/api/album-create',{'schema_version':1,'id':temp,'enabled':False,'interval_seconds':900});created=True
    for index in range(3):
        url=photo_url(temp,f'test-{index}.bmp');r=request('PUT',url,data=bmp,timeout=90);assert r.status_code==201,r.status_code
        wait_idle();r=request('GET',url,timeout=60);r.raise_for_status();assert hashlib.sha256(r.content).hexdigest()==digest
    event('transfers_verified',uploads=3,downloads=4,bytes_per_photo=len(bmp),sha256_match=True)
    # Playback admits one queued next request, rejects file traffic and duplicates.
    old=preview('half');deadline=time.monotonic()+8
    while not state()['announcement']['busy'] and time.monotonic()<deadline:time.sleep(.2)
    original=state()['album']['current_photo']
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        get=pool.submit(request,'GET',source)
        nxt=pool.submit(request,'POST','/api/next',{})
        get_code=get.result().status_code;nxt_r=nxt.result()
    assert get_code==409 and nxt_r.status_code==202,(get_code,nxt_r.status_code)
    duplicate=request('POST','/api/next',{});assert duplicate.status_code==409
    s=state();assert s['album']['current_photo']==original
    wait_preview(old);s=wait_idle(100);assert s['album']['current_photo']!=original
    event('playback_contention',file_status=get_code,next_status=202,duplicate_status=409,deferred_photo=True)
    # NTP failure is scoped to a temporary unreachable documentation address.
    command('/api/time-settings',{**original_time,'servers':['192.0.2.1']})
    r=request('POST','/api/time-sync',{});r.raise_for_status();job=r.json()['job_id'];time.sleep(1)
    attempts=[]
    for path,body in [('/api/announcement-test',{'kind':'hour','volume_percent':40}),('/api/next',{})]:attempts.append(request('POST',path,body).status_code)
    deadline=time.monotonic()+90
    while time.monotonic()<deadline:
        s=state()
        if s['control']['job_id']==job and s['control']['state']=='failed':break
        time.sleep(1)
    assert s['control']['state']=='failed' and s['rtc']['valid']
    event('ntp_timeout',result=s['network']['sync_result'],concurrent_statuses=attempts,rtc_valid=True)
    command('/api/time-settings',original_time);command('/api/time-sync',{})
    event('ntp_restored',result=state()['network']['sync_result'])
    # Provoke an incomplete upload while Wi-Fi is off, then verify atomic cleanup.
    request('POST','/api/diagnostics/network-fault',{}).raise_for_status()
    from urllib.parse import urlsplit
    target=urlsplit(BASE);connection=http.client.HTTPConnection(target.hostname,target.port or 80,timeout=4)
    name='interrupted.bmp';url=photo_url(temp,name)
    connection.putrequest('PUT',url);connection.putheader('X-Album-Token',token);connection.putheader('Content-Type','image/bmp');connection.putheader('Content-Length',len(bmp));connection.endheaders()
    try:
        for offset in range(0,65536,4096):connection.send(bmp[offset:offset+4096]);time.sleep(.15)
        time.sleep(2)
    except (OSError,socket.timeout):pass
    finally:connection.close()
    deadline=time.monotonic()+75;offline=False;recovered=False
    while time.monotonic()<deadline:
        try:
            s=state()
            if s['network']['wifi_connected'] and not diag()['network_fault_active']:recovered=True;break
        except requests.RequestException:offline=True
        time.sleep(1)
    assert recovered,'Wi-Fi did not recover'
    wait_idle(75);r=request('GET','/api/files?album='+temp);r.raise_for_status();files=r.json()['files'];assert all(f['name']!=name for f in files)
    event('wifi_fault_recovered',offline_observed=offline,incomplete_upload_not_published=True,web_restored=True)
    old=preview();s=wait_preview(old);event('post_fault_preview',result=s['announcement']['last_test_result'])
    # Disconnect during synthesis/playback, then read completion after recovery.
    old=preview();request('POST','/api/diagnostics/network-fault',{}).raise_for_status()
    deadline=time.monotonic()+75;offline=False;finished=False
    while time.monotonic()<deadline:
        time.sleep(2)
        try:
            current=state()
            if current['announcement']['completed_test_id']>old and not diag()['network_fault_active']:
                assert current['announcement']['last_test_result']=='succeeded';finished=True;break
        except requests.RequestException:offline=True
    assert finished,'Offline playback did not finish or Wi-Fi did not recover'
    event('offline_playback',offline_observed=offline,result='succeeded')
    report['profile']=diag();(OUT/'combined-profile.json').write_text(json.dumps(report['profile'],indent=2))
finally:
    if token:
        try:
            wait_idle(100)
            if original_time:command('/api/time-settings',original_time)
            if created:
                r=request('GET','/api/files?album='+temp);r.raise_for_status()
                for f in r.json()['files']:
                    r=request('DELETE',photo_url(temp,f['name']));r.raise_for_status();wait_idle()
                command('/api/album-delete',{'schema_version':1,'id':temp})
            after=state();report['final']={'albums':after['album']['albums'],'photos':after['album']['photos'],'rtc_valid':after['rtc']['valid'],'ntp_servers_restored':not original_time or after['network']['ntp_servers']==original_time['servers'],'wifi_connected':after['network']['wifi_connected'],'periods_preserved':after['announcement']['pause_periods']==report['periods'],'dedup_preserved':after['announcement']['last_processed_epoch']==report['dedup']}
            assert report['final']['photos']==report['baseline_photos'] and report['final']['albums']==report['baseline_albums']
            event('cleanup_complete',**report['final'])
        except Exception as error:event('cleanup_failed',error=str(error));raise
    (OUT/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
