#include "perfkit-mongo-config-io.hpp"

#include <mongoc.h>

static void use_mongoc()
{
    struct brrrr_t {
        brrrr_t() noexcept
        {
            ::mongoc_init();
        }

        ~brrrr_t() noexcept
        {
            ::mongoc_cleanup();
        }
    };

    static brrrr_t brrrr{};
    (void)brrrr;
}

auto perfkit::mongo_ext::try_load_configs(
        const char* uri_str,
        const char* storage,
        const char* site,
        const char* name,
        const char* version,
        std::ostream* olog) -> std::optional<global_config_storage_t>
{
    use_mongoc();

    std::optional<global_config_storage_t> result;
    bson_error_t error = {};
    auto uri = mongoc_uri_new_with_error(uri_str, &error);
    if (!uri) {
        *olog << "Failed to parse URI with error: " << error.message << std::endl;
        return {};
    }

    auto client = mongoc_client_new_from_uri(uri);
    if (!client) {
        *olog << "mongoc_client_new_from_uri(uri) failed." << std::endl;
        goto EXIT_URI;
    }

    {
        mongoc_client_set_appname(client, "perfkit-config-loader");
        auto database = mongoc_client_get_database(client, storage);
        auto collection = mongoc_client_get_collection(client, storage, site);

        // Try retrieve data
        auto query = BCON_NEW("$query", "{", "name", name, "version", version, "}");
        auto cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0, query, nullptr, nullptr);
        bson_free(query);

        if (bson_t const* step; mongoc_cursor_next(cursor, &step)) {
            auto str = bson_as_json(step, nullptr);
            auto json = nlohmann::json::parse(str, nullptr, false);

            if (json.is_discarded()) {
                *olog << "Failed to parse retrieved json" << std::endl;
                goto NOPTS;
            }

            if (auto data = json.find("data"); data == json.end()) {
                *olog << "Given json dosen't have 'data' field!" << std::endl;
                goto NOPTS;
            } else {
                json = move(*data);
            }

            result.emplace(move(json));

        NOPTS:
            bson_free(str);
        } else {
            *olog << "Failed to find entry: " << name << ":" << version << ")" << std::endl;
        }

    EXIT_CURSOR:
        mongoc_cursor_destroy(cursor);

        // Release all resource
    EXIT_COLLECTION:
        mongoc_collection_destroy(collection);
        mongoc_database_destroy(database);
        mongoc_client_destroy(client);
    }

EXIT_URI:
    mongoc_uri_destroy(uri);

    return result;
}

bool perfkit::mongo_ext::try_save_configs(
        const char* uri_str,
        const char* storage,
        const char* site,
        const char* name,
        const char* version,
        const global_config_storage_t& cfg,
        std::ostream* olog)
{
    use_mongoc();

    bool result = false;
    bson_error_t error = {};
    auto uri = mongoc_uri_new_with_error(uri_str, &error);
    if (!uri) {
        *olog << "Failed to parse URI with error: " << error.message << std::endl;
        return false;
    }

    auto client = mongoc_client_new_from_uri(uri);
    if (!client) {
        *olog << "mongoc_client_new_from_uri(uri) failed." << std::endl;
        goto EXIT_URI;
    }

    {
        mongoc_client_set_appname(client, "perfkit-config-loader");
        auto database = mongoc_client_get_database(client, storage);
        auto collection = mongoc_client_get_collection(client, storage, site);

        // Dump all data in config
        auto json = nlohmann::json::object({{"name", name}, {"version", version}, {"data", cfg}});
        auto payload = json.dump();
        auto query = BCON_NEW("name", name, "version", version);

        if (auto bson = bson_new_from_json((uint8_t*)payload.data(), (ssize_t)payload.size(), &error)) {
            if (!(result = mongoc_collection_replace_one(collection, query, bson, nullptr, nullptr, &error))) {
                *olog << "Replacement failed. Trying to insert new collection ... (" << error.message << ")" << std::endl;

                if (!(result = mongoc_collection_insert_one(collection, bson, nullptr, nullptr, &error))) {
                    *olog << "Failed to insert collection. All tries exhausted! " << error.message;
                }
            }
        }

    EXIT_COLLECTION:  // Release all resource
        bson_free(query);
        mongoc_collection_destroy(collection);
        mongoc_database_destroy(database);
        mongoc_client_destroy(client);
    }

EXIT_URI:
    mongoc_uri_destroy(uri);
    return result;
}
