#include "ProxySettings.hpp"
#include <sculk/reflection/jsonc/reflection.hpp>

namespace sculk {

bool ProxySettings::load() {
    auto load = reflection::jsonc::load_file(
        *this,
        "./proxy_settings.jsonc",
        reflection::builtin_key_formatter::snake_case_formatter
    );
    return load.has_value();
}

void ProxySettings::save() const {
    reflection::jsonc::save_file(
        *this,
        "./proxy_settings.jsonc",
        reflection::builtin_key_formatter::snake_case_formatter
    );
}

} // namespace sculk