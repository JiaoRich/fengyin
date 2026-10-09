#pragma once
#include <string>
namespace fengyin::audioengine {
enum class RecoveryRoleAction { preserve, retry, restore };
inline RecoveryRoleAction recoveryRoleAction(const std::wstring& saved,
                                             const std::wstring& current,
                                             const std::wstring& ownedVirtual)
{
    if (saved == L"@preserve") return RecoveryRoleAction::preserve;
    if (current.empty()) return RecoveryRoleAction::retry;
    if (ownedVirtual.empty() || current != ownedVirtual) return RecoveryRoleAction::preserve;
    return RecoveryRoleAction::restore;
}
}
