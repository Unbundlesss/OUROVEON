//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

#include "config/base.h"

namespace config {

OURO_CONFIG( Layout )
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::PerAppConfig;
    static constexpr auto StorageFilename   = "layout.json";

    // if we make changes to imgui components or layout infra that would invalidate a saved layout.ini, 
    // incrementing this revision will cause the app to load from the bundled default layout automatically on startup
    // 
    // v4 | 0.8.0 changes, 20230917
    //
    static constexpr uint32_t CurrentGuiMasterRevision = 4;

    uint32_t        guiMasterRevision = 0;

    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( guiMasterRevision )
        );
    }
};
using LayoutOptional = std::optional< Layout >;

} // namespace config

