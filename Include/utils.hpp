#pragma once

namespace cpt::utils
{
static inline void fatalError(const std::string_view message,
                              const std::source_location location = std::source_location::current())
{
    throw std::runtime_error(std::format("[FATAL ERROR] :: {}.\nLine -> {}.\nColumn -> {}.\nFile Name "
                                         "-> {}.\n, Function Name -> {}.\n",
                                         message, location.line(), location.column(), location.file_name(),
                                         location.function_name()));
}

static inline void dxCheck(const HRESULT hr, const std::source_location location = std::source_location::current())
{
    if (FAILED(hr))
    {
        fatalError("HRESULT FAILED!", location);
    }
}
} // namespace cpt::utils