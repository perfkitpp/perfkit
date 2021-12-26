# Windowing

Perfkit graphics의 모든 드로잉은 texture 단위로 수행된다.

Texture 내에서 bitblt 또는 texturing 등을 통해 다른 texture에 대한 의존성을 부여할 수 있다.

모든 이미지는 registry에 등록되며, enable 상태가 되면 렌더링이 활성화.

간소화된 드로우 콜 프로토콜 제공

```c++

using namespace perfkit::graphics;

// 임의의 버퍼를 업로드.
// force_lossless가 지정되지 않으면 손실 압축으로 대역폭 줄임.
// draw(..., [](...){ ctx->draw(...); }); 의 짧은 버전.
draw(
        texture::key::create("base_buffer"),
        {
            my_data, // must be contiguous, and size of (my_width*my_height*channels(my_texture_format))
            {my_width, my_height},
            my_texture_format,
            my_force_lossless,
        });

// struct perfkit::graphics::vertex_3d
//   pos: VEC3F
//   uv:  VEC2S (-32768~+32767, as -1.0~1.0 range normal)

// struct perfkit::graphics::vertex_instant
//   pos: VEC3F
//   col: VEC4F

// 임의의 3D Mesh를 업로드.
// 폴리곤 세트 및 
draw(
        mesh::create("my_mesh"),
        std::vector<vertex_3d>{},
        std::vector<int16>{});

// font rasterization request.
// EVERY 'declare()' works only once for each keys!
declare(
        font::key::create("my_font"),
        "consolas",
        {
            my_size,
            my_weight,
            my_font_flags,
        });

// material creation request
declare(
        material::key::create("my_mat"),
        {
            texture::key::create("base_buffer"), // diffuse, 4 ch 32BIT
            {}, // normal, 3 ch 24BIT
            {}, // emissive, 3 ch 32BIT
        });

// register image drawing.
draw(
        texture::key::create("my_image"),
        {
            my_texture_width,
            my_texture_height,
        },
        [&](texture::context* ctx){
            ctx->camera(
                    my_cam_pos,
                    my_cam_fwd,
                    my_cam_rwd,
                    my_cam_hfov);
            
            ctx->bitblt(
                    texture::key::create("base_buffer"), // my_image depends on base_buffer
                    to_point, src_rect);
            
            ctx->stretch(
                    texture::key::create("base_buffer"),
                    to_rect, src_rect);
            
            ctx->object(
                    mesh::key::create("my_mesh"), // my_image depends on my_mesh
                    material::key::create("my_mat"), // my_mat depends on my_mat
                    my_position,
                    my_rotation); // AxisAngle
        });

// ERROR: invalid cross dependency
// declare(material::key::create("my_mat"), {texture::key_create("my_image")});

// Draw window.
// The only difference is, size of the window will always be automatically determined.
draw(
        window::key::create("my_window"),
        [&](texture::context* ctx) {
            auto size = ctx->size(); // comes from client 
        });

consume_events(
        window::key::create("my_window"),
        [&](window::events const& e) {
            switch(e.type())
            {
                case window_event::mouse_hover: // every tick mouse is hovering
                case window_event::mouse_move:  // mouse moved
                case window_event::mouse_click: // down & up in same place
                case window_event::mouse_double_click: // down & up in same place x 2
                case window_event::mouse_down:
                case window_event::mouse_up:
                case window_event::mouse_scroll: // continuous scroll event 
                    break;
                
                case window_event::keyboard_down:
                case window_event::keyboard_up:
                case window_event::keyboard_hold: // every tick pressing keyboard
                    break;
            }
        });

```

## 드로우 콜 바이너리 형식

각 드로우 콜의 구성

```yaml
# 'commands' DRAW CALL INDIVIDUAL PACKET HEADER
header[32bit]:
  bit  0 -  6: command type 0~127
  bit  7 - 31: packet payload length
payload[0~2^25 (16MB)]: byte[]; schema varies per header's command type 
```

커맨드 형식

```yaml
_0 NONE:

_2 SYS_RASTERIZE_FONT:
  B 0-7: hash64; key
  B 8-9: int16; font weight
  B 10-13: float; font size
  B 14-15: FLAG16
    F-0: FLAG_ITALIC
  B 16-M: int32[]; rasterize range
  B M-N: string-sz; font-family

_3 SYS_UPLOAD_MESH:
  B 0-7: hash64; key
  B 8-9: int16; number of vertices
  B 10-11: int16; number of indices
  B 12-N: vertex_0[]; default fully-featured vertices
  B N-M: int16[]; indices. 3 indices make up single triangle.
  
_4 SYS_DECLARE_MATERIAL:
  B 0-7: hash64; key
  B 8-15: hash64; albedo texture key, F32_BGR
  B 16-23: hash64; normal texture key, F32_3
  B 24-31: hash64; metalic texture key, F32_1
  B 32-40: hash64; roughness texture key, F32_1
  B 41-47: hash64; opacity texture key, F32_1
  B 48-55: FLAG64
    F-0: FLAG_ALPHA_MODE_CONTINUOUS; if enabled, this material will be treated as alpha texture,
      which will be sorted after all solid mesh renderings.
      otherwse, all non-1 alpha values in opacity texture will be masked

12 TEXTURE_RESHAPE:
  B 0-7: hash64; key, or 0 for null
  B 8-9: int16; width
  B 10-11: int16; height
  B 12: TEXTURE FORMAT
    LOWER 2BIT: channel count 1~4
    UPPER 6BIT: channel TYPE
      E-0: U8
      E-1: U16
      E-2: U32
      E-3: S8
      E-4: S16
      E-5: S32
      E-6: F32
      E-7: SFXN8  # signed fixed point normal
      E-8: SFXN16
      E-9: SFXN32
      E-10: FXN8  # unsigned fixed point normal
      E-11: FXN16
      E-12: FXN32

13 TEXTURE_COPY: # must match with reshape command's format
  B 0-7: hash64; key, or 0 for null
  B 8-9: number of bytes
  B 10: format
    E-0: RAW
    E-1: JPEG # mono or bgr image
  B 11-N: payload

20 RENDER_CONTEXT: select current target texture context
  B 0-7: hash64; target texture to activate context

21 RENDER_BITBLT:
  B 0-7: hash64; source texture key
  B 8-11: int16[2]; target point
  B 12-19: int16[4] source rect

22 RENDER_STRETCH:
  B 0-7: hash64; source texture key
  B 8-15: int16[4]; target point
  B 16-23: int16[4]; source rect

30 BRUSH_SELECT_MESH:
  B 0-7: hash64; mesh

31 BRUSH_SELECT_MATERIAL:
  B 0-7: hash64; material; if null, use color instead, for wireframe rendering.
  B 8-9: FLAG16

32 BRUSH_SELECT_COLOR:
  B 0-3: byte[4]; bgra color
  B 4-7: float; line thickness if available.
    if it's screen domain, it's in pixels
    otherwise(3d), it's in centimeters.

40 RENDER_MESH:
  B 0-11: float[3]; position
  B 12-23: float[3]; scale
  B 24-35: float[3]; rotation

41 RENDER_MESH_INSTANT_3D:
41 RENDER_MESH_INSTANT_2D:
  
60 DRAW2D_MOVETO:
  B 0-3: int16[2]; screen point

61 DRAW2D_LINETO:
  B 0-3: int16[2]; screen point

62 DRAW2D_CIRCLE: draw circle here
  B 0-1: int16; radius in screen point

63 DRAW2D_ELIPSE: draw elipse
  B 0-7: int16[4]; target box

64 DRAW2D_POLY: draw random 2d polygon
  B 0: FLAG8
    F-0: FLAG_CLOSED
    F-1: FLAG_FILL
  B 1-N: int16[2][]; poly lines

70 RENDER_DBG_BILLBOARD_TEXT:
  B 0-11: float[3]; position
  B 12-15: float; scale (in cm)
  B 16-19: FLAG32
    F-0~1: H_ALIGNMENT; [0] align left, [1] align center, [2] align right, [3] NONE
    F-2~3: V_ALIGNMENT; [0] align top, [1] align center, [2] align bottom, [3] NONE
  B 20-23: int32; Text length
  B 24-N: content

71 DRAW2D_DBG_TEXT:
  B 0-3: int16[2]; screen position
  B 4-7: FLAG32
    F-0~1: H_ALIGNMENT; [0] align left, [1] align center, [2] align right, [3] NONE
    F-2~3: V_ALIGNMENT; [0] align top, [1] align center, [2] align bottom, [3] NONE
  B 8-11: int32; string length
  B 12-N: content

```


