#include "conjure/assets/asset_loader.h"

using namespace conjure;
using namespace conjure::assets;

asset_loader::asset_loader() {}

asset_loader::~asset_loader() {}

core::result<asset_payload> asset_loader::create_fallback() const {
    return make_null_payload();
}
