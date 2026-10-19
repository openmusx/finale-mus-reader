include_guard(GLOBAL)
include(FetchContent)

# Register only requested dependencies: FetchContent's first declaration controls its source.
function(finale_mus_reader_declare_musxdom)
    FetchContent_Declare(musxdom
        GIT_REPOSITORY https://github.com/openmusx/musxdom.git
        GIT_TAG 4f29939c68e2855e0494b059efa15761899374da
        GIT_SHALLOW FALSE)
endfunction()

function(finale_mus_reader_declare_zlib)
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
        URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endfunction()

function(finale_mus_reader_declare_pugixml)
    FetchContent_Declare(pugixml
        URL https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz
        URL_HASH SHA256=655ade57fa703fb421c2eb9a0113b5064bddb145d415dd1f88c79353d90d511a
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endfunction()
