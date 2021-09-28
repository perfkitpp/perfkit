#pragma once
#include <array>
#include <memory>
#include <vector>

#include "perfkit/common/array_view.hxx"

namespace perfkit::trace {
template <typename Ty_, size_t N_>
struct vec_ : std::array<Ty_, N_> {
  using std::array<Ty_, N_>::array;
};

using vec2i = vec_<int, 2>;
using vec2d = vec_<double, 2>;
using vec3d = vec_<double, 3>;

struct size2i : vec2i {
  using vec2i::vec2i;

  auto& w() noexcept { return (*this)[0]; }
  auto& h() noexcept { return (*this)[1]; }
  auto& w() const noexcept { return (*this)[0]; }
  auto& h() const noexcept { return (*this)[1]; }
};

using polygon_view = array_view<vec2d>;

class image {
 public:
  struct buffer {
    size_t length;
    uint8_t data[0];
  };

 public:
  image clone() noexcept;
  void copy_from(size2i size, int bits, void const* data) noexcept;

  void const* raw() const noexcept { return ((buffer*)_raw.get())->data; }
  size2i size() const noexcept { return _size; }
  int channels() const noexcept { return _channels; }

 private:
  std::shared_ptr<uint8_t[]> _raw;
  size2i _size  = {};
  int _channels = 24;
};

class mesh3d {
 public:
  struct vertex {
    vec3d position;
    vec3d normal;
  };

  class factory {
   public:
    std::shared_ptr<mesh3d> generate();
  };

 public:
  auto vertices() const -> std::vector<vertex> const&;
  auto indices() const -> std::vector<std::array<int, 3>> const&;
};

struct interaction {
};

class interactive_image {
 public:
  void reset(vec2i size);
  void reset(image buffer);

  void add_onscreen_text();  // add text on screen, content, scale.

  void add_2d_polygon();  // polygon, position, rotation
  void add_2d_text();     // add floating text

  void add_3d_object();  // mesh, transform
  void add_3d_text();    // add billboard text on 3d space.

  void set_2d_camera_properties();  // zoom, offset, rotation
  void set_3d_camera_properties();  // intrinsic, extrinsic

  void retrieve_interactions();
};

}  // namespace perfkit::trace