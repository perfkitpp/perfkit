#pragma once

namespace perfkit::rgl {
enum class command {
    none,

    texture = 1,
    render_target = 2,
    window = 3,
};

enum class texture_command {
    none
};

enum class render_target_command {
    none
};

enum class window_command {
    none
};

enum class client_event {
    none,

    subscribe_resource = 1001,
    unsubscribe_resource = 1002,
};

enum class resource_type {
    none,
    texture,
    render_target,
    mesh,
    material
};

struct basic_handle {
    uint64_t value;

   private:
    template <size_t OfstBits, size_t MaskBits>
    uint64_t get() const noexcept
    {
        return (value >> OfstBits) & ((1llu << MaskBits) - 1);
    }

    template <size_t OfstBits, size_t MaskBits>
    void set(uint64_t v) noexcept
    {
        value &= ~(((1llu << MaskBits) - 1) << OfstBits);
        value |= (v & (1llu << MaskBits) - 1) << OfstBits;
    }

   public:
    auto index() const noexcept { return this->get<0, 16>(); }
    void index(uint64_t v) noexcept { this->set<0, 16>(v); }

    auto id() const noexcept { return this->get<16, 45>(); }
    void id(uint64_t v) noexcept { this->set<16, 45>(v); }

    auto type() const noexcept { return (resource_type)this->get<61, 3>(); }
    void type(resource_type v) noexcept { this->set<61, 3>(uint64_t(v)); }

    static constexpr size_t max_index() noexcept { return 1 << 16; }
};

}  // namespace perfkit::rgl
