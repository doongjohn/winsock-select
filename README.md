# winsock2 study

## bind

- <https://woo-dev.tistory.com/135>\
    > 운영체제는 상대방의 연결 요청을 받아 여러 개의 프로세스 중 적절한 프로세스로 전달해야 합니다.\
    > 이때 어떤 프로세스로 전달할지 구분하기 위해 필요한 식별자가 포트 번호입니다.\
    > 각 프로세스는 당연하게도 중복된 포트 번호를 사용할 수 없습니다.\
    > 이미 사용 중인 포트에 바인딩을 시도한다면 함수는 실패하고 에러를 반환합니다.\
    > 포트를 0으로 지정하면 라이브러리가 알아서 사용 중이지 않은 포트를 골라 바인딩해 줍니다.\
    > 이때 사용되는 포트는 동적 포트(dynamic port)로 [49152, 65535] 범위 내에서 할당됩니다.\
    > 만약 특정한 포트를 지정하고 싶다면 [1024, 49151] 범위 내에서 사용해야 합니다.\
    > 서버는 보통 고정된 포트 번호를 사용해야 하므로 직접 지정해 주는 것이 좋습니다.

## TCP

### socket close & shutdown
- <https://stackoverflow.com/questions/4160347/close-vs-shutdown-socket>

### socket option `SO_LINGER`
- <https://www.ikpil.com/635>

## Async socket
- <https://www.slideshare.net/jacking/winsock-api-wsapoll-fast-loopback>

### select
- <https://stackoverflow.com/questions/24956235/select-function-in-non-blocking-sockets>
- <https://velog.io/@octo__/WSAEventSelect-모델>
- <https://velog.io/@paniksu/C-select-모델-실습>
- `select` will return `error 10022` if there is no valid fd in the fd_set\
    <https://stackoverflow.com/a/56359990>

## Common issue

### check port forwarding
- <https://blog.noip.com/troubleshooting-common-port-forwarding-issues>
- <https://www.portchecktool.com/>

### check `Windows Firewall` and `NAT hairpinning`
- <https://stackoverflow.com/a/52356996>

