# WEB <-> PERFKIT SERVER PROTOCOL

## API description

#### `/api/login`

**`MULTIPART` PARAMS**

| name   | type   | condition |
|--------|--------|-----------|
| id     | string |           |
| passwd | string | SHA256    |

**`JSON` REPLY**

| name   | type   | desc                      |
|--------|--------|---------------------------|
| status | string | "ADMIN", "GUEST", "ERROR" |

#### `/api/`

## WEBSOCKETS

#### `/stream/tty`

Provides bidirectional pseudo-tty stream.

#### `/stream/window/<id>`

- **TO CLIENT:** MJPEG stream
- **TO SERVER:** User input json object stream

**`JSON` User Input**

| name     | type      | desc |
|----------|-----------|------|
| typecode | int       | *1   |
| value    | <variant> | *1   |

**Typecodes**

| value | name      | value-type            | desc              |
|-------|-----------|-----------------------|-------------------|
| 0     | None      |                       |                   |   
| 100   | MOUSE_CLK | x:int, y:int, btn:int | btn 0:L, 1:R, 2:M |
| 200   | KEYDOWN   | bit[127]              | 0-9a-zA-ZF[1-12]  |
| 201   | KEYUP     | "                     | "                 |
| 300   | CHAR      | string                |                   |

