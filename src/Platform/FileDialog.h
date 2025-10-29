#pragma once
#include <string>

namespace Diligent {
namespace Platform {

// Opens a native file dialog for picking a glTF/glb file.
// Returns true and sets outPath on success; returns false if the user cancels or on error.
bool OpenFileDialogGLTF(std::string& outPath);

} // namespace Platform
} // namespace Diligent
