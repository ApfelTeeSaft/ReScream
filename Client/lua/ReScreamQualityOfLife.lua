local DISPLAY_WINDOWED = 1
local DISPLAY_FULLSCREEN = 2
local DISPLAY_BORDERLESS = 3
local DISPLAY_TRANSITION_VERSION = 3
local DISPLAY_SETTLE_UPDATES = 8
local DEBUG_OVERLAY_LAYER = 3000
local DEBUG_OVERLAY_RECREATE_UPDATES = 2
local NETWORK_GUI_ROW = 30
local host_pcall = rawget(_G, "pcall")

local function protected_call(callable, ...)
    if type(host_pcall) == "function" then
        return host_pcall(callable, ...)
    end
    return true, callable(...)
end

local function current_global()
    local value = rawget(_G, "Global")
    return type(value) == "table" and value or nil
end

local function current_managers()
    local value = rawget(_G, "managers")
    return type(value) == "table" and value or nil
end

local function current_save_manager()
    local managers_table = current_managers()
    local value = managers_table and managers_table.save or nil
    return type(value) == "table" and value or nil
end

local function current_menu_manager()
    local managers_table = current_managers()
    local value = managers_table and managers_table.menu or nil
    return type(value) == "table" and value or nil
end

local function current_debug_manager()
    local managers_table = current_managers()
    local value = managers_table and managers_table.debug or nil
    return type(value) == "table" and value or nil
end

local function current_debug_gui()
    local debug_manager = current_debug_manager()
    local value = debug_manager and debug_manager.gui or nil
    return type(value) == "table" and value or nil
end

local function current_viewport_manager()
    local managers_table = current_managers()
    local value = managers_table and managers_table.viewport or nil
    local value_type = type(value)
    if value_type == "table" or value_type == "userdata" then
        return value
    end
    return nil
end

local function current_render_settings()
    local value = rawget(_G, "RenderSettings")
    local value_type = type(value)
    if value_type == "table" or value_type == "userdata" then
        return value
    end
    return nil
end

local function current_core_setup()
    local value = rawget(_G, "core_setup")
    return type(value) == "table" and value or nil
end

local function normalize_display_mode(value)
    value = tonumber(value)
    if value == DISPLAY_WINDOWED or value == DISPLAY_FULLSCREEN
        or value == DISPLAY_BORDERLESS then
        return value
    end

    local render_settings = current_render_settings()
    if render_settings ~= nil and render_settings.fullscreen == false then
        return DISPLAY_WINDOWED
    end
    return DISPLAY_FULLSCREEN
end

local function normalize_boolean(value)
    return value == true
end

local function display_mode()
    local global = current_global()
    return normalize_display_mode(
        global and rawget(global, "rescream_display_mode") or nil)
end

local function setting_enabled(name)
    local global = current_global()
    return global ~= nil and rawget(global, name) == true
end

local function seed_runtime_defaults()
    local global = current_global()
    if global == nil then
        return false
    end

    if rawget(global, "rescream_show_fps") == nil then
        rawset(global, "rescream_show_fps", false)
    end
    if rawget(global, "rescream_show_network_packets") == nil then
        rawset(global, "rescream_show_network_packets", false)
    end
    return true
end

local function register_setting(id, name, default_value, normalize)
    local global = current_global()
    local save_manager = current_save_manager()
    if global == nil or save_manager == nil
        or type(save_manager.setup_profile_setting) ~= "function"
        or type(save_manager.get_profile_setting) ~= "function"
        or type(save_manager.set_profile_setting) ~= "function" then
        return false
    end

    local available_settings = rawget(
        save_manager, "_available_profile_setting_map")
    local profile_ids = rawget(save_manager, "_profile_id_map")
    local save_state = rawget(global, "save_manager")
    local profile_info = type(save_state) == "table"
        and rawget(save_state, "profile_info") or nil
    if type(available_settings) ~= "table"
        or type(profile_ids) ~= "table"
        or type(profile_info) ~= "table" then
        return false
    end

    local setting = rawget(available_settings, name)
    if setting == nil then
        local occupied_name = rawget(profile_ids, id)
        if occupied_name ~= nil and occupied_name ~= name then
            return false
        end
        save_manager:setup_profile_setting(id, name, default_value)
        setting = rawget(available_settings, name)
    end
    if type(setting) ~= "table" or setting.id ~= id then
        return false
    end

    local stored_value = rawget(profile_info, id)
    local selected_value = stored_value
    if selected_value == nil then
        selected_value = save_manager:get_profile_setting(name, true)
    end
    if selected_value == nil then
        selected_value = rawget(global, name)
    end
    if selected_value == nil then
        selected_value = default_value
    end
    selected_value = normalize(selected_value)
    rawset(global, name, selected_value)
    if stored_value ~= selected_value then
        save_manager:set_profile_setting(name, selected_value)
    end
    return true
end

local function register_profile_settings()
    local remembered = rawget(_G, "__rescream_qol_preferences")
    seed_runtime_defaults()
    local display_ready = register_setting(
        20007, "rescream_display_mode", normalize_display_mode(nil),
        normalize_display_mode)
    local fps_ready = register_setting(
        20008, "rescream_show_fps", false, normalize_boolean)
    local network_ready = register_setting(
        20009, "rescream_show_network_packets", false,
        normalize_boolean)
    local ready = display_ready and fps_ready and network_ready
    local global = current_global()
    local save_manager = current_save_manager()
    if ready and global ~= nil and type(remembered) == "table" then
        local remembered_display = normalize_display_mode(
            rawget(remembered, "display_mode"))
        local remembered_fps = normalize_boolean(
            rawget(remembered, "show_fps"))
        local remembered_network = normalize_boolean(
            rawget(remembered, "show_network_packets"))
        rawset(global, "rescream_display_mode", remembered_display)
        rawset(global, "rescream_show_fps", remembered_fps)
        rawset(global, "rescream_show_network_packets", remembered_network)
        save_manager:set_profile_setting(
            "rescream_display_mode", remembered_display)
        save_manager:set_profile_setting(
            "rescream_show_fps", remembered_fps)
        save_manager:set_profile_setting(
            "rescream_show_network_packets", remembered_network)
    elseif ready and global ~= nil then
        remembered = {
            display_mode = display_mode(),
            show_fps = setting_enabled("rescream_show_fps"),
            show_network_packets = setting_enabled(
                "rescream_show_network_packets")
        }
        rawset(_G, "__rescream_qol_preferences", remembered)
    end
    return ready
end

local function remember_preference(name, value)
    local remembered = rawget(_G, "__rescream_qol_preferences")
    if type(remembered) ~= "table" then
        remembered = {}
        rawset(_G, "__rescream_qol_preferences", remembered)
    end
    remembered[name] = value
end

local function set_profile_value(name, value)
    local save_manager = current_save_manager()
    if save_manager == nil
        or type(save_manager.set_profile_setting) ~= "function" then
        return false
    end

    local available_settings = rawget(
        save_manager, "_available_profile_setting_map")
    if type(available_settings) ~= "table"
        or type(rawget(available_settings, name)) ~= "table" then
        return false
    end
    save_manager:set_profile_setting(name, value)
    return true
end

local function save_settings()
    local save_manager = current_save_manager()
    if save_manager ~= nil
        and type(save_manager.save_profile_delayed) == "function" then
        save_manager:save_profile_delayed(true)
    end
end

local function native_borderless_active()
    return rawget(_G, "__rescream_native_borderless_active") == true
end

local function request_debug_overlay_rebuild(delay)
    local invalidate = rawget(
        _G, "__rescream_invalidate_debug_overlay")
    if type(invalidate) == "function" then
        protected_call(
            invalidate, delay or DEBUG_OVERLAY_RECREATE_UPDATES)
    else
        rawset(_G, "__rescream_debug_overlay_recreate_updates",
               delay or DEBUG_OVERLAY_RECREATE_UPDATES)
    end
end

local function persist_display_config(desired_fullscreen)
    local get_mode = rawget(_G, "core_setup_get_mode")
    local get_aspect = rawget(_G, "core_setup_get_aspect")
    local file_api = rawget(_G, "File")
    local application = rawget(_G, "Application")
    local render_settings = current_render_settings()
    if type(get_mode) ~= "function" or type(get_aspect) ~= "function"
        or file_api == nil or application == nil
        or type(file_api.open) ~= "function"
        or type(application.render_settings_filename) ~= "function"
        or render_settings == nil then
        return false
    end

    local mode_ok, resolution, persisted_fullscreen, persisted =
        protected_call(get_mode)
    if not mode_ok or resolution == nil or type(persisted) ~= "table" then
        return false
    end
    if persisted_fullscreen == desired_fullscreen then
        return true
    end

    local aspect_ok, aspect = protected_call(get_aspect)
    local filename_ok, filename = protected_call(
        application.render_settings_filename, application)
    if not aspect_ok or not filename_ok or filename == nil
        or resolution.x == nil or resolution.y == nil then
        return false
    end
    local refresh = resolution.z
    local live_resolution = render_settings.resolution
    if refresh == nil and live_resolution ~= nil then
        refresh = live_resolution.z
    end
    local adapter = render_settings.adapter
    if refresh == nil or adapter == nil
        or persisted.shadow_quality == nil
        or persisted.max_anisotropy == nil or persisted.ssao == nil
        or persisted.dof == nil or persisted.bloom == nil then
        return false
    end

    local xml = "<renderer_config>\n\t<d3d_device adapter=\""
        .. tostring(adapter) .. "\" resolution=\""
        .. tostring(resolution.x) .. " " .. tostring(resolution.y)
        .. "\" refresh_rate=\"" .. tostring(refresh)
        .. "\" windowed=\"" .. tostring(not desired_fullscreen)
        .. "\" aspect_ratio=\"" .. tostring(aspect) .. "\"/>\n"
        .. "\t<render_settings>\n"
        .. "\t\t<variable name=\"shadow_quality\" value=\""
        .. tostring(persisted.shadow_quality) .. "\"/>\n"
        .. "\t\t<variable name=\"max_anisotropy\" value=\""
        .. tostring(persisted.max_anisotropy) .. "\"/>\n"
        .. "\t\t<variable name=\"ssao\" value=\""
        .. tostring(persisted.ssao) .. "\"/>\n"
        .. "\t\t<variable name=\"dof\" value=\""
        .. tostring(persisted.dof) .. "\"/>\n"
        .. "\t\t<variable name=\"bloom\" value=\""
        .. tostring(persisted.bloom) .. "\"/>\n"
        .. "\t</render_settings>\n</renderer_config>"

    local open_ok, handle = protected_call(
        file_api.open, file_api, filename, "w")
    if not open_ok or handle == nil or type(handle.puts) ~= "function"
        or type(handle.close) ~= "function" then
        return false
    end
    local puts_ok = protected_call(handle.puts, handle, xml)
    local close_ok = protected_call(handle.close, handle)
    return puts_ok and close_ok
end

local function display_transition_state(global)
    local state = rawget(global, "__rescream_display_transition")
    if type(state) ~= "table"
        or state.version ~= DISPLAY_TRANSITION_VERSION then
        state = {
            version = DISPLAY_TRANSITION_VERSION,
            target = nil,
            phase = nil,
            stable_updates = 0,
            force_apply = false,
            config_fullscreen = nil,
            config_retry_updates = 0
        }
        rawset(global, "__rescream_display_transition", state)
    end
    return state
end

local function begin_display_transition(global, mode, force_apply)
    local state = display_transition_state(global)
    state.target = mode
    state.phase = "release_native"
    state.stable_updates = 0
    if force_apply == true then
        state.force_apply = true
    end
    rawset(_G, "__rescream_native_borderless_requested", false)
    return state
end

local function complete_display_transition(global, state, mode)
    state.phase = "complete"
    state.stable_updates = 0
    rawset(global, "__rescream_applied_display_mode", mode)
    rawset(_G, "__rescream_renderer_transition_pending", false)
    rawset(_G, "__rescream_native_borderless_requested",
           mode == DISPLAY_BORDERLESS)
end

local function sync_display_config(state, desired_fullscreen)
    if state.config_fullscreen == desired_fullscreen then
        return true
    end
    local retry_updates = tonumber(state.config_retry_updates) or 0
    if retry_updates > 0 then
        state.config_retry_updates = retry_updates - 1
        return false
    end
    if persist_display_config(desired_fullscreen) then
        state.config_fullscreen = desired_fullscreen
        state.config_retry_updates = 0
        return true
    end
    state.config_retry_updates = 120
    return false
end

local function advance_display_transition()
    local global = current_global()
    local render_settings = current_render_settings()
    local core_setup_table = current_core_setup()
    if global == nil or render_settings == nil or core_setup_table == nil then
        return false
    end

    local mode = display_mode()
    local desired_fullscreen = mode == DISPLAY_FULLSCREEN
    local state = display_transition_state(global)
    if state.target ~= mode then
        state = begin_display_transition(global, mode)
    elseif state.phase == nil then
        if render_settings.fullscreen ~= desired_fullscreen
            or (mode == DISPLAY_BORDERLESS
                and not native_borderless_active())
            or (mode ~= DISPLAY_BORDERLESS
                and native_borderless_active()) then
            state.config_fullscreen = nil
            state = begin_display_transition(global, mode)
        else
            sync_display_config(state, desired_fullscreen)
            complete_display_transition(global, state, mode)
            return true
        end
    end

    if state.phase == "complete" then
        if core_setup_table.flag_rs_apply == true then
            state.phase = "release_native"
            state.stable_updates = 0
            rawset(_G, "__rescream_native_borderless_requested", false)
            rawset(_G, "__rescream_renderer_transition_pending",
                   not native_borderless_active())
            return true
        end
        if render_settings.fullscreen ~= desired_fullscreen
            or (mode == DISPLAY_BORDERLESS
                and not native_borderless_active())
            or (mode ~= DISPLAY_BORDERLESS
                and native_borderless_active()) then
            state.config_fullscreen = nil
            state = begin_display_transition(global, mode)
        else
            sync_display_config(state, desired_fullscreen)
            rawset(_G, "__rescream_renderer_transition_pending", false)
            rawset(_G, "__rescream_native_borderless_requested",
                   mode == DISPLAY_BORDERLESS)
            return true
        end
    end

    if state.phase == "release_native" then
        rawset(_G, "__rescream_native_borderless_requested", false)
        if native_borderless_active() then
            rawset(_G, "__rescream_renderer_transition_pending", false)
            return true
        end
        rawset(_G, "__rescream_renderer_transition_pending",
               core_setup_table.flag_rs_apply == true)
        if core_setup_table.flag_rs_apply == true then
            return true
        end
        state.phase = "apply_renderer"
    end

    if state.phase == "apply_renderer" then
        rawset(_G, "__rescream_native_borderless_requested", false)
        sync_display_config(state, desired_fullscreen)
        if state.force_apply == true
            or render_settings.fullscreen ~= desired_fullscreen then
            request_debug_overlay_rebuild(
                DEBUG_OVERLAY_RECREATE_UPDATES)
            render_settings.fullscreen = desired_fullscreen
            core_setup_table.flag_rs_apply = true
            state.force_apply = false
        end
        if core_setup_table.flag_rs_apply == true then
            state.phase = "wait_renderer"
            state.stable_updates = 0
            rawset(_G, "__rescream_renderer_transition_pending", true)
            return true
        end
        state.phase = "settle_renderer"
        state.stable_updates = 0
    end

    if state.phase == "wait_renderer" then
        rawset(_G, "__rescream_native_borderless_requested", false)
        rawset(_G, "__rescream_renderer_transition_pending", true)
        if core_setup_table.flag_rs_apply == true then
            state.stable_updates = 0
            return true
        end
        if render_settings.fullscreen ~= desired_fullscreen then
            state.phase = "apply_renderer"
            state.stable_updates = 0
            rawset(_G, "__rescream_renderer_transition_pending", false)
            return true
        end
        state.phase = "settle_renderer"
        state.stable_updates = 0
    end

    if state.phase == "settle_renderer" then
        rawset(_G, "__rescream_native_borderless_requested", false)
        rawset(_G, "__rescream_renderer_transition_pending", false)
        if core_setup_table.flag_rs_apply == true then
            state.phase = "wait_renderer"
            state.stable_updates = 0
            rawset(_G, "__rescream_renderer_transition_pending", true)
            return true
        end
        if render_settings.fullscreen ~= desired_fullscreen then
            state.phase = "apply_renderer"
            state.stable_updates = 0
            return true
        end
        state.stable_updates = (state.stable_updates or 0) + 1
        if state.stable_updates < DISPLAY_SETTLE_UPDATES then
            return true
        end
        if mode == DISPLAY_BORDERLESS then
            state.phase = "wait_native"
            rawset(_G, "__rescream_native_borderless_requested", true)
        else
            complete_display_transition(global, state, mode)
            return true
        end
    end

    if state.phase == "wait_native" then
        rawset(_G, "__rescream_renderer_transition_pending", false)
        if core_setup_table.flag_rs_apply == true
            or render_settings.fullscreen ~= desired_fullscreen then
            state = begin_display_transition(global, mode)
            rawset(_G, "__rescream_renderer_transition_pending",
                   core_setup_table.flag_rs_apply == true)
            return true
        end
        rawset(_G, "__rescream_native_borderless_requested", true)
        if native_borderless_active() then
            complete_display_transition(global, state, mode)
        end
    end
    return true
end

local function apply_display_mode()
    return advance_display_transition()
end

local function request_render_settings_apply()
    local global = current_global()
    if global == nil then
        rawset(_G, "__rescream_pending_render_settings_apply", true)
        return false
    end
    begin_display_transition(global, display_mode(), true)
    rawset(_G, "__rescream_pending_render_settings_apply", false)
    return advance_display_transition()
end

rawset(_G, "__rescream_request_render_settings_apply",
       request_render_settings_apply)

local function set_display_mode(mode)
    mode = normalize_display_mode(mode)
    local global = current_global()
    if global ~= nil then
        rawset(global, "rescream_display_mode", mode)
        begin_display_transition(global, mode)
    end
    remember_preference("display_mode", mode)
    set_profile_value("rescream_display_mode", mode)
    apply_display_mode()
    save_settings()
end

local function install_core_update_hook(method_name, original_name)
    local core_setup_table = current_core_setup()
    if core_setup_table == nil then
        return false
    end

    local hooks = rawget(_G, "__rescream_core_update_hooks")
    if type(hooks) ~= "table" then
        hooks = {}
        rawset(_G, "__rescream_core_update_hooks", hooks)
    end
    local installed = rawget(hooks, method_name)
    if type(installed) == "table"
        and installed.owner == core_setup_table
        and rawget(core_setup_table, method_name) == installed.wrapper then
        return true
    end

    local original = rawget(core_setup_table, method_name)
    if type(original) ~= "function" then
        return false
    end
    local wrapper = function(self, ...)
        original(self, ...)
        if rawget(_G, "__rescream_native_qol_driver") ~= true then
            local advance = rawget(
                _G, "__rescream_quality_of_life_tick")
            if type(advance) == "function" then
                advance()
            end
        end
    end
    rawset(core_setup_table, original_name, original)
    core_setup_table[method_name] = wrapper
    hooks[method_name] = {
        owner = core_setup_table,
        original = original,
        wrapper = wrapper
    }
    return true
end

local function install_display_hooks()
    local update_ready = install_core_update_hook(
        "update", "__rescream_original_update_display_mode")
    local paused_ready = install_core_update_hook(
        "paused_update", "__rescream_original_paused_update_display_mode")
    return update_ready and paused_ready
end

local overlay_backend_ready

local function native_fps_backend_ready()
    local menu_manager = current_menu_manager()
    local debug_manager = current_debug_manager()
    local macro = debug_manager and debug_manager.macro or nil
    return menu_manager ~= nil
        and type(menu_manager.toggle_fps_debug) == "function"
        and type(menu_manager.is_fps_debug_enabled) == "function"
        and type(macro) == "table"
        and type(macro.fps) == "function"
        and type(macro.is_fps_enabled) == "function"
end

local function fps_enabled()
    local menu_manager = current_menu_manager()
    if menu_manager ~= nil
        and type(menu_manager.is_fps_debug_enabled) == "function" then
        return menu_manager:is_fps_debug_enabled() == true
    end
    return false
end

local function apply_fps_setting()
    local desired = setting_enabled("rescream_show_fps")
    if overlay_backend_ready ~= nil and overlay_backend_ready() then
        if native_fps_backend_ready() and fps_enabled() then
            current_menu_manager():toggle_fps_debug()
        end
        local sync = rawget(_G, "__rescream_sync_debug_overlay")
        return type(sync) == "function" and sync() or false
    end
    if native_fps_backend_ready() then
        local menu_manager = current_menu_manager()
        if fps_enabled() ~= desired then
            menu_manager:toggle_fps_debug()
        end
        local sync = rawget(_G, "__rescream_sync_debug_overlay")
        if type(sync) == "function" then
            sync()
        end
        return fps_enabled() == desired
    end

    local sync = rawget(_G, "__rescream_sync_debug_overlay")
    return type(sync) == "function" and sync() or false
end

local function set_fps_setting()
    local enabled = not setting_enabled("rescream_show_fps")
    local global = current_global()
    if global ~= nil then
        rawset(global, "rescream_show_fps", enabled)
    end
    remember_preference("show_fps", enabled)
    set_profile_value("rescream_show_fps", enabled)
    apply_fps_setting()
    save_settings()
end

local function network_debug_text()
    local value = rawget(_G, "__rescream_network_debug_text")
    if type(value) ~= "string" or value == "" then
        return "Steam P2P packet counters initializing..."
    end
    return value
end

local function engine_object(value)
    local value_type = type(value)
    if value_type == "table" or value_type == "userdata" then
        return value
    end
    return nil
end

local function native_network_backend_ready()
    local debug_gui = current_debug_gui()
    return debug_gui ~= nil
        and type(debug_gui.set_func) == "function"
        and type(debug_gui.set) == "function"
        and type(debug_gui.set_color) == "function"
        and type(debug_gui.set_enabled) == "function"
end

overlay_backend_ready = function()
    local overlay = engine_object(rawget(_G, "Overlay"))
    local timer_manager = engine_object(rawget(_G, "TimerManager"))
    local color = engine_object(rawget(_G, "Color"))
    return overlay ~= nil and type(overlay.newgui) == "function"
        and timer_manager ~= nil
        and type(timer_manager.main) == "function"
        and type(timer_manager.wall) == "function"
        and color ~= nil and color.white ~= nil
end

local function engine_alive(value)
    if engine_object(value) == nil then
        return false
    end
    local alive_function = rawget(_G, "alive")
    if type(alive_function) == "function" then
        local ok, result = protected_call(alive_function, value)
        if ok then
            return result == true
        end
    end
    local core_code = rawget(_G, "CoreCode")
    if type(core_code) == "table"
        and type(core_code.alive) == "function" then
        local ok, result = protected_call(core_code.alive, value)
        if ok then
            return result == true
        end
    end
    return true
end

local function protected_method(object, name, ...)
    if engine_object(object) == nil then
        return false, nil
    end
    local lookup_ok, method = protected_call(function()
        return object[name]
    end)
    if not lookup_ok or type(method) ~= "function" then
        return false, nil
    end
    return protected_call(method, object, ...)
end

local function overlay_element_ready(element)
    return engine_alive(element)
        and type(element.set_visible) == "function"
        and type(element.set_text) == "function"
end

local function debug_overlay_state_ready(state)
    return type(state) == "table"
        and engine_alive(state.workspace)
        and engine_alive(state.panel)
        and type(state.panel.set_visible) == "function"
        and overlay_element_ready(state.fps)
        and overlay_element_ready(state.network)
end

local function destroy_debug_overlay(recreate_updates)
    local state = rawget(_G, "__rescream_debug_overlay")
    rawset(_G, "__rescream_debug_overlay", nil)
    if type(state) == "table" and engine_alive(state.workspace) then
        local gui = state.gui
        if engine_object(gui) == nil
            or type(gui.destroy_workspace) ~= "function" then
            local overlay = rawget(_G, "Overlay")
            local ok, replacement = protected_method(overlay, "newgui")
            if ok then
                gui = replacement
            end
        end
        protected_method(gui, "destroy_workspace", state.workspace)
    end

    local delay = tonumber(recreate_updates)
    if delay ~= nil and delay >= 0 then
        local existing = tonumber(rawget(
            _G, "__rescream_debug_overlay_recreate_updates")) or 0
        rawset(_G, "__rescream_debug_overlay_recreate_updates",
               math.max(existing, delay))
    end
    return true
end

rawset(_G, "__rescream_invalidate_debug_overlay",
       destroy_debug_overlay)

local function overlay_recreation_blocked()
    local core_setup_table = current_core_setup()
    return (core_setup_table ~= nil
            and core_setup_table.flag_rs_apply == true)
        or (tonumber(rawget(
            _G, "__rescream_debug_overlay_recreate_updates")) or 0) > 0
end

local function create_debug_overlay()
    local existing = rawget(_G, "__rescream_debug_overlay")
    if debug_overlay_state_ready(existing) then
        return existing
    end
    if overlay_recreation_blocked() or not overlay_backend_ready() then
        return nil
    end

    local overlay = rawget(_G, "Overlay")
    local timer_manager = rawget(_G, "TimerManager")
    local color = rawget(_G, "Color")
    local gui_ok, gui = protected_method(overlay, "newgui")
    if not gui_ok then
        return nil
    end
    if engine_object(gui) == nil
        or type(gui.create_screen_workspace) ~= "function" then
        return nil
    end
    local workspace_ok, workspace = protected_method(
        gui, "create_screen_workspace")
    if not workspace_ok or not engine_alive(workspace)
        or type(workspace.panel) ~= "function"
        or type(workspace.set_timer) ~= "function" then
        return nil
    end
    local timer_ok, main_timer = protected_method(timer_manager, "main")
    if not timer_ok
        or not protected_method(workspace, "set_timer", main_timer) then
        protected_method(gui, "destroy_workspace", workspace)
        return nil
    end

    local panel_ok, panel = protected_method(workspace, "panel")
    if not panel_ok or not engine_alive(panel)
        or type(panel.text) ~= "function"
        or type(panel.set_visible) ~= "function" then
        protected_method(gui, "destroy_workspace", workspace)
        return nil
    end
    if type(panel.set_layer) == "function" then
        protected_method(panel, "set_layer", DEBUG_OVERLAY_LAYER)
    end

    local fps_ok, fps = protected_method(panel, "text", {
        name = "rescream_fps_debug",
        text = "FPS: initializing...",
        x = 18,
        y = 16,
        font = "gmfont_22",
        font_size = 22,
        color = color.white,
        layer = 1
    })
    local network_ok, network = protected_method(panel, "text", {
        name = "rescream_network_debug",
        text = network_debug_text(),
        x = 18,
        y = 44,
        font = "gmfont_22",
        font_size = 22,
        color = color.white,
        layer = 1
    })
    if not fps_ok or not network_ok
        or not overlay_element_ready(fps)
        or not overlay_element_ready(network) then
        protected_method(gui, "destroy_workspace", workspace)
        return nil
    end

    local state = {
        gui = gui,
        workspace = workspace,
        panel = panel,
        fps = fps,
        network = network,
        elapsed = 1,
        fps_average = nil
    }
    rawset(_G, "__rescream_debug_overlay", state)
    return state
end

local function fallback_selection()
    return setting_enabled("rescream_show_fps"),
        setting_enabled("rescream_show_network_packets")
end

local function sync_debug_overlay()
    local show_fps, show_network = fallback_selection()
    local state = rawget(_G, "__rescream_debug_overlay")
    if type(state) == "table" and not debug_overlay_state_ready(state) then
        destroy_debug_overlay(1)
        state = nil
    end
    if not show_fps and not show_network then
        if debug_overlay_state_ready(state) then
            protected_method(state.fps, "set_visible", false)
            protected_method(state.network, "set_visible", false)
            protected_method(state.panel, "set_visible", false)
            if type(state.workspace.hide) == "function" then
                protected_method(state.workspace, "hide")
            end
        end
        return overlay_backend_ready()
            or native_fps_backend_ready()
            or native_network_backend_ready()
    end

    if overlay_recreation_blocked() then
        return overlay_backend_ready()
    end
    state = create_debug_overlay()
    if state == nil then
        return false
    end
    if type(state.workspace.show) == "function" then
        protected_method(state.workspace, "show")
    end
    local fps_ok = protected_method(
        state.fps, "set_visible", show_fps)
    local network_ok = protected_method(
        state.network, "set_visible", show_network)
    local panel_ok = protected_method(state.panel, "set_visible", true)
    if not fps_ok or not network_ok or not panel_ok then
        destroy_debug_overlay(1)
        return false
    end
    return true
end

rawset(_G, "__rescream_sync_debug_overlay", sync_debug_overlay)

local function update_debug_overlay()
    local core_setup_table = current_core_setup()
    if core_setup_table ~= nil
        and core_setup_table.flag_rs_apply == true then
        return
    end
    local recreate_updates = tonumber(rawget(
        _G, "__rescream_debug_overlay_recreate_updates")) or 0
    if recreate_updates > 0 then
        rawset(_G, "__rescream_debug_overlay_recreate_updates",
               recreate_updates - 1)
        return
    end
    if not sync_debug_overlay() then
        return
    end
    local state = rawget(_G, "__rescream_debug_overlay")
    if type(state) ~= "table" then
        return
    end

    local show_fps, show_network = fallback_selection()
    if not show_fps and not show_network then
        return
    end

    local timer_manager = rawget(_G, "TimerManager")
    if engine_object(timer_manager) == nil
        or type(timer_manager.wall) ~= "function" then
        return
    end
    local wall_ok, wall_timer = protected_method(timer_manager, "wall")
    if not wall_ok or engine_object(wall_timer) == nil
        or type(wall_timer.delta_time) ~= "function" then
        return
    end
    local delta_ok, delta = protected_method(wall_timer, "delta_time")
    if not delta_ok or type(delta) ~= "number" or delta <= 0 then
        return
    end

    local instantaneous = 1 / delta
    if state.fps_average == nil then
        state.fps_average = instantaneous
    else
        state.fps_average = state.fps_average * 0.9
            + instantaneous * 0.1
    end
    state.elapsed = (state.elapsed or 0) + delta
    if state.elapsed < 0.25 then
        return
    end
    state.elapsed = 0
    if show_fps then
        if not protected_method(state.fps, "set_text", string.format(
            "FPS: %.1f", state.fps_average)) then
            destroy_debug_overlay(1)
            return
        end
    end
    if show_network then
        if not protected_method(
            state.network, "set_text", network_debug_text()) then
            destroy_debug_overlay(1)
        end
    end
end

local function on_resolution_changed()
    destroy_debug_overlay(DEBUG_OVERLAY_RECREATE_UPDATES)
end

rawset(_G, "__rescream_on_resolution_changed",
       on_resolution_changed)

local function install_resolution_changed_hook()
    local viewport = current_viewport_manager()
    if viewport == nil
        or type(viewport.add_resolution_changed_func) ~= "function" then
        return false
    end
    local hook = rawget(_G, "__rescream_resolution_changed_hook")
    if type(hook) == "table" and hook.owner == viewport then
        return true
    end
    local callback_function = function()
        local handler = rawget(
            _G, "__rescream_on_resolution_changed")
        if type(handler) == "function" then
            protected_call(handler)
        end
    end
    local ok, identifier = protected_method(
        viewport, "add_resolution_changed_func", callback_function)
    if not ok then
        return false
    end
    rawset(_G, "__rescream_resolution_changed_hook", {
        owner = viewport,
        identifier = identifier,
        callback = callback_function
    })
    return true
end

local function repair_runtime_generation()
    local generation = rawget(_G, "__rescream_qol_runtime_generation")
    if type(generation) ~= "table" then
        generation = {}
        rawset(_G, "__rescream_qol_runtime_generation", generation)
    end

    local global = current_global()
    local save_manager = current_save_manager()
    local viewport = current_viewport_manager()
    local overlay_owner = rawget(_G, "Overlay")
    if generation.overlay_owners_initialized == true then
        if generation.overlay_global ~= global
            or generation.viewport ~= viewport
            or generation.overlay_owner ~= overlay_owner then
            destroy_debug_overlay(DEBUG_OVERLAY_RECREATE_UPDATES)
        end
    else
        generation.overlay_owners_initialized = true
    end
    generation.overlay_global = global
    generation.viewport = viewport
    generation.overlay_owner = overlay_owner

    if generation.global ~= global or generation.save ~= save_manager then
        if register_profile_settings() then
            generation.global = global
            generation.save = save_manager
        end
    end

    local menu_manager = current_menu_manager()
    local menues = type(menu_manager) == "table"
        and rawget(menu_manager, "_menues") or nil
    if generation.menu ~= menu_manager or generation.menues ~= menues then
        local installer = rawget(
            _G, "__rescream_install_quality_of_life_menu")
        local installed = type(installer) == "function"
            and installer(menu_manager) == true
        if installed then
            generation.menu = menu_manager
            generation.menues = menues
        end
    end

    local menu_class = rawget(_G, "MainMenuManager")
    local installed_menu_hook = type(menu_class) == "table"
        and rawget(menu_class,
                   "__rescream_quality_of_life_setup_hook") or nil
    local menu_hook_stale = type(menu_class) == "table"
        and (type(installed_menu_hook) ~= "table"
             or installed_menu_hook.owner ~= menu_class
             or installed_menu_hook.wrapper ~= menu_class.setup_menues)
    if generation.menu_class ~= menu_class or menu_hook_stale then
        local hook_installer = rawget(
            _G, "__rescream_install_quality_of_life_hooks")
        local installed = type(hook_installer) == "function"
            and hook_installer() == true
        if installed then
            generation.menu_class = menu_class
        end
    end

    local performance_maintainer = rawget(
        _G, "__rescream_performance_tick")
    if type(performance_maintainer) == "function" then
        protected_call(performance_maintainer)
    end
end

local function update_quality_of_life()
    repair_runtime_generation()
    install_display_hooks()
    install_resolution_changed_hook()
    if rawget(_G, "__rescream_pending_render_settings_apply") == true then
        request_render_settings_apply()
    else
        advance_display_transition()
    end
    local maintain = rawget(
        _G, "__rescream_maintain_debug_backends")
    if type(maintain) == "function" then
        protected_call(maintain)
    end
    update_debug_overlay()
end

rawset(_G, "__rescream_quality_of_life_tick",
       update_quality_of_life)
rawset(_G, "__rescream_advance_borderless_request",
       update_quality_of_life)

local function clear_network_gui(gui)
    if type(gui) ~= "table" then
        return
    end
    if type(gui.set_func) == "function" then
        protected_call(gui.set_func, gui, NETWORK_GUI_ROW, nil)
    end
    if type(gui.set) == "function" then
        protected_call(gui.set, gui, NETWORK_GUI_ROW, "")
    end
end

local function apply_network_setting()
    local global = current_global()
    if global == nil then
        return false
    end

    local state = rawget(global, "__rescream_network_debug_state")
    if type(state) ~= "table" then
        state = { active = false, gui = nil, refresh_updates = 0 }
        rawset(global, "__rescream_network_debug_state", state)
    end

    local enabled = setting_enabled("rescream_show_network_packets")
    local debug_gui = current_debug_gui()
    local backend_ready = native_network_backend_ready()

    if overlay_backend_ready() then
        state.refresh_updates = (state.refresh_updates or 0) + 1
        if state.gui ~= nil and state.gui ~= debug_gui then
            clear_network_gui(state.gui)
        end
        if backend_ready and (state.active or state.gui ~= debug_gui
            or state.refresh_updates >= 30) then
            clear_network_gui(debug_gui)
            state.refresh_updates = 0
        end
        state.active = false
        state.gui = debug_gui
        return sync_debug_overlay()
    end

    if not backend_ready then
        if state.active then
            clear_network_gui(state.gui)
            state.active = false
            state.gui = nil
            state.refresh_updates = 0
        end
        return sync_debug_overlay()
    end

    if enabled then
        if state.active and state.gui ~= debug_gui then
            clear_network_gui(state.gui)
            state.active = false
            state.refresh_updates = 0
        end
        state.refresh_updates = (state.refresh_updates or 0) + 1
        if not state.active or state.refresh_updates >= 30 then
            debug_gui:set_func(NETWORK_GUI_ROW, network_debug_text)
            debug_gui:set_color(NETWORK_GUI_ROW, 0.35, 0.85, 1, 1)
            state.active = true
            state.gui = debug_gui
            state.refresh_updates = 0
        end
        debug_gui:set_enabled(true)
        if type(debug_gui.set_enabled_paused) == "function" then
            debug_gui:set_enabled_paused(true)
        end
    elseif state.active then
        clear_network_gui(state.gui or debug_gui)
        state.active = false
        state.gui = nil
        state.refresh_updates = 0
    end
    sync_debug_overlay()
    return true
end

local function set_network_setting()
    local enabled = not setting_enabled("rescream_show_network_packets")
    local global = current_global()
    if global ~= nil then
        rawset(global, "rescream_show_network_packets", enabled)
    end
    remember_preference("show_network_packets", enabled)
    set_profile_value("rescream_show_network_packets", enabled)
    apply_network_setting()
    save_settings()
end

rawset(_G, "__rescream_maintain_debug_backends", function()
    apply_fps_setting()
    apply_network_setting()
end)

local function copy_table(source)
    local result = {}
    for key, value in pairs(source) do
        result[key] = value
    end
    return result
end

local function prepare_owned_menu(menues, name, root_menu, title_text)
    local menu = rawget(menues, name)
    if menu ~= nil
        and (type(menu) ~= "table"
             or not rawget(menu, "__rescream_owned")) then
        return nil
    end
    if menu == nil then
        menu = {
            __rescream_owned = true,
            name = name,
            choices = {},
            choices_name = {},
            fields = {},
            refresh_on_user_change = "true",
            deactivate_callback = save_settings
        }
        menues[name] = menu
    end

    if type(menu.choices) ~= "table" then
        menu.choices = {}
    end
    if type(menu.choices_name) ~= "table" then
        menu.choices_name = {}
    end
    if type(menu.fields) ~= "table" then
        menu.fields = {}
    end
    local title = menu.fields[1]
    if type(title) ~= "table" then
        title = copy_table(root_menu.fields[1])
        menu.fields[1] = title
    end
    title.text = title_text
    title.original_text = title_text
    title.ignore_localization = true
    menu.name = name
    menu.refresh_on_user_change = "true"
    menu.deactivate_callback = save_settings
    return menu
end

local function upsert_choice(menu, marker_name, marker_value,
                             alternate_menu, text, position)
    local choices = menu.choices
    local choice = nil
    local choice_index = nil
    local index = 1
    while index <= #choices do
        local candidate = choices[index]
        local matches = type(candidate) == "table"
            and (rawget(candidate, marker_name) == marker_value
                 or (alternate_menu ~= nil
                     and candidate.menu == alternate_menu))
        if matches and choice == nil then
            choice = candidate
            choice_index = index
            index = index + 1
        elseif matches then
            table.remove(choices, index)
            if choice_index ~= nil and index < choice_index then
                choice_index = choice_index - 1
            end
        else
            index = index + 1
        end
    end

    if choice == nil then
        choice = {}
        table.insert(choices, choice)
        choice_index = #choices
    end

    choice[marker_name] = marker_value
    choice.text = text
    choice.original_text = text
    choice.ignore_localization = true

    if position ~= nil then
        table.remove(choices, choice_index)
        local target = math.min(position, #choices + 1)
        table.insert(choices, target, choice)
    end
    menu.choices_name[text] = choice
    return choice
end

local function install_menu(menu_manager)
    if type(menu_manager) ~= "table" then
        return false
    end

    local root_installer = rawget(_G, "__rescream_install_fov_menu")
    if type(root_installer) == "function" then
        root_installer(menu_manager)
    end

    local menues = rawget(menu_manager, "_menues")
    local root_menu = type(menues) == "table"
        and rawget(menues, "rescream_menu") or nil
    if type(root_menu) ~= "table"
        or not rawget(root_menu, "__rescream_owned")
        or type(root_menu.choices) ~= "table"
        or type(root_menu.fields) ~= "table"
        or type(root_menu.fields[1]) ~= "table" then
        return false
    end
    if type(root_menu.choices_name) ~= "table" then
        root_menu.choices_name = {}
    end

    local display_menu = prepare_owned_menu(
        menues, "rescream_display_menu", root_menu, "Display Mode")
    local debug_menu = prepare_owned_menu(
        menues, "rescream_debug_menu", root_menu, "Debug UI")
    if display_menu == nil or debug_menu == nil then
        return false
    end

    local windowed = upsert_choice(
        display_menu, "__rescream_display_mode", DISPLAY_WINDOWED,
        nil, "Windowed", 1)
    windowed.callback = function()
        set_display_mode(DISPLAY_WINDOWED)
    end
    windowed.status_check = function()
        return display_mode() == DISPLAY_WINDOWED
    end

    local fullscreen = upsert_choice(
        display_menu, "__rescream_display_mode", DISPLAY_FULLSCREEN,
        nil, "Fullscreen", 2)
    fullscreen.callback = function()
        set_display_mode(DISPLAY_FULLSCREEN)
    end
    fullscreen.status_check = function()
        return display_mode() == DISPLAY_FULLSCREEN
    end

    local borderless = upsert_choice(
        display_menu, "__rescream_display_mode", DISPLAY_BORDERLESS,
        nil, "Borderless Windowed", 3)
    borderless.callback = function()
        set_display_mode(DISPLAY_BORDERLESS)
    end
    borderless.status_check = function()
        return display_mode() == DISPLAY_BORDERLESS
    end

    local fps = upsert_choice(
        debug_menu, "__rescream_setting", "rescream_show_fps",
        nil, "Show FPS", 1)
    fps.callback = set_fps_setting
    fps.status_check = function()
        return setting_enabled("rescream_show_fps")
    end

    local packets = upsert_choice(
        debug_menu, "__rescream_setting",
        "rescream_show_network_packets", nil,
        "Show Steam Packet Stats", 2)
    packets.callback = set_network_setting
    packets.status_check = function()
        return setting_enabled("rescream_show_network_packets")
    end

    local display_navigation = upsert_choice(
        root_menu, "__rescream_menu", "rescream_display_menu",
        "rescream_display_menu", "Display Mode", 2)
    display_navigation.menu = "rescream_display_menu"

    local debug_navigation = upsert_choice(
        root_menu, "__rescream_menu", "rescream_debug_menu",
        "rescream_debug_menu", "Debug UI", 4)
    debug_navigation.menu = "rescream_debug_menu"
    return true
end

rawset(_G, "__rescream_install_quality_of_life_menu", install_menu)

local function install_menu_setup_hook()
    local menu_class = rawget(_G, "MainMenuManager")
    if type(menu_class) ~= "table"
        or type(menu_class.setup_menues) ~= "function" then
        return false
    end

    local hook_name = "__rescream_quality_of_life_setup_hook"
    local installed = rawget(menu_class, hook_name)
    if type(installed) == "table"
        and installed.owner == menu_class
        and type(installed.original) == "function"
        and type(installed.wrapper) == "function"
        and menu_class.setup_menues == installed.wrapper then
        return true
    end

    local original_name =
        "__rescream_original_setup_menues_quality_of_life"
    local original = menu_class.setup_menues
    local wrapper = function(self, ...)
        original(self, ...)
        local installer = rawget(
            _G, "__rescream_install_quality_of_life_menu")
        if type(installer) == "function" then
            installer(self)
        end
    end
    rawset(menu_class, original_name, original)
    rawset(menu_class, hook_name, {
        owner = menu_class,
        original = original,
        wrapper = wrapper
    })
    menu_class.setup_menues = wrapper
    return type(original) == "function"
end

rawset(_G, "__rescream_install_quality_of_life_hooks",
       install_menu_setup_hook)

seed_runtime_defaults()
local profile_ready = register_profile_settings()
local menu_hook_ready = install_menu_setup_hook()
local menu_ready = install_menu(current_menu_manager())

local display_hook_ready = install_display_hooks()
local resolution_hook_ready = install_resolution_changed_hook()
local display_ready
if rawget(_G, "__rescream_pending_render_settings_apply") == true then
    display_ready = request_render_settings_apply()
else
    display_ready = apply_display_mode()
end
local fps_ready = apply_fps_setting()
local network_ready = apply_network_setting()
local runtime_generation = rawget(
    _G, "__rescream_qol_runtime_generation")
if type(runtime_generation) ~= "table" then
    runtime_generation = {}
    rawset(_G, "__rescream_qol_runtime_generation", runtime_generation)
end
runtime_generation.overlay_global = current_global()
runtime_generation.viewport = current_viewport_manager()
runtime_generation.overlay_owner = rawget(_G, "Overlay")
runtime_generation.overlay_owners_initialized = true
if profile_ready then
    runtime_generation.global = current_global()
    runtime_generation.save = current_save_manager()
end
if menu_ready then
    local current_menu = current_menu_manager()
    runtime_generation.menu = current_menu
    runtime_generation.menues = type(current_menu) == "table"
        and rawget(current_menu, "_menues") or nil
end
if menu_hook_ready then
    runtime_generation.menu_class = rawget(_G, "MainMenuManager")
end
local debug_ready = fps_ready and network_ready
local complete = profile_ready and menu_hook_ready and menu_ready
    and display_hook_ready and display_ready and debug_ready

local readiness = rawget(_G, "__rescream_quality_of_life_readiness")
if type(readiness) ~= "table" then
    readiness = {}
    rawset(_G, "__rescream_quality_of_life_readiness", readiness)
end
readiness.profile = profile_ready
readiness.menu_hook = menu_hook_ready
readiness.menu = menu_ready
readiness.display_hook = display_hook_ready
readiness.display_applied = display_ready
readiness.resolution_hook = resolution_hook_ready
readiness.fps = fps_ready
readiness.network_packets = network_ready
readiness.debug_ui = debug_ready
readiness.complete = complete

rawset(_G, "__rescream_quality_of_life_status",
       "profile=" .. (profile_ready and "ready" or "waiting")
       .. " menu=" .. (menu_ready and "ready" or "waiting")
       .. " display="
       .. ((display_hook_ready and display_ready) and "ready" or "waiting")
       .. " resolution="
       .. (resolution_hook_ready and "ready" or "waiting")
       .. " fps=" .. (fps_ready and "ready" or "waiting")
       .. " packets=" .. (network_ready and "ready" or "waiting"))

rawset(_G, "__rescream_quality_of_life_menu", menu_ready)
rawset(_G, "__rescream_display_mode_hook",
       display_hook_ready and display_ready)
rawset(_G, "__rescream_debug_ui_hook", debug_ready)
rawset(_G, "__rescream_quality_of_life_fix", complete)
return complete
