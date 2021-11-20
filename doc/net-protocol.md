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

> 멀티 유저를 구현하려면 서버 쪽에도 로직이 추가되어야 하는데, 귀찮으므로 ... 그냥 서버는 중계기 역할만 하고,
> (즉, 웹소켓으로 TCP 소켓 입출력을 단순히 forwarding)
>

## API: Session

BSON/JSON 통신 가능하며, 오브젝트 단위로 메시지 넘어간다

실제 데이터 형식은 BSON/JSON으로 퍼블리시되며, 편의상

### HEADER

메시지 공통: 모든 메시지는 8바이트 ASCII 문자열로 시작한다.

첫 4바이트는 식별을 위한 헤더, 다음 4바이트는 Base64 인코딩 된 메시지 페이로드 길이

    <- 4 byte header -><- 4 byte base64 buffer length ->  
    o ` P %            c x Z 3

#### CLIENT -> SERVER

```json
{
  "type": "string; e.g. cmd:reset_cache",
  "parameter": {
    "key": "value"
  }
}
```

#### SERVER -> CLIENT

```json
{
  "type": "string; e.g. update:shell",
  "cache_fence": "int64; ",
  "payload": {
    "key": "value"
  }
}
```

### *cmd:login*

아이디/암호 페어로 로그인 요청.

**REQ**

```yaml
parameter:
  token: string; SHA256 hash created with id and password
```

### *update:error*

모든 종류의 에러/경고에 대해 발송. 로그인하지 않은 세션의 요청에 대해서도 이 채널로 에러가 나간다.

```yaml
payload:
  error_code: string; short stringified error code
  what: string; details of error
```

### *cmd:reset_cache*

내부 캐시 상태를 리셋 요청한다. 일반적으로 login 성공 시 reset이 이루어지므로, client logic에 따라 다시 호출할 일이 없을 수도 있음.

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
```

**REP**

```yaml
payload:
  reply_to: hash64
  new_command: string
  candidates: list<string>
```

### *update:session_info*

세션 정보. reset 이후 1회 송신. Reset의 signal을 겸한다.

```yaml
payload:
  name: string; the session name server initially registered.
  endpoint: string; endpoint of this session. can be used as key
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
  children: list<category_scheme|entity_scheme>

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
  config_key: integer; unique key from 'update:new_config_class'
  value: any; new value
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
  children?: list<node_scheme>; if folded, field itself is skipped. 
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

### *update:window_list*

이미지 리스트 업데이트.

> modality == true이면 창을 강제로 팝업시키거나, 혹은 반짝거리게끔 구현 ...

```yaml
payload:
  content: list<window_meta_scheme>

image_meta_scheme:
  name: string; name of window
  window_key: string; unique string key of window
  modality: boolean; true if window is waiting for user interaction
  size: int16[2]; canvas size
```

### *cmd:window_control*

윈도우 제어. 반복된 open 요청은 무시되며, close 요청은 한 번에 승인(멀티 유저 인터페이스를 위함)

```yaml
parameter:
  guid:
  window_key: string; target window to manipulate
  operation?: string one-of [open|close|yes|no]; 'yes' and 'no' are for modality, that are treated as simple 'close'
    on non-modal windows
  wnd_size: int16[2]; current window size of client machine, which is used for optimize image/draw call transfer.
  camera_3d?: camera_scheme; request new camera status if needed. this can be ignored/overwritten from server side.
    by explicitly calling 'buffer->ignore_client_camera_control'
    position: float[3]
    rotation: float[3]
    hfov: float; 


```

### *update:window_resource*

리소스 업데이트. 윈도우가 open 상태인 동안에만 유효하며, close 호출 후 다시 open 시 전체 리소스가 전부 재송신된다.

> 멀티 클라이언트 구현 시, 명시적으로 open 하지 않은 윈도우에 대해서도 세션 데이터를 유지해야 함!

```yaml
payload:
  window_key: string; key of the target window where resources will be stored.
  textures?: list<texture_scheme>
  meshes?: list<mesh_scheme>
  materials?: list<material_scheme>
  fonts?: list<font_scheme>
  lights?: list<light_scheme>
  deleted_resources?: list<hash64>; list of deleted resource keys

mesh_scheme:
  resource_key: hash64; resource key of this
  vertices: list<vertex_scheme>
  indices: list<int16>

vertex_scheme[]: # array
  0: float[3]; vertex coordinate
  1: fixed_16/15f[3]; vertex normal
  2: fixed_16/15f[2]|byte[4]; uv0 or vertex color
  # maybe uv1?

texture_scheme:
  resource_key: hash64; resource key of this
  offset?: int16[2]; offset of bytes target. if this field is specified, this texture data
    will refresh portion of existing image. otherwise, any different-sized
    or different-typed texture should overwrite existing resources
  size: int16[2]; texture resolution
  type: string; texture encoding type - RAW, BMP, JPEG, PNG, MPEG, H264 ...
  bytes: binary; encoded image stream

material_scheme:
  diffuse?: hash64; resource key to texture (RGBA)
  normal?: hash64; resource key to texture (RGB)
  emmisive?: hash64; resource key to texture (MONO)
  attributes: set<string> [wireframe|flag_names...];

font_scheme:
  font_family: string;
  default_size: float; font default size
  weight: int16; font weight, 1000 as default

light_scheme:
  type: string one-of [point|spot|ambient]; type of light
  position: float[3];
  rotation?: float[3];
  range0: float; light range. ambient light is not affected.
  range1: float2; second light range. ambient light is not affected.
  texture?: hash64; light texture.


```

### *update:window_draw_call*

그리기 요청.

```yaml
payload:
  window_key: string; target window
  content: list<draw_call_scheme>
  camera_3d?: camera_scheme; same as above.

draw_call_scheme[]:
  0: hash64; target texture resource. 0 if draw call targets screen buffer.
  1: byte; draw call type
  2: from [1]; draw call content
    (1=>x10): 2D_BITBLT copy image to target 2d space
      0: hash64; resource key to texture
      1?: int16[2][2]; src image rectangle. if not specified, the whole image will be used.
      2: int16[2]; dst image pivot

    (1=>x11): 2D_STRETCHBLT
      0: hash64; resource key to texture
      1: int16[2][2]; dst image rect
      2?: int16[2][2]; src image rect, if not specified, the whole image will be used.

    (1=>x21): 2D_LINES
      0: byte[4]; color
      1: list<int16[2]>; points to connect
      2: fixed_16/8f; thickness of line

    (1=>x22): 2d_POLYGON
      0: byte[4]; color
      1: list<int16[2]>; points to connect. last point recurses to initial point
        to make closed shape.
      2: boolean; true if filled.

    (1=>x23): 2D_ELLIPSE
      0: byte[4]; color
      1: int16[2][2]; rectangle (x, y, w, h)
      2: boolean; true if filled

    (1=>x24): 2D_TEXT
      0: byte[4]; color
      1: int16[2]; pivot position
      2: hash64; resource key to font
      3: string; content
      4: fixed_16/12f; font scale

    (1=>x30): 3D_OBJECT
      1: hash64; resource key to mesh
      2?: hash64; resource key to material. if not specified, mesh will be rendered as wireframe.
      3: float[3]; position
      4: float[3]; rotation
      5: float[3]; scale

    (1=>x40): 3D_DEBUG_LINES
      1: byte[4]; color
      2: list<float[3]>; points to connect

    (1=>x41): 3D_DEBUG_SPHERE
      1: byte[4]; color
      2: float[3]; point
      3: float; radius
      4: boolean; is_filled

    (1=>x42): 3D_DEBUG_CUBE
      1: byte[4]; color
      2: float[3][2]; AABB
      3: boolean; is_filled


```

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
