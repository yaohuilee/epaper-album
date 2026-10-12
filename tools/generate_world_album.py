"""Generate a resumable Qwen Image 2.1 collection through local ComfyUI."""
import argparse
import hashlib
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path
from prepare_photos import prepare

SCENES = '''asia|guilin|中国桂林|Karst peaks reflected in the Li River, bamboo groves, morning mist and warm sunlight, Guilin China
asia|huangshan|中国黄山|Granite mountain spires, ancient twisted pine trees above a sea of white clouds, Huangshan China
asia|jiuzhaigou|中国九寨沟|Turquoise transparent lake with submerged tree trunks, autumn forests and snowy mountains, Jiuzhaigou China
asia|zhangjiajie|中国张家界|Towering sandstone pillars rising above lush forest and thin mist, Zhangjiajie China
asia|yuanyang|中国元阳|Terraced rice fields reflecting golden sunrise with emerald mountain ridges, Yuanyang China
asia|wuyuan|中国婺源|Golden rapeseed fields, white traditional village houses, winding river and spring hills, Wuyuan China
asia|xinjiang|中国喀纳斯|Blue alpine river winding through golden larch forests, snow peaks, Kanas Xinjiang China
asia|mount_fuji|日本富士山|Snow capped Mount Fuji reflected in Lake Kawaguchi, red maple leaves framing clear blue water, Japan
asia|kyoto_arashiyama|日本京都|Tall green bamboo forest beside a quiet mossy path lit by soft morning sun, Arashiyama Kyoto Japan
asia|shirakawa|日本白川乡|Traditional steep thatched farmhouses covered in snow beneath blue mountain skies, Shirakawa-go Japan
asia|jeju|韩国济州岛|Dark volcanic basalt coast, emerald grass, white surf and deep blue ocean, Jeju Island Korea
asia|halong|越南下龙湾|Green limestone islands emerging from turquoise water, one distant red sail, Ha Long Bay Vietnam
asia|bali|印度尼西亚巴厘岛|Lush rice terraces with palm trees and warm tropical morning light, Bali Indonesia
asia|komodo|印度尼西亚科莫多|Pink sand beach, curved turquoise bay and dry green island hills, Komodo Indonesia
asia|palawan|菲律宾巴拉望|Crystal clear lagoon enclosed by jagged limestone cliffs and tropical forest, Palawan Philippines
asia|bhutan|不丹帕罗|White and red Tiger's Nest monastery perched on a high green Himalayan cliff, Bhutan
asia|nepal|尼泊尔安纳普尔纳|Snowy Annapurna mountain range above flowering rhododendron hills under crisp morning light, Nepal
asia|kashmir|印度克什米尔|Calm Dal Lake with a colorful wooden boat, distant mountains and lotus leaves, Kashmir India
asia|cappadocia|土耳其卡帕多奇亚|Golden fairy chimney rock formations and a few distant hot air balloons under bright dawn sky, Cappadocia Turkey
asia|wadi_rum|约旦瓦迪拉姆|Huge red sandstone mountains and golden desert dunes under vivid blue sky, Wadi Rum Jordan
europe|dolomites|意大利多洛米蒂|Jagged pale Dolomite peaks above emerald meadows and an alpine lake, clear summer light, Italy
europe|tuscany|意大利托斯卡纳|Rolling green and golden hills, a winding cypress lined road and stone farmhouse, Tuscany Italy
europe|amalfi|意大利阿马尔菲|Pastel coastal village on steep green cliffs above the azure Mediterranean, Amalfi Italy
europe|swiss_alps|瑞士阿尔卑斯|Snow capped Matterhorn reflected in a small alpine lake with yellow wildflowers, Switzerland
europe|lauterbrunnen|瑞士劳特布龙嫩|Green alpine valley, white waterfall falling from a tall cliff and tiny chalets, Lauterbrunnen Switzerland
europe|hallstatt|奥地利哈尔施塔特|Austrian lakeside village with church spire reflected in still blue water beneath mountains, Hallstatt
europe|provence|法国普罗旺斯|Neat purple lavender rows with a rustic stone farmhouse and golden sunlight, Provence France
europe|mont_saint_michel|法国圣米歇尔山|Mont Saint Michel abbey on a rocky tidal island reflected in shallow water under blue skies, France
europe|loire|法国卢瓦尔河谷|Elegant French castle beside a river with green gardens and flowering trees, Loire Valley France
europe|norway_fjord|挪威峡湾|Deep blue fjord between steep green cliffs with cascading waterfalls, Geiranger Norway
europe|lofoten|挪威罗弗敦|Red fishermen cabins beside turquoise Arctic water with rugged mountain peaks, Lofoten Norway
europe|iceland_waterfall|冰岛斯科加瀑布|Wide powerful waterfall over green cliffs with a clear rainbow and blue sky, Skogafoss Iceland
europe|iceland_black_beach|冰岛黑沙滩|Black volcanic sand, basalt columns, foaming white waves and offshore sea stacks, Reynisfjara Iceland
europe|scottish_highlands|苏格兰高地|Green and golden Highland hills surrounding a deep blue loch with a small stone castle, Scotland
europe|lake_district|英国湖区|Rolling green hills, dry stone walls and a tranquil reflective lake under scattered white clouds, England Lake District
europe|ireland_cliffs|爱尔兰莫赫悬崖|Green grassy cliffs dropping into a blue Atlantic with white sea spray, Cliffs of Moher Ireland
europe|santorini|希腊圣托里尼|White houses with blue domes above deep azure sea in clear afternoon sun, Santorini Greece
europe|plitvice|克罗地亚普利特维采|Turquoise terraced lakes linked by white waterfalls and lush green forest, Plitvice Croatia
europe|bled|斯洛文尼亚布莱德|Small church island in emerald Lake Bled with mountains and autumn trees, Slovenia
europe|madeira|葡萄牙马德拉|Dramatic green volcanic coastal cliffs, terraced gardens and luminous blue Atlantic, Madeira Portugal
africa|sahara|摩洛哥撒哈拉|Flowing orange Sahara dunes with long elegant shadows beneath a pure blue sky, Erg Chebbi Morocco
africa|atlas|摩洛哥阿特拉斯山|Snow capped Atlas Mountains above red earth villages and green terraced valleys, Morocco
africa|chefchaouen|摩洛哥舍夫沙万|Blue painted mountain town with stone steps, flower pots and bright sunlight, Chefchaouen Morocco
africa|nile|埃及尼罗河|White felucca sails on the blue Nile beside palm trees and golden desert hills, Aswan Egypt
africa|white_desert|埃及白沙漠|Sculptural white chalk rock formations on golden sand beneath an expansive blue sky, White Desert Egypt
africa|simien|埃塞俄比亚塞米恩|Dramatic Ethiopian highland escarpments, green plateaus and misty deep valleys, Simien Mountains
africa|serengeti|坦桑尼亚塞伦盖蒂|Golden savanna grassland with an acacia tree and distant zebras in warm morning light, Serengeti Tanzania
africa|kilimanjaro|坦桑尼亚乞力马扎罗|Snow capped Kilimanjaro above emerald plains and scattered acacia trees, Tanzania
africa|ngorongoro|坦桑尼亚恩戈罗恩戈罗|Vast green volcanic crater with blue lake and sunlit grasslands, Ngorongoro Tanzania
africa|zanzibar|坦桑尼亚桑给巴尔|White sand beach, turquoise Indian Ocean, green palms and a distant traditional dhow, Zanzibar
africa|masai_mara|肯尼亚马赛马拉|Wide green and golden savanna with distant giraffes and red earth trail beneath white clouds, Masai Mara Kenya
africa|lake_nakuru|肯尼亚纳库鲁湖|A shallow blue lake with a band of flamingos, green hills and clean daylight, Lake Nakuru Kenya
africa|victoria_falls|赞比亚维多利亚瀑布|Immense white waterfall falling into a deep green gorge with sunlight and rainbow, Victoria Falls Zambia
africa|okavango|博茨瓦纳奥卡万戈|Blue waterways winding through emerald wetland islands and golden reeds, Okavango Delta Botswana
africa|sossusvlei|纳米比亚苏丝斯黎|Tall red sand dunes surrounding a white salt pan and dark ancient trees, Sossusvlei Namibia
africa|skeleton_coast|纳米比亚骷髅海岸|Golden desert dunes meeting cold blue Atlantic waves, dramatic empty Skeleton Coast Namibia
africa|cape_town|南非开普敦|Table Mountain above a vivid blue coastal bay with flowering foreground and sunny skies, Cape Town South Africa
africa|drakensberg|南非德拉肯斯堡|Green rolling mountain foothills, tall basalt cliffs and golden wildflowers, Drakensberg South Africa
africa|baobabs|马达加斯加猴面包树|Avenue of immense baobab trees beside a red dirt road and green fields in warm sun, Madagascar
africa|seychelles|塞舌尔海滩|Rounded granite boulders, white sand and turquoise sea with green palms, La Digue Seychelles
americas|banff|加拿大班夫|Turquoise Moraine Lake with snowy Rocky Mountain peaks and emerald pine trees, Banff Canada
americas|jasper|加拿大贾斯珀|Spirit Island surrounded by bright blue Maligne Lake and layered mountain peaks, Jasper Canada
americas|niagara|加拿大尼亚加拉|Wide curved Niagara Falls with white spray and a rainbow in clear daylight, Canada
americas|alaska|美国阿拉斯加|Blue glacier flowing between rugged snowy mountains above a cold turquoise fjord, Alaska USA
americas|yosemite|美国优胜美地|Granite valley cliffs with Bridalveil Fall, green pine forest and clear blue sky, Yosemite USA
americas|grand_canyon|美国大峡谷|Layered red and orange canyon walls with a winding blue Colorado River, Grand Canyon USA
americas|yellowstone|美国黄石|Grand Prismatic Spring with bright blue center, yellow orange rim and green forest, Yellowstone USA
americas|antelope_canyon|美国羚羊谷|Smooth flowing red sandstone canyon walls illuminated by a golden shaft of daylight, Antelope Canyon USA
americas|big_sur|美国大苏尔|Rugged green California coastal cliffs and a graceful distant bridge above azure Pacific waves, Big Sur USA
americas|hawaii|美国夏威夷|Green Na Pali coast cliffs rising from turquoise ocean with white waves, Kauai Hawaii
americas|monument_valley|美国纪念碑谷|Tall red sandstone buttes on a golden desert plain under white clouds and blue sky, Monument Valley USA
americas|mexico_cenote|墨西哥天然井|Open limestone cenote with deep turquoise water, dangling vines and sunbeams, Yucatan Mexico
americas|costa_rica|哥斯达黎加雨林|Lush tropical rainforest waterfall flowing into an emerald pool with red flowers, Costa Rica
americas|cuba|古巴比尼亚莱斯|Green tobacco valley with limestone hills and small red roofed farms in clear sunlight, Vinales Cuba
americas|machu_picchu|秘鲁马丘比丘|Ancient stone terraces of Machu Picchu above a lush green Andean mountain valley, Peru
americas|rainbow_mountain|秘鲁彩虹山|Naturally striped red yellow and turquoise Andean mountain ridge beneath a clear blue sky, Vinicunca Peru
americas|uyuni|玻利维亚乌尤尼|Mirror like shallow salt flat reflecting a vast blue sky and white clouds with a tiny island, Salar de Uyuni Bolivia
americas|patagonia|智利巴塔哥尼亚|Torres del Paine granite towers, turquoise lake and golden grassy foreground, Patagonia Chile
americas|iguazu|巴西伊瓜苏瀑布|Numerous broad waterfalls across green rainforest cliffs with bright white spray, Iguazu Brazil
americas|los_glaciares|阿根廷冰川国家公园|Jagged blue Perito Moreno glacier front beside turquoise lake with snowy mountains, Argentina
oceania-polar|sydney_coast|澳大利亚悉尼海岸|Golden Australian beach and turquoise waves with a distant white sail shaped opera house, Sydney Australia
oceania-polar|great_barrier_reef|澳大利亚大堡礁|Turquoise shallow reef lagoons with coral patterns and a small white sand island, Great Barrier Reef Australia
oceania-polar|uluru|澳大利亚乌鲁鲁|Immense glowing red Uluru rock above golden desert grasses beneath vivid blue skies, Australia
oceania-polar|twelve_apostles|澳大利亚十二门徒|Tall golden limestone sea stacks above blue surf beside green coastal cliffs, Twelve Apostles Australia
oceania-polar|tasmania|澳大利亚塔斯马尼亚|Pink granite mountains surrounding a crescent white beach with turquoise water, Wineglass Bay Tasmania
oceania-polar|blue_mountains|澳大利亚蓝山|Layered blue forested valleys and sandstone Three Sisters cliffs under white clouds, Blue Mountains Australia
oceania-polar|milford_sound|新西兰米尔福德峡湾|Mitre Peak reflected in calm blue fjord with steep green cliffs and white waterfalls, Milford Sound New Zealand
oceania-polar|lake_tekapo|新西兰特卡波湖|Luminous turquoise lake with purple lupin flowers and snowy distant mountains, Lake Tekapo New Zealand
oceania-polar|mount_cook|新西兰库克山|Snow capped Mount Cook above a rocky alpine valley and blue glacial river, New Zealand
oceania-polar|rotorua|新西兰罗托鲁瓦|Colorful geothermal pool with orange mineral banks, emerald vegetation and gentle white steam, Rotorua New Zealand
oceania-polar|wanaka|新西兰瓦纳卡湖|A lone willow tree in a clear blue lake backed by golden hills and snowy mountain peaks, Lake Wanaka New Zealand
oceania-polar|fiji|斐济群岛|Small lush tropical island with white beach and clear azure lagoon beneath sunny white clouds, Fiji
oceania-polar|bora_bora|法属波利尼西亚波拉波拉|Emerald volcanic peak rising above a turquoise tropical lagoon and tiny palm islands, Bora Bora
oceania-polar|moorea|法属波利尼西亚莫雷阿|Jagged emerald mountain island, luminous turquoise lagoon and flowering tropical shoreline, Moorea
oceania-polar|samoa|萨摩亚海沟|Deep emerald swimming hole enclosed by lush tropical vegetation with a wooden ladder, To Sua Samoa
oceania-polar|new_caledonia|新喀里多尼亚|Heart shaped mangrove island surrounded by clear turquoise lagoon and blue ocean, New Caledonia
oceania-polar|greenland|格陵兰岛|Sculptural white and blue icebergs floating in a calm deep blue Arctic fjord under clear daylight, Greenland
oceania-polar|svalbard|挪威斯瓦尔巴|Snowy Arctic mountains reflected in blue sea beside colorful small houses, Svalbard
oceania-polar|antarctica|南极半岛|Tall blue icebergs and white Antarctic mountains with a few distant penguins on a rocky shore in bright daylight
oceania-polar|south_georgia|南乔治亚岛|Snowy subantarctic mountains rising above a green coastal plain with a distant small king penguin colony, South Georgia'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8188')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=100)
    args = parser.parse_args()
    base = args.url.rstrip('/')
    out = args.output
    for sub in ('originals', 'bmp', 'previews', 'workflows'):
        (out/sub).mkdir(parents=True, exist_ok=True)
    subjects = []
    for index, line in enumerate(SCENES.splitlines(), 1):
        region, slug, title, scene = line.split('|')
        subjects.append(dict(index=index, region=region, name=f'{index:03d}_{slug}', title=title, scene=scene))
    assert len(subjects) == 100 and len({s['name'] for s in subjects}) == 100
    (out/'subjects.json').write_text(json.dumps(subjects, ensure_ascii=False, indent=2), encoding='utf-8')

    def api(path, payload=None):
        data = json.dumps(payload).encode() if payload is not None else None
        req = urllib.request.Request(base+path, data=data, headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=60) as response:
            return json.load(response)

    for item in subjects[:args.limit]:
        name = item['name']
        original = out/'originals'/f'{name}.png'
        bmp = out/'bmp'/f'{name}.bmp'
        job_path = out/'workflows'/f'{name}.json'
        if not original.exists():
            if job_path.exists():
                job = json.loads(job_path.read_text(encoding='utf-8'))
            else:
                prompt = ('Beautiful fine art travel landscape photograph of '+item['scene']+'. '
                          'Wide horizontal composition, strong readable foreground and distant scenery, '
                          'bright natural daylight, clear detail, luminous clean colors, blue green white '
                          'with golden and red accents, elegant balanced framing, no text, no watermark, no border.')
                graph = {
                    '1': {'class_type': 'UNETLoader', 'inputs': {'unet_name': 'Qwen-Image-2.1\\qwen-image-2.1-UC-fp8.safetensors', 'weight_dtype': 'default'}},
                    '2': {'class_type': 'CLIPLoader', 'inputs': {'clip_name': 'qwen3vl_8b_bf16.safetensors', 'type': 'qwen_image', 'device': 'default'}},
                    '3': {'class_type': 'VAELoader', 'inputs': {'vae_name': 'qwen_image_2.1_vae_bf16.safetensors'}},
                    '4': {'class_type': 'TextEncodeQwenImage21', 'inputs': {'clip': ['2', 0], 'prompt': prompt, 'negative_prompt': '', 'resolution': 1024}},
                    '5': {'class_type': 'QwenImage21Cache', 'inputs': {'model': ['1', 0], 'device': 'cpu', 'dtype': 'default'}},
                    '6': {'class_type': 'KSampler', 'inputs': {'model': ['5', 0], 'positive': ['4', 0], 'negative': ['4', 1], 'latent_image': ['9', 0], 'seed': 202610110000+item['index'], 'steps': 25, 'cfg': 1.0, 'sampler_name': 'euler', 'scheduler': 'simple', 'denoise': 1.0}},
                    '7': {'class_type': 'VAEDecode', 'inputs': {'samples': ['6', 0], 'vae': ['3', 0]}},
                    '8': {'class_type': 'SaveImage', 'inputs': {'images': ['7', 0], 'filename_prefix': 'epaper_world_20261011/'+name}},
                    '9': {'class_type': 'EmptySD3LatentImage', 'inputs': {'width': 1280, 'height': 768, 'batch_size': 1}},
                }
                reply = api('/prompt', {'prompt': graph, 'client_id': 'epaper-world-album'})
                job = dict(item, prompt=prompt, prompt_id=reply['prompt_id'], workflow=graph)
                job_path.write_text(json.dumps(job, ensure_ascii=False, indent=2), encoding='utf-8')
                print('QUEUED', name, reply['prompt_id'], flush=True)
            deadline = time.monotonic()+1800
            while True:
                history = api('/history/'+job['prompt_id']).get(job['prompt_id'])
                if history:
                    if history['status'].get('status_str') == 'error':
                        raise RuntimeError(json.dumps(history['status'], ensure_ascii=False))
                    images = history.get('outputs', {}).get('8', {}).get('images', [])
                    if images:
                        query = urllib.parse.urlencode(images[0])
                        with urllib.request.urlopen(base+'/view?'+query, timeout=60) as response:
                            data = response.read()
                        temporary = original.with_suffix('.tmp')
                        temporary.write_bytes(data)
                        temporary.replace(original)
                        break
                if time.monotonic() > deadline:
                    raise TimeoutError('Generation deadline: '+name)
                time.sleep(3)
        if not bmp.exists():
            result = prepare(original, bmp, (800, 480), out/'previews'/f'{name}.png')
            (out/'bmp'/f'{name}.json').write_text(json.dumps(dict(item, **result), ensure_ascii=False, indent=2), encoding='utf-8')
        print('READY', item['index'], name, hashlib.sha256(bmp.read_bytes()).hexdigest(), flush=True)
    print('GENERATION_COMPLETE', len(list((out/'bmp').glob('*.bmp'))), flush=True)


if __name__ == '__main__':
    main()
