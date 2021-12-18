# TCP PROTOCOL spec for perfkit remote GUI provider

1. perfkit의 net provider에서 모든 데이터 형식 + 시퀀스 번호만 정의 및 분류. API 서버는 json object 형태로만 데이터를 다루게 된다. Websocket으로 업데이트 있을 때마다 모든
   클라이언트에 데이터 갱신 지속적으로 쏴주다가, 신규 연결 또는 reset 시 전체 데이터를 보내는 식
2. net provider는 JSON 또는 BSON으로 데이터 송신 모드 설정 가능.
3. 즉, net provider와 apiserver는 기본적으로 같은 데이터를 송신한다. 단, net provider는 연결 시 데이터를 consistent하게 client에게 보내고, 송신 마친 데이터는
   flush -> 즉, 로컬에 캐시되지 않음, apiserver는 데이터를 캐시하고 + web interface 제공
4. 따라서, API 스펙은 먼저 net-provider에 대해 완전해야 하며, API server 없이도 순수 소켓 연결을 통해 client가 동작할 수 있어야 한다.
5. net provider는 최초 생성 시 tcp server mode 또는 apiserver provider mode 선택 가능. 후자를 선택 시 apiserver에 연결 시도하고, 연결 성공하기 전까지는
   fallback으로 tcp server mode로 동작

> 따라서 perfkit 쪽은 단일 연결만 유지하는 tcp 서버 하나 두고, 연결 갱신 또는 reset 요청마다 시퀀스 리셋하고
> 송신한 뒤 이후부터 업데이트 보내는 방식으로 구현

> 멀티 유저를 구현하려면 서버 쪽에도 로직이 추가되어야 하는데, 귀찮으므로 ... 그냥 서버는 중계기 역할만 하게
> (즉, 웹소켓으로 TCP 소켓 입출력을 단순히 forwarding)
> APIserver 구현 시, 세션에 따라 url을 다르게 해 노출 가능하게 설정 ...

## API: Session

BSON/JSON 통신 가능하며, 오브젝트 단위로 메시지 넘어간다

실제 데이터 형식은 MesasgePack/JSON으로 퍼블리시되며, 편의상 YAML로 표기

### DATA TYPES PER ROUTE CHANNELS

```yaml
auth: json
cmd, update, rpc: messagepack
```

### HEADER

메시지 공통: 모든 메시지는 8바이트 ASCII 문자열로 시작한다.

첫 4바이트는 식별을 위한 헤더, 다음 4바이트는 메시지 페이로드 길이

    <- 4 byte header -><- 4 byte base64 buffer length ->  
    o ` P %            c x Z 3
    111 96 80 37

이 때, 항상 route 필드가 먼저 파싱될 수 있도록 array 필드에 순서를 정해 헤더에 넣는다.

#### CLIENT -> SERVER

```json
{
  "route": "string; e.g. cmd:reset_cache",
  "parameter": {
    "key": "value; parameter"
  }
}
```

#### SERVER -> CLIENT

```json
{
  "route": "string; e.g. update:shell",
  "fence": "uint64; sequence number",
  "payload": {
    "key": "value;"
  }
}
```

### *auth:login*

아이디/암호로 생성된 SHA256 해시로 로그인 요청. 항상 전체 메시지를 JSON 형식으로 날려야 한다.

**REQ**

```yaml
parameter:
  id: string; id
  pw: string; SHA256 hash created with password
```

### *update:error*

모든 종류의 에러/경고에 대해 발송. 로그인하지 않은 세션의 요청에 대해서도 이 채널로 에러가 나간다.

```yaml
payload:
  error_code: string; short stringified error code
  what: string; details of error
```

### *cmd:reset_cache*

내부 캐시 상태를 리셋 요청한다. login과 동시에 1회 보내는 것이 기본.

```yaml
parameter:
# none
```

### *cmd:heartbeat*

클라이언트가 딱히 보낼 게 없을 때, 서버의 timeout disconnection을 방지하기 위해 송신.

### *cmd:push_command*

커맨드 푸시 요청

```yaml
parameter:
  command: string
```

### *rpc:suggest_command*

명령행 자동 완성 요청
**REQ**

```yaml
parameter:
  reply_to: hash64; a hash to recognize rpc reply
  command: string
  position: int32;
```

**REP**

```yaml
payload:
  reply_to: hash64
  new_command: string
  candidates: list<string>
```

### *update:epoch*

최초의 패킷을 나타내는 세션 정보.

```yaml
payload:
  name: string; the session name server initially registered.
  hostname: string; machine name of session host device.
  keystr: string; keystring of this session. may contain various unique informations
  epoch: int64; timestamp in milliseconds that the server was initially created
  description: string; description of this session.
  num_cores: int32; number of machine cores of given session's system
```

### *update:session_state*

세션 상태 정보. CPU 점유율, 메모리 사용량 등.

```yaml
payload:
  cpu_usage: double; cpu usage percentage. can exceeds 100% if program utilizes more than 1 core
  memory_usage: int64; number of bytes occupied by this process.
  bw_out: int32; average number of bytes read in from socket
  bw_in: int32; average number of bytes write out to socket
```

### *update:shell_output*

```yaml
payload:
  content: string
```

### *update:new_config_class*

```yaml
payload:
  key: string
  root: category_scheme

category_scheme:
  name: string
  subcategories: list<category_scheme>
  entities: list<entity_scheme>
  
entity_scheme:
  name: string
  config_key: hash64; a unique key in a config class scope
  value: any; current value
  metadata:
    description: string
    default_value: any
    min: any
    max: any
    ...
```

### *update:config_entity*

```yaml
payload:
  class_key: string; name of config class
  content: list<entity_scheme>;

entity_scheme:
  config_key: hash64; unique key from 'update:new_config_class'
  value: any; new value
```

### *cmd:configure_entity*

```yaml
parameter:
  class_key: string; name of config class
  content: list<entity_scheme>; same as above
     
entity_scheme: <same as above>
```

### *update:trace_class_list*

추가/제거된 트레이스 클래스가 있을 때, 전체 리스트를 다시 보냄

```yaml
payload:
  content: list<string>; list of new trace classes
```

### *cmd:signal_fetch_traces*

트레이스는 fetch를 위해 먼저 signaling이 필요 ... signaling 대상이 될 트레이스 리스트를 보낸다.

```yaml
parameter:
  targets: list<string>; list of trace classes 
```

### *update:traces*

```yaml
payload:
  class_name: string; name of trace class
  root: node_scheme

node_scheme:
  name: string
  trace_key: hash64; unique key in trace class scope
  subscribe: boolean; if subscribing this node
  value: oneof [int64|double|string|timestamp|boolean];
  children?: list<node_scheme>; if folded, field will be empty. 
```

### *cmd:control_trace*

각 트레이스 노드의 상태 전환

```yaml
parameter:
  class_name: string; name of trace clsas
  trace_key: hash64
  fold?: boolean; whether to fold or not a trace node
  subscribe?: boolean; whether to subscribe or not a trace node
```

## Windowing

[참조:Windowing](./windowing.md)

### *cmd:window_control*

```yaml
parameter:
  key: string; target window or image name
  watch: bool; enable or disable
```

### *update:draw_call*

모든 종류의 드로우 콜

```yaml
parameter:
   count: int32; number of draw calls
   commands: byte[]; packed bytes of draw calls
```

드로우 콜에 대해서는 위의 [Windowing](windowing.md) 문서 참조


### *cmd:window_interact*

```yaml
parameter:
  window_key: string; target window name
  events: list<event_scheme>

event_scheme[]:
  3: int64; event timestamp
  1: byte; event type
  2: from [1]; event content
    (0=>x10): SIG_MOUSE_ENTER; mouse enters to window
    (0=>x11): SIG_MOUSE_LEAVE; mouse leaves from window
    (0=>x20): TICK_MOUSE_HOVER
    (0=>x21): TICK_MOUSE_MOVE
    (0=>x31): EVT_MOUSE_CLICK
    (0=>x32): EVT_MOUSE_DBL_CLICK
    (0=>x33): EVT_MOUSE_DOWN
    (0=>x34): EVT_MOUSE_UP
      0: int16[2]; mouse x, y
      1: bitset<16>; mouse button status bitmask
        B-0: LEFT_BUTTON
        B-1: RIGHT_BUTTON
        B-2: MIDDLE_BUTTON
        B-8: KEY_CTRL
        B-9: KEY_ALT
        B-A: KEY_SHIFT
        B-B: KEY_SPACE

    (0=>x40): EVT_MOUSE_WHEEL_UP
    (0=>x41): EVT_MOUSE_WHEEL_DN
      0: int16[2]; mouse x, y
      1: float; wheel amount. 1 is standard.

    (0=>x42): EVT_WIDE; zoom in/out event. can be generated from touch device
    (0=>x43): EVT_NEAR
      0: int16[2]; pivot x, y
      1: float; amount of zoom, x1.0 is default.

    (0=>x80): EVT_KEY_DOWN
    (0=>x81): EVT_KEY_UP
    (0=>x82): EVT_KEY_INPUT_RAW
      0: list<byte>; list of keycodes from event.

    (0=>x83): EVT_KEY_INPUT_IME
      0: string; utf-8 text input

    (0=>xD0): EVT_TOUCH_DOWN
    (0=>xD1): EVT_TOUCH_MOVE
    (0=>xD2): EVT_TOUCH_UP


```
