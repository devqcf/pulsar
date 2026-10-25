# Яндекс Станция Мини 1-го поколения

Мини (`axg_s420_v03`) — 32-битная armhf-система (glibc 2.23) на 64-битном ядре. У неё нет
`yandex,*` в devicetree, `/data/quasar/rootkeeper.sh` и часов/датчика освещённости/Zigbee, поэтому
модель определяется отдельно, а часть сущностей Home Assistant для неё отключена.

## Что поддерживается

- 4 микрофона (`hw:0,2`, S32_LE, 8 каналов, 48 кГц), VQE-пресет `yandexmini`, каналы динамиков
  6–7; каналы 4–5 содержат DC-смещение и не используются.
- 6-светодиодное кольцо на IS31FL3236-совместимом контроллере (`/dev/i2c-0`, `0x3c`), включая
  светомузыку. Анализатор спектра поддерживает 256-точечный блок Мини (768 кадров VQE при
  48 кГц) наряду с 512-точечным блоком Миди/Лайт 2.
- Усилитель: `SPK mute` и gates `/sys/class/tdm_state/amp_table`, `/sys/class/tdm_state/aux_amp`.
- Громкость — программная, применяется к GStreamer-плееру и Sendspin; аппаратный `Master` почти
  не ослабляет сигнал.
- Кнопки `adc_keypad`: TALK (`KEY_POWER`, 116), громкость ±, MUTE (`KEY_MUTE`, 113). MUTE
  переключает программное отключение микрофонов; отдельного аппаратного переключателя нет.
- HA ESPHome API, Assist, объявления, таймеры, Sendspin/Music Assistant, Wi-Fi/Uptime/Restart.

Не создаются сущности часов, яркости/текста/рожиц часов, освещённости, Zigbee и «Case color»:
соответствующего оборудования или файлов `ledconfigs` у Мини нет. Сущность «Hardware mic mute»
показывает состояние кнопки MUTE.

## Установка

Начальный root на заблокированной Мини обычно ставят через UART/NAND; после этого ADB (порт 5555)
и SSH должны работать одновременно. Установщик не получает root сам.

В копии этого репозитория:

```sh
make -C satellite ARCH=armhf ZIG=/path/to/zig-0.14.1 binary
go -C installer run . --list 192.168.1.50
go -C installer run . --yes 192.168.1.50
```

`pulsar-install` определяет Мини по `/proc/device-tree/amlogic-dt-id` = `axg_s420_v03`, выбирает
armhf-бинарник и копирует `satellite/deploy/asound.conf` в `/data/pulsar/asound.conf`. Повторный
запуск обновляет бинарник и скрипты, сохраняя прежний бинарник как `pulsar.prev`.

Если установщик запускается из готового архива, в нём уже должны быть
`satellite/deploy/asound.conf` и armhf-бинарник. Установка вручную:

```sh
adb connect 192.168.1.50:5555
adb -s 192.168.1.50:5555 push satellite/build/armhf/pulsar /data/pulsar/pulsar.new
adb -s 192.168.1.50:5555 push satellite/deploy/asound.conf /data/pulsar/asound.conf.new
for f in boot.sh supervise.sh ctl.sh; do
  adb -s 192.168.1.50:5555 push "satellite/deploy/$f" "/data/pulsar/$f.new"
done
adb -s 192.168.1.50:5555 shell 'cd /data/pulsar && \
  chmod 755 pulsar.new boot.sh.new supervise.sh.new ctl.sh.new && \
  chmod 644 asound.conf.new && \
  mv pulsar.new pulsar && mv asound.conf.new asound.conf && \
  mv boot.sh.new boot.sh && mv supervise.sh.new supervise.sh && mv ctl.sh.new ctl.sh'
```

## Автозапуск

`pulsar-install` пишет `/etc/init.d/S99zpulsar`. На Мини уже есть `S99onboot.sh`, который может
запускать `/data/opt/boot_ssh_adb.sh`; оба пути используют lock-файл `/tmp/pulsar.lock` в
`boot.sh`, поэтому повторный запуск безопасен. Строку в `rootkeeper.sh` добавлять не нужно — на
Мини этого файла нет.

Ручной вариант без установщика — добавить в `/data/opt/boot_ssh_adb.sh` (с резервной копией):

```sh
[ -x /data/pulsar/boot.sh ] && /data/pulsar/boot.sh </dev/null >/dev/null 2>&1 &
```

Затем:

```sh
touch /data/pulsar/enabled
/data/pulsar/ctl.sh enable
```

`ctl.sh disable`, `restart`, `reload`, `status` и `log` работают как на других моделях.

## Проверка

```sh
/data/pulsar/ctl.sh status
tail -100 /tmp/pulsar.log
cat /data/pulsar/state.conf
```

В журнале не должно быть `music analysis: unsupported capture layout`, `clock unavailable`,
`input devices unavailable` и ошибок I2C/ALSA. В HA должны появиться только сущности из списка
выше; часов, Lux, Zigbee и Case color быть не должно.

## Режим без интернета

Если станция должна работать только в LAN, можно запретить ей маршрут по умолчанию. На проверенной
прошивке это делается через `dhcpcd` (после перезагрузки настройка сохраняется):

```sh
cp /etc/dhcpcd.conf /data/pulsar/dhcpcd.conf.bak
printf '\nnogateway\n' >> /etc/dhcpcd.conf
busybox route del default
```

После этого станция не ходит в интернет, но остаются доступны HA, SSH, ADB, Sendspin и mDNS в
пределах `192.168.51.0/24`. Вернуть маршрут: удалить `nogateway` из `/etc/dhcpcd.conf` и
перезагрузиться (или добавить `default gw` вручную). Проверка обновлений при этом ожидаемо
заканчивается `Network is unreachable`; обновляться нужно вручную из архива форка.

## Откат

- Установщик при замене бинарника сохраняет предыдущий в `/data/pulsar/pulsar.prev`; `supervise.sh`
  сам возвращает его после серии падений.
- Быстрый ручной откат: `/data/pulsar/ctl.sh disable`, затем вернуть `pulsar.prev` на место
  `pulsar` и выполнить `ctl.sh enable`.
- Полный откат станции — из проверенного NAND/rootfs-снимка (см. `README-RESTORE.md` в каталоге
  резервной копии). Нельзя делать `dd` raw-образа поверх смонтированного `/dev/ubi0_0`.

## Ограничения

- Начальный root и восстановление NAND требуют UART; сетевой установщик этого не делает.
- MUTE — программный: он останавливает обработку микрофонов, но не отключает их физически.
- Часы, датчик освещённости и Zigbee на Мини 1 отсутствуют; соответствующий код в бинарнике есть,
  но не активируется.
- Автообновление Мини проверяет релизы `devqcf/pulsar`, а не upstream: upstream armhf-бинарник
  собран под glibc 2.30 и не запустится на glibc 2.23. Обновляться можно из архива форка или
  кнопкой Firmware, когда в форке опубликован более новый совместимый релиз.
- Музыкальный анализатор использует блок 256 точек (16 мс при 16 кГц) вместо 512 у других моделей;
  это влияет на частотное разрешение светомузыки, но не на звук.
