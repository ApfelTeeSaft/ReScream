local function disable_switchable_huds()
    local tweak = rawget(_G, "tweak_data")
    local version = type(tweak) == "table"
        and rawget(tweak, "version_config") or nil
    if type(version) ~= "table" then
        return false
    end
    version.switchable_huds = false
    return true
end

local function install_gameplay_guard()
    local manager = rawget(_G, "PlayerSwitchableHudManager")
    if type(manager) ~= "table" or type(manager.update) ~= "function" then
        return true
    end

    local installed = rawget(manager, "__rescream_f4_update_guard")
    if type(installed) == "table"
        and type(installed.original) == "function"
        and type(installed.wrapper) == "function"
        and manager.update == installed.wrapper then
        return true
    end

    local original = manager.update
    local wrapper = function(self, ...)
        disable_switchable_huds()
        return original(self, ...)
    end
    rawset(manager, "__rescream_f4_update_guard", {
        original = original,
        wrapper = wrapper
    })
    manager.update = wrapper
    return true
end

local function maintain_f4_fix()
    local flag_ready = disable_switchable_huds()
    local guard_ready = install_gameplay_guard()
    rawset(_G, "__rescream_f4_fix", flag_ready and guard_ready)
    return flag_ready and guard_ready
end

rawset(_G, "__rescream_f4_fix_tick", maintain_f4_fix)
return maintain_f4_fix()
