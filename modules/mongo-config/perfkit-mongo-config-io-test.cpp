#include "perfkit-mongo-config-io.hpp"

int main(void)
{
    char const* uri = getenv("MONGO_URI");
    char const* storage = getenv("MONGO_STORAGE");
    char const* site = getenv("MONGO_SITE");
    char const* name = getenv("MONGO_NAME");
    char const* version = getenv("MONGO_VERSION");

    auto v = perfkit::mongo_ext::try_load_configs(
            uri,
            storage,
            site,
            name,
            version);

    nlohmann::json f = *v;
    std::cout << f.dump(4) << std::endl;

    std::cout << "Save Config: "
              << perfkit::mongo_ext::try_save_configs(uri, storage, site, name, version, *v)
              << std::endl;
}
