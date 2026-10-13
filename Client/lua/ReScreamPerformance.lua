local managers_table = rawget(_G, "managers")
local global = rawget(_G, "Global")

if type(managers_table) ~= "table" or type(global) ~= "table" then
    return false
end

local save_manager = managers_table.save
if type(save_manager) ~= "table"
    or type(save_manager.setup_profile_setting) ~= "function"
    or type(save_manager.get_profile_setting) ~= "function"
    or type(save_manager.set_profile_setting) ~= "function"
    or type(save_manager.save_profile_delayed) ~= "function" then
    return false
end

local available_settings = rawget(save_manager, "_available_profile_setting_map")
local profile_ids = rawget(save_manager, "_profile_id_map")
local save_state = rawget(global, "save_manager")
local profile_info = type(save_state) == "table"
    and rawget(save_state, "profile_info") or nil

if type(available_settings) ~= "table"
    or type(profile_ids) ~= "table"
    or type(profile_info) ~= "table" then
    return false
end

local settings = {
    {
        id = 20002,
        name = "rescream_disable_shadows",
        text = "Disable Shadows",
        render_key = "shadow_quality",
        forced_value = "disabled"
    },
    {
        id = 20003,
        name = "rescream_disable_ssao",
        text = "Disable Ambient Occlusion",
        render_key = "ssao",
        forced_value = false
    },
    {
        id = 20004,
        name = "rescream_disable_dof",
        text = "Disable Depth of Field",
        render_key = "dof",
        forced_value = false
    },
    {
        id = 20005,
        name = "rescream_disable_bloom",
        text = "Disable Bloom",
        render_key = "bloom",
        forced_value = false
    },
    {
        id = 20006,
        name = "rescream_disable_rain",
        text = "Disable Rain Particles",
        rain = true
    }
}

local render_settings_list = {
    settings[1], settings[2], settings[3], settings[4]
}
local remembered_settings = rawget(
    _G, "__rescream_performance_preferences")

local function setting_enabled(setting)
    local current_global = rawget(_G, "Global")
    if type(current_global) == "table"
        and rawget(current_global, setting.name) ~= nil then
        return rawget(current_global, setting.name) == true
    end

    local preferences = rawget(
        _G, "__rescream_performance_preferences")
    return type(preferences) == "table"
        and rawget(preferences, setting.name) == true
end

for _, setting in ipairs(settings) do
    local registered = rawget(available_settings, setting.name)
    if registered == nil then
        local occupied_name = rawget(profile_ids, setting.id)
        if occupied_name ~= nil and occupied_name ~= setting.name then
            return false
        end
        save_manager:setup_profile_setting(setting.id, setting.name, false)
        registered = rawget(available_settings, setting.name)
    end

    if type(registered) ~= "table" or registered.id ~= setting.id then
        return false
    end

    local stored_value = rawget(profile_info, setting.id)
    local selected_value = stored_value
    if type(remembered_settings) == "table"
        and rawget(remembered_settings, setting.name) ~= nil then
        selected_value = rawget(remembered_settings, setting.name)
    end
    if selected_value == nil then
        selected_value = save_manager:get_profile_setting(setting.name, true)
    end
    selected_value = selected_value == true
    rawset(global, setting.name, selected_value)

    if stored_value ~= selected_value then
        save_manager:set_profile_setting(setting.name, selected_value)
    end
end

if type(remembered_settings) ~= "table" then
    remembered_settings = {}
    rawset(_G, "__rescream_performance_preferences",
           remembered_settings)
end
for _, setting in ipairs(settings) do
    remembered_settings[setting.name] = setting_enabled(setting)
end

local render_settings = rawget(_G, "RenderSettings")
local function persistent_render_memory()
    local memory = rawget(
        _G, "__rescream_performance_render_memory")
    if type(memory) ~= "table" then
        memory = { baseline = {}, forced = {} }
        rawset(_G, "__rescream_performance_render_memory", memory)
    end
    if type(memory.baseline) ~= "table" then
        memory.baseline = {}
    end
    if type(memory.forced) ~= "table" then
        memory.forced = {}
    end
    return memory
end

local function ensure_render_state(current_global,
                                   current_render_settings)
    local current_type = type(current_render_settings)
    if type(current_global) ~= "table"
        or (current_type ~= "table" and current_type ~= "userdata") then
        return nil
    end

    local state = rawget(current_global, "__rescream_render_state")
    if type(state) ~= "table" then
        state = { baseline = {}, forced = {} }
        rawset(current_global, "__rescream_render_state", state)
    end
    local previous_baseline = type(state.baseline) == "table"
        and state.baseline or nil
    local previous_forced = type(state.forced) == "table"
        and state.forced or nil
    local memory = persistent_render_memory()

    for _, setting in ipairs(render_settings_list) do
        local key = setting.render_key
        if memory.baseline[key] == nil then
            if previous_baseline ~= nil
                and previous_baseline[key] ~= nil then
                memory.baseline[key] = previous_baseline[key]
            else
                memory.baseline[key] = current_render_settings[key]
            end
        end
        if memory.forced[key] == nil then
            memory.forced[key] = previous_forced ~= nil
                and previous_forced[key] == true or false
        end
    end

    state.baseline = memory.baseline
    state.forced = memory.forced
    return state
end

local function make_render_change_wrapper(original_render_change)
    return function(resolution, requested, ...)
        if type(requested) ~= "table" then
            return original_render_change(resolution, requested, ...)
        end

        local adjusted = {}
        for key, value in pairs(requested) do
            adjusted[key] = value
        end

        local current_global = rawget(_G, "Global")
        local current_render_settings = rawget(_G, "RenderSettings")
        local current_state = ensure_render_state(
            current_global, current_render_settings)
        if current_state == nil then
            return original_render_change(resolution, requested, ...)
        end

        for _, setting in ipairs(render_settings_list) do
            local requested_value = requested[setting.render_key]
            local enabled = setting_enabled(setting)

            if requested_value ~= nil
                and (not enabled
                     or requested_value ~= setting.forced_value) then
                current_state.baseline[setting.render_key] = requested_value
            end

            if enabled then
                adjusted[setting.render_key] = setting.forced_value
                current_state.forced[setting.render_key] = true
            else
                current_state.forced[setting.render_key] = false
            end
        end

        return original_render_change(resolution, adjusted, ...)
    end
end

local function install_render_hook()
    local current = rawget(_G, "rendering_change_resolution")
    if type(current) ~= "function" then
        return false
    end

    local record = rawget(
        _G, "__rescream_performance_render_change_hook")
    if type(record) == "table"
        and type(record.original) == "function"
        and type(record.wrapper) == "function"
        and current == record.wrapper then
        return true
    end

    local original = current
    if type(record) ~= "table" then
        local legacy_original = rawget(
            _G, "__rescream_original_rendering_change_resolution")
        if type(legacy_original) == "function"
            and current ~= legacy_original then
            original = legacy_original
        end
    elseif current == record.wrapper then
        original = record.original
    end
    if type(original) ~= "function" then
        return false
    end

    local wrapper = make_render_change_wrapper(original)
    rawset(_G, "rendering_change_resolution", wrapper)
    if rawget(_G, "rendering_change_resolution") ~= wrapper then
        return false
    end
    rawset(_G, "__rescream_original_rendering_change_resolution",
           original)
    rawset(_G, "__rescream_performance_render_change_hook", {
        original = original,
        wrapper = wrapper
    })
    return true
end

local render_state = ensure_render_state(global, render_settings)
local render_hook_ready = render_state ~= nil and install_render_hook()

local function apply_render_settings()
    local current_render_settings = rawget(_G, "RenderSettings")
    local current_type = type(current_render_settings)
    if current_type ~= "table" and current_type ~= "userdata" then
        return false
    end

    local current_global = rawget(_G, "Global")
    local current_state = ensure_render_state(
        current_global, current_render_settings)
    if current_state == nil then
        return false
    end

    local requested = {
        shadow_quality = current_render_settings.shadow_quality,
        ssao = current_render_settings.ssao,
        dof = current_render_settings.dof,
        bloom = current_render_settings.bloom,
        max_anisotropy = current_render_settings.max_anisotropy
    }

    for _, setting in ipairs(render_settings_list) do
        local key = setting.render_key
        if setting_enabled(setting) then
            if not current_state.forced[key] then
                current_state.baseline[key] = current_render_settings[key]
            end
            requested[key] = setting.forced_value
            current_state.forced[key] = true
        else
            if current_state.forced[key]
                and current_state.baseline[key] ~= nil then
                requested[key] = current_state.baseline[key]
            end
            current_state.forced[key] = false
        end
    end

    for key, value in pairs(requested) do
        current_render_settings[key] = value
    end
    local request = rawget(
        _G, "__rescream_request_render_settings_apply")
    if type(request) == "function" then
        request()
    else
        rawset(_G, "__rescream_pending_render_settings_apply", true)
    end
    return true
end

local function ensure_rain_state(current_global)
    if type(current_global) ~= "table" then
        return nil
    end
    local state = rawget(current_global, "__rescream_rain_state")
    if type(state) ~= "table" then
        state = { requested = false, suppressing = false }
        rawset(current_global, "__rescream_rain_state", state)
    end
    return state
end

local function make_rain_start_wrapper(original_rain_start)
    return function(self, ...)
        local current_global = rawget(_G, "Global")
        local current_state = ensure_rain_state(current_global)
        if current_state ~= nil then
            current_state.requested = true
        end
        if setting_enabled(settings[5]) then
            return
        end
        return original_rain_start(self, ...)
    end
end

local function make_rain_stop_wrapper(original_rain_stop)
    return function(self, ...)
        local current_global = rawget(_G, "Global")
        local current_state = ensure_rain_state(current_global)
        if current_state ~= nil and not current_state.suppressing then
            current_state.requested = false
        end
        return original_rain_stop(self, ...)
    end
end

local function install_rain_hooks()
    local current_class = rawget(_G, "RainEffect")
    if type(current_class) ~= "table" then
        return false
    end
    local current_start = current_class.start
    local current_stop = current_class.stop
    if type(current_start) ~= "function"
        or type(current_stop) ~= "function" then
        return false
    end

    local record = rawget(
        current_class, "__rescream_performance_rain_hook")
    if type(record) == "table"
        and type(record.start_original) == "function"
        and type(record.stop_original) == "function"
        and current_start == record.start_wrapper
        and current_stop == record.stop_wrapper then
        return true
    end

    local original_start = current_start
    local original_stop = current_stop
    if type(record) == "table" then
        if current_start == record.start_wrapper
            and type(record.start_original) == "function" then
            original_start = record.start_original
        end
        if current_stop == record.stop_wrapper
            and type(record.stop_original) == "function" then
            original_stop = record.stop_original
        end
    end
    if type(original_start) ~= "function"
        or type(original_stop) ~= "function" then
        return false
    end

    local start_wrapper = make_rain_start_wrapper(original_start)
    local stop_wrapper = make_rain_stop_wrapper(original_stop)
    current_class.start = start_wrapper
    current_class.stop = stop_wrapper
    if current_class.start ~= start_wrapper
        or current_class.stop ~= stop_wrapper then
        return false
    end

    if rawget(current_class, "__rescream_original_start") == nil then
        rawset(current_class, "__rescream_original_start", original_start)
    end
    if rawget(current_class, "__rescream_original_stop") == nil then
        rawset(current_class, "__rescream_original_stop", original_stop)
    end
    rawset(current_class, "__rescream_performance_rain_hook", {
        start_original = original_start,
        stop_original = original_stop,
        start_wrapper = start_wrapper,
        stop_wrapper = stop_wrapper
    })
    return true
end

local environment_manager = managers_table.environment_effects
local rain_class = rawget(_G, "RainEffect")
local rain_state = ensure_rain_state(global)
local rain_hook_ready = install_rain_hooks()

local function current_rain_effect()
    local current_managers = rawget(_G, "managers")
    local manager = type(current_managers) == "table"
        and current_managers.environment_effects or nil
    if type(manager) ~= "table" or type(manager.effect) ~= "function" then
        return nil
    end
    return manager:effect("rain")
end

local function rain_is_active(effect)
    local grid = type(effect) == "table" and rawget(effect, "_grid") or nil
    if type(grid) ~= "table" then
        return false
    end
    for _, item in ipairs(grid) do
        if type(item) == "table"
            and (rawget(item, "effect") ~= nil
                 or rawget(item, "active") == true) then
            return true
        end
    end
    return false
end

local function apply_rain_setting()
    local current_managers = rawget(_G, "managers")
    local manager = type(current_managers) == "table"
        and current_managers.environment_effects or nil
    local effect = current_rain_effect()
    if type(manager) ~= "table" or type(effect) ~= "table" then
        return false
    end

    local current_global = rawget(_G, "Global")
    local current_rain_state = ensure_rain_state(current_global)
    if current_rain_state == nil then
        return false
    end

    if setting_enabled(settings[5]) then
        if rain_is_active(effect) then
            current_rain_state.requested = true
            current_rain_state.suppressing = true
            manager:stop("rain")
            current_rain_state.suppressing = false
        end
    elseif current_rain_state.requested and not rain_is_active(effect) then
        manager:use("rain")
    end
    return true
end

if rain_hook_ready and not rawget(rain_state, "initialized") then
    local initial_rain_effect = current_rain_effect()
    if type(initial_rain_effect) == "table" then
        rain_state.requested = rain_is_active(initial_rain_effect)
        rain_state.initialized = true
    end
end

local function set_setting(setting, enabled)
    local current_global = rawget(_G, "Global")
    if type(current_global) ~= "table" then
        return
    end
    rawset(current_global, setting.name, enabled == true)
    local preferences = rawget(
        _G, "__rescream_performance_preferences")
    if type(preferences) ~= "table" then
        preferences = {}
        rawset(_G, "__rescream_performance_preferences", preferences)
    end
    preferences[setting.name] = enabled == true

    local current_managers = rawget(_G, "managers")
    local current_save = type(current_managers) == "table"
        and current_managers.save or nil
    if type(current_save) == "table"
        and type(current_save.set_profile_setting) == "function" then
        current_save:set_profile_setting(setting.name, enabled == true)
        if type(current_save.save_profile_delayed) == "function" then
            current_save:save_profile_delayed(true)
        end
    end

    if setting.rain then
        apply_rain_setting()
    else
        apply_render_settings()
    end
end

local function make_toggle(setting)
    return function()
        set_setting(setting, not setting_enabled(setting))
    end
end

local function make_status_check(setting)
    return function()
        return setting_enabled(setting)
    end
end

local function save_settings()
    local current_managers = rawget(_G, "managers")
    local current_save = type(current_managers) == "table"
        and current_managers.save or nil
    if type(current_save) == "table"
        and type(current_save.save_profile_delayed) == "function" then
        current_save:save_profile_delayed(true)
    end
end

local function copy_table(source)
    local result = {}
    for key, value in pairs(source) do
        result[key] = value
    end
    return result
end

local function add_navigation_choice(root_menu)
    if type(root_menu.choices) ~= "table" then
        return false
    end

    local choice = nil
    for _, candidate in ipairs(root_menu.choices) do
        if type(candidate) == "table"
            and rawget(candidate, "__rescream_menu") ==
                "rescream_performance_menu" then
            choice = candidate
            break
        end
    end
    if choice == nil then
        choice = {
            __rescream_menu = "rescream_performance_menu",
            text = "Performance",
            original_text = "Performance",
            ignore_localization = true,
            menu = "rescream_performance_menu"
        }
        table.insert(root_menu.choices,
                     math.min(3, #root_menu.choices + 1), choice)
    end

    if type(root_menu.choices_name) ~= "table" then
        root_menu.choices_name = {}
    end
    root_menu.choices_name["Performance"] = choice
    return true
end

local function add_menu_choice(menu, setting)
    if type(menu.choices) ~= "table" then
        return false
    end

    local choice = nil
    for _, candidate in ipairs(menu.choices) do
        if type(candidate) == "table"
            and rawget(candidate, "__rescream_setting") == setting.name then
            choice = candidate
            break
        end
    end

    if choice == nil then
        choice = {
            __rescream_setting = setting.name,
            text = setting.text,
            original_text = setting.text,
            ignore_localization = true,
            callback = make_toggle(setting),
            status_check = make_status_check(setting)
        }
        table.insert(menu.choices, choice)
    end

    if type(menu.choices_name) ~= "table" then
        menu.choices_name = {}
    end
    menu.choices_name[setting.text] = choice
    return true
end

local function install_menu(menu_manager)
    local menues = type(menu_manager) == "table"
        and rawget(menu_manager, "_menues") or nil
    local root_menu = type(menues) == "table"
        and rawget(menues, "rescream_menu") or nil
    if type(root_menu) ~= "table"
        or not rawget(root_menu, "__rescream_owned")
        or type(root_menu.fields) ~= "table"
        or type(root_menu.fields[1]) ~= "table" then
        return false
    end

    local menu = rawget(menues, "rescream_performance_menu")
    if menu ~= nil
        and (type(menu) ~= "table"
             or not rawget(menu, "__rescream_owned")) then
        return false
    end
    if menu == nil then
        local title = copy_table(root_menu.fields[1])
        title.text = "Performance"
        title.original_text = "Performance"
        title.ignore_localization = true
        menu = {
            __rescream_owned = true,
            name = "rescream_performance_menu",
            choices = {},
            choices_name = {},
            fields = { title },
            refresh_on_user_change = "true",
            deactivate_callback = save_settings
        }
        menues.rescream_performance_menu = menu
    end

    for _, setting in ipairs(settings) do
        if not add_menu_choice(menu, setting) then
            return false
        end
    end
    return add_navigation_choice(root_menu)
end

rawset(_G, "__rescream_install_performance_menu", install_menu)

local menu_ready = false
local menu_manager = managers_table.menu
local menu_class = rawget(_G, "MainMenuManager")
if type(menu_manager) == "table" and type(menu_class) == "table"
    and type(menu_class.setup_menues) == "function" then
    if rawget(menu_class,
              "__rescream_original_setup_menues_performance") == nil then
        local original_setup_menues = menu_class.setup_menues
        rawset(menu_class,
               "__rescream_original_setup_menues_performance",
               original_setup_menues)
        menu_class.setup_menues = function(self, ...)
            original_setup_menues(self, ...)
            local installer = rawget(
                _G, "__rescream_install_performance_menu")
            if type(installer) == "function" then
                installer(self)
            end
        end
    end
    menu_ready = install_menu(menu_manager)
end

local render_ready = render_hook_ready
if render_ready and not rawget(render_state, "initialized") then
    for _, setting in ipairs(render_settings_list) do
        if setting_enabled(setting) then
            render_ready = apply_render_settings()
            break
        end
    end
    if render_ready then
        render_state.initialized = true
    end
end
local rain_ready = rain_hook_ready and apply_rain_setting()

local function restore_generation_settings(current_global, current_save)
    if type(current_global) ~= "table" or type(current_save) ~= "table"
        or type(current_save.setup_profile_setting) ~= "function"
        or type(current_save.set_profile_setting) ~= "function" then
        return false
    end
    local available = rawget(
        current_save, "_available_profile_setting_map")
    local identifiers = rawget(current_save, "_profile_id_map")
    local current_save_state = rawget(current_global, "save_manager")
    local current_profile = type(current_save_state) == "table"
        and rawget(current_save_state, "profile_info") or nil
    if type(available) ~= "table" or type(identifiers) ~= "table"
        or type(current_profile) ~= "table" then
        return false
    end

    local preferences = rawget(
        _G, "__rescream_performance_preferences")
    if type(preferences) ~= "table" then
        return false
    end
    for _, setting in ipairs(settings) do
        local registered = rawget(available, setting.name)
        if registered == nil then
            local occupied = rawget(identifiers, setting.id)
            if occupied ~= nil and occupied ~= setting.name then
                return false
            end
            current_save:setup_profile_setting(
                setting.id, setting.name, false)
            registered = rawget(available, setting.name)
        end
        if type(registered) ~= "table" or registered.id ~= setting.id then
            return false
        end
        local enabled = rawget(preferences, setting.name) == true
        rawset(current_global, setting.name, enabled)
        current_save:set_profile_setting(setting.name, enabled)
    end
    return true
end

local function prepare_generation_render_state(current_global,
                                               current_render_settings)
    return ensure_render_state(current_global, current_render_settings)
        ~= nil
end

local function apply_active_render_settings()
    local memory = persistent_render_memory()
    for _, setting in ipairs(render_settings_list) do
        if setting_enabled(setting)
            or memory.forced[setting.render_key] == true then
            return apply_render_settings()
        end
    end
    return true
end

local function maintain_performance_generation()
    local generation = rawget(
        _G, "__rescream_performance_generation")
    if type(generation) ~= "table" then
        generation = {}
        rawset(_G, "__rescream_performance_generation", generation)
    end

    local current_managers = rawget(_G, "managers")
    local current_global = rawget(_G, "Global")
    local current_save = type(current_managers) == "table"
        and current_managers.save or nil
    local current_render_settings = rawget(_G, "RenderSettings")
    local current_render_type = type(current_render_settings)
    local runtime_changed = generation.global ~= current_global
        or generation.save ~= current_save
    local profile_ready = true
    if runtime_changed then
        if not restore_generation_settings(current_global, current_save) then
            profile_ready = false
        else
            generation.global = current_global
            generation.save = current_save
        end
    end

    local current_render_helper = rawget(
        _G, "rendering_change_resolution")
    local render_changed = runtime_changed
        or generation.render_settings ~= current_render_settings
        or generation.render_helper ~= current_render_helper
    local current_render_ready = not render_changed
        and generation.render_settings ~= nil
        and (current_render_type == "table"
             or current_render_type == "userdata")
        and type(current_render_helper) == "function"
    if render_changed and profile_ready
        and (current_render_type == "table"
             or current_render_type == "userdata") then
        current_render_ready = prepare_generation_render_state(
            current_global, current_render_settings)
            and install_render_hook()
        if current_render_ready then
            current_render_ready = apply_active_render_settings()
        end
        if current_render_ready then
            generation.render_settings = current_render_settings
            generation.render_helper = rawget(
                _G, "rendering_change_resolution")
        end
    end

    local current_menu = type(current_managers) == "table"
        and current_managers.menu or nil
    local current_menues = type(current_menu) == "table"
        and rawget(current_menu, "_menues") or nil
    local current_menu_ready = type(current_menu) == "table"
        and type(current_menues) == "table"
        and generation.menu == current_menu
        and generation.menues == current_menues
    if not current_menu_ready then
        current_menu_ready = install_menu(current_menu)
        if current_menu_ready then
            generation.menu = current_menu
            generation.menues = current_menues
        end
    end

    local current_environment = type(current_managers) == "table"
        and current_managers.environment_effects or nil
    local current_rain_class = rawget(_G, "RainEffect")
    local current_rain_start = type(current_rain_class) == "table"
        and current_rain_class.start or nil
    local current_rain_stop = type(current_rain_class) == "table"
        and current_rain_class.stop or nil
    local rain_changed = generation.environment ~= current_environment
        or generation.rain_class ~= current_rain_class
        or generation.rain_start ~= current_rain_start
        or generation.rain_stop ~= current_rain_stop
    local current_rain_ready = not rain_changed
        and type(current_environment) == "table"
        and type(current_rain_class) == "table"
        and type(current_rain_start) == "function"
        and type(current_rain_stop) == "function"
    if rain_changed and profile_ready then
        current_rain_ready = install_rain_hooks()
            and apply_rain_setting()
        if current_rain_ready then
            generation.environment = current_environment
            generation.rain_class = current_rain_class
            generation.rain_start = current_rain_class.start
            generation.rain_stop = current_rain_class.stop
        end
    end

    local complete = profile_ready and current_render_ready
        and current_menu_ready and current_rain_ready
    rawset(_G, "__rescream_performance_menu", current_menu_ready)
    rawset(_G, "__rescream_performance_render_hook",
           current_render_ready)
    rawset(_G, "__rescream_performance_rain_hook",
           current_rain_ready)
    rawset(_G, "__rescream_performance_fix", complete)
    rawset(_G, "__rescream_performance_status",
           "menu=" .. (current_menu_ready and "ready" or "waiting")
           .. " render="
           .. (current_render_ready and "ready" or "waiting")
           .. " rain="
           .. (current_rain_ready and "ready" or "waiting"))
    return complete
end

rawset(_G, "__rescream_performance_tick",
       maintain_performance_generation)
local initial_generation = {
    global = global,
    save = save_manager
}
if render_ready then
    initial_generation.render_settings = render_settings
    initial_generation.render_helper = rawget(
        _G, "rendering_change_resolution")
end
if menu_ready then
    initial_generation.menu = menu_manager
    initial_generation.menues = rawget(menu_manager, "_menues")
end
if rain_ready then
    initial_generation.environment = environment_manager
    initial_generation.rain_class = rain_class
    initial_generation.rain_start = rain_class.start
    initial_generation.rain_stop = rain_class.stop
end
rawset(_G, "__rescream_performance_generation", initial_generation)

rawset(_G, "__rescream_performance_menu", menu_ready)
rawset(_G, "__rescream_performance_render_hook", render_ready)
rawset(_G, "__rescream_performance_rain_hook", rain_ready)
rawset(_G, "__rescream_performance_fix",
       menu_ready and render_ready and rain_ready)
rawset(_G, "__rescream_performance_status",
       "menu=" .. (menu_ready and "ready" or "waiting")
       .. " render=" .. (render_ready and "ready" or "waiting")
       .. " rain=" .. (rain_ready and "ready" or "waiting"))
return menu_ready and render_ready and rain_ready
