# Music Assistant settings patch: remove the dead `volume_control` override.
#
# Symptom: MA forced the station's volume to 0% and it kept snapping back to 0.
# Cause: the MA player for the station had
#     volume_control = media_player.yandexioreceiver_ff98f02920eee05859efb47d
# pointing at the *dead* Yandex stock receiver (the stock quasar stack is stopped
# in pulsar mode). MA could not read/set volume through that entity, defaulted to
# 0, sent Volume(0) to the station, the station applied gain=0 and reported 0 back
# -> feedback loop stuck at 0%.
#
# This script removes the override so MA uses its own internal volume control.
# Run it against the *running* container (the file is owned by root inside it):
#
#   docker exec music-assistant-server python3 /path/to/ma_settings_patch.py
#
# or inline:
#
#   docker exec music-assistant-server python3 -c "import json;d=json.load(open('/data/settings.json'));[d['players'][k]['values'].pop('volume_control',None) for k in d['players']];json.dump(d,open('/data/settings.json','w'),indent=2,ensure_ascii=False)"
#
# Then restart MA:  docker restart music-assistant-server
import json

PATH = "/data/settings.json"
d = json.load(open(PATH))
removed = []
for key, player in d.get("players", {}).items():
    values = player.get("values", {})
    if "volume_control" in values:
        removed.append((key, player.get("default_name") or player.get("name"), values.pop("volume_control")))
if removed:
    json.dump(d, open(PATH, "w"), indent=2, ensure_ascii=False)
    for key, name, vc in removed:
        print(f"removed volume_control from {name} ({key}): was {vc}")
else:
    print("no volume_control overrides found")
