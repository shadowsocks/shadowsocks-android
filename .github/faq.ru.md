### Решение проблем

Не получается подключиться к серверу:

1. Выключите режим энергосбережения, если он активен;
2. Проверьте настройки;
3. Сотрите данные приложения.

Не работает: [Добавьте отчет](https://github.com/shadowsocks/shadowsocks-android/issues/new) прикрепив лог (logcat), или отправьте отзыв в Google Play. Затем попробуйте стереть данные приложения.

### Как добавить виджет и/или переключать профиль на основании состояния сети?

Использйте интеграцию с [Tasker](http://tasker.dinglisch.net/).

### Как добавить QR код из локальной галереи?

Отсканируйте его при помощи стороннего сканера, например [QuickMark Barcode Scanner](https://play.google.com/store/apps/details?id=tw.com.quickmark) и нажмите на `ss` ссылку.


### Почему режим NAT более не поддерживается?

1. Требуются ROOT права;
2. Нет поддержки IPv6;
3. Нет поддержки UDP relay.

### Как убрать восклицательный знак когда используется режим VPN?

Восклицательный знак на значке Wi-Fi/Сотовой связи появляется, потому что система не может подключиться к portal-серверу (по-умолчанию к `clients3.google.com`) без VPN подключения. Чтобы убрать его, следуйте инструкциям в [этом топике](https://www.noisyfox.cn/45.html). (на упрощенном китайском)

### Почему мой ROM не поддерживается?

1. Некоторые ROM используют неправильную реализацию службы VPN, особенно IPv6;
2. Некоторые ROM используют агрессивную (или так называемую неправильную) фоновую службу с ошибочной политикой остановки;
3. Если вы используете фреймворк Xposed и/или приложения для энергосбережения, вероятно наше приложение не будет нормально работать с ними.

* Иправления для MIUI: [#772](https://github.com/shadowsocks/shadowsocks-android/issues/772)
* Иправления для Huawei: [#1091 (комментарий)](https://github.com/shadowsocks/shadowsocks-android/issues/1091#issuecomment-276949836)
* Вызванные из-за Xposed: [#1414](https://github.com/shadowsocks/shadowsocks-android/issues/1414)
* Samsung и/или Brevent: [#1410](https://github.com/shadowsocks/shadowsocks-android/issues/1410)
* Не устанавливайте это приложение на SD карту, из-за проблем с разрешениями: [#1124 (комментарий)](https://github.com/shadowsocks/shadowsocks-android/issues/1124#issuecomment-307556453)

### Как приостановить службу Shadowsocks?

* Для Android 7.0+: Используйте переключатель в быстрых настройках;
* Используйте интеграцию с Tasker;
* Добавьте профиль c выбранными приложениями, которые будут работать через Shadowsocks прокси, выключив обходной режим.

### Почему Shadowsocks потребляет так много энергии на Android 5.0+?

Так как Shadowsocks работает со всем траффиком устройства, заряд батареи используемый другими приложениями также считается за тот, что использует Shadowsocks. Таким образом использование батареи Shadowsocks'ом равен сумме всей активности устройства в сети. Shadowsocks сам по себе в общем-то является приложением связанного процесса ввода/вывода на современных Android устройствах, которое, ожидаемо, не потребляет значительного количества энергии.

Так что если вы заметите значительное увеличение потребляемой энергии после того, как вы использовали Shadowsocks, это скорее всего вызванно другими приложениями. Например, сервисы Google Play могут потреблять больше энергии после того, как подключатся к Google, и т.д.

Больше информации: https://kb.adguard.com/en/android/solving-problems/battery

### Оно прекрасно работает при Wi-Fi но не может подключиться через сотовую связь?

Разрешите этому приложению передачу данных в фоновом режиме в настройках приложения.

Если у вас оперератор Yota, то измените точку доступа APN с `yota.ru` на `client.yota.ru`

### Почему требуется разрешение камере на устройствах младше Android 6.0?

Чтобы сканировать QR код через встроенный QR сканер.

Кстати, обновите уже свою версию Android.

### Как использовать режим Transproxy (прозрачный прокси)?

1. Установите [AFWall+](https://github.com/ukanth/afwall);
2. Настройте пользовательский скрипт:
```sh
IP6TABLES=/system/bin/ip6tables
IPTABLES=/system/bin/iptables
ULIMIT=/system/bin/ulimit
SHADOWSOCKS_UID=`dumpsys package com.github.shadowsocks | grep userId | cut -d= -f2 - | cut -d' ' -f1 -`
PORT_DNS=5450
PORT_TRANSPROXY=8200
$ULIMIT -n 4096
$IP6TABLES -F
$IP6TABLES -A INPUT -j DROP
$IP6TABLES -A OUTPUT -j DROP
$IPTABLES -t nat -F OUTPUT
$IPTABLES -t nat -A OUTPUT -o lo -j RETURN
$IPTABLES -t nat -A OUTPUT -d 127.0.0.1 -j RETURN
$IPTABLES -t nat -A OUTPUT -m owner --uid-owner $SHADOWSOCKS_UID -j RETURN
$IPTABLES -t nat -A OUTPUT -p tcp --dport 53 -j DNAT --to-destination 127.0.0.1:$PORT_DNS
$IPTABLES -t nat -A OUTPUT -p udp --dport 53 -j DNAT --to-destination 127.0.0.1:$PORT_DNS
$IPTABLES -t nat -A OUTPUT -p tcp -j DNAT --to-destination 127.0.0.1:$PORT_TRANSPROXY
$IPTABLES -t nat -A OUTPUT -p udp -j DNAT --to-destination 127.0.0.1:$PORT_TRANSPROXY
```
3. Настройте пользовательский скрипт отключения:
```sh
IP6TABLES=/system/bin/ip6tables
IPTABLES=/system/bin/iptables
$IPTABLES -t nat -F OUTPUT
$IP6TABLES -F
```
4. Убедитесь, что траффик для Shadowsocks разрешен;
5. Запустите службу прозрачного прокси (transproxy) в Shadowsocks и включите брандмауэр.
