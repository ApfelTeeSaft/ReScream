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

local setting_id = 20001
local setting_name = "rescream_fov"
local default_fov = 60
local minimum_fov = 60
local maximum_fov = 110

local function clamp_fov(value)
    value = tonumber(value) or default_fov
    if value < minimum_fov then
        return minimum_fov
    end
    if value > maximum_fov then
        return maximum_fov
    end
    return value
end

local setting = rawget(available_settings, setting_name)
if setting == nil then
    local occupied_name = rawget(profile_ids, setting_id)
    if occupied_name ~= nil and occupied_name ~= setting_name then
        return false
    end
    save_manager:setup_profile_setting(
        setting_id, setting_name, default_fov)
    setting = rawget(available_settings, setting_name)
end

if type(setting) ~= "table" or setting.id ~= setting_id then
    return false
end

local stored_fov = rawget(profile_info, setting_id)
local selected_fov = stored_fov
if selected_fov == nil then
    selected_fov = save_manager:get_profile_setting(setting_name, true)
end
selected_fov = clamp_fov(selected_fov)
rawset(global, "rescream_fov", selected_fov)

if stored_fov ~= selected_fov then
    save_manager:set_profile_setting(setting_name, selected_fov)
end

local function set_fov(value)
    local clamped = clamp_fov(value)
    rawset(global, "rescream_fov", clamped)

    local current_managers = rawget(_G, "managers")
    local current_save = type(current_managers) == "table"
        and current_managers.save or nil
    if type(current_save) == "table"
        and type(current_save.set_profile_setting) == "function" then
        current_save:set_profile_setting(setting_name, clamped)
    end
end

local function save_fov()
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

local function add_parent_choice(parent)
    if type(parent) ~= "table" or type(parent.choices) ~= "table" then
        return false
    end

    local choice = nil
    for _, candidate in ipairs(parent.choices) do
        if candidate.menu == "rescream_menu" then
            choice = candidate
            break
        end
    end

    if choice == nil then
        choice = {
            text = "ReScream Menu",
            original_text = "ReScream Menu",
            ignore_localization = true,
            menu = "rescream_menu"
        }
        table.insert(parent.choices, choice)
    end

    if type(parent.choices_name) ~= "table" then
        parent.choices_name = {}
    end
    parent.choices_name["ReScream Menu"] = choice
    return true
end

local function choice_is_fov(candidate)
    return type(candidate) == "table"
        and (rawget(candidate, "__rescream_setting") == setting_name
             or rawget(candidate, "value") == "Global.rescream_fov"
             or (candidate.text == "Field of View"
                 and candidate.slider == "true"))
end

local function take_choice(menu, predicate)
    if type(menu) ~= "table" or type(menu.choices) ~= "table" then
        return nil
    end

    local result = nil
    local index = 1
    while index <= #menu.choices do
        local candidate = menu.choices[index]
        if predicate(candidate) then
            if result == nil then
                result = candidate
            end
            table.remove(menu.choices, index)
        else
            index = index + 1
        end
    end
    return result
end

local function prepare_owned_menu(menues, name, source_title, title_text)
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
            deactivate_callback = save_fov
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
        title = copy_table(source_title)
        menu.fields[1] = title
    end
    title.text = title_text
    title.original_text = title_text
    title.ignore_localization = true
    menu.name = name
    menu.refresh_on_user_change = "true"
    menu.deactivate_callback = save_fov
    return menu
end

local function install_menu(menu_manager)
    local menues = type(menu_manager) == "table"
        and rawget(menu_manager, "_menues") or nil
    if type(menues) ~= "table" then
        return false
    end

    local lag_settings = rawget(menues, "lag_settings")
    local legacy_settings = rawget(menues, "options_menu")
    local title_parent = lag_settings or legacy_settings
    local source_title = type(title_parent) == "table"
        and type(title_parent.fields) == "table"
        and title_parent.fields[1] or nil
    if type(source_title) ~= "table" then
        return false
    end

    local rescream_menu = prepare_owned_menu(
        menues, "rescream_menu", source_title, "ReScream Menu")
    if rescream_menu == nil then
        return false
    end

    local qol_menu = prepare_owned_menu(
        menues, "rescream_qol_menu", source_title, "Quality of Life")
    if qol_menu == nil then
        return false
    end

    local slider = take_choice(qol_menu, choice_is_fov)
    local legacy_slider = take_choice(rescream_menu, choice_is_fov)
    if slider == nil then
        slider = legacy_slider
    end
    if slider == nil then
        slider = {}
    end
    slider.__rescream_setting = setting_name
    slider.text = "Field of View"
    slider.original_text = "Field of View"
    slider.ignore_localization = true
    slider.slider = "true"
    slider.value = "Global.rescream_fov"
    slider.min = tostring(minimum_fov)
    slider.max = tostring(maximum_fov)
    slider.callback = set_fov
    table.insert(qol_menu.choices, 1, slider)
    qol_menu.choices_name["Field of View"] = slider
    rescream_menu.choices_name["Field of View"] = nil

    local function is_qol_navigation(candidate)
        return type(candidate) == "table"
            and (rawget(candidate, "__rescream_menu") ==
                    "rescream_qol_menu"
                 or candidate.menu == "rescream_qol_menu")
    end
    local navigation = take_choice(rescream_menu, is_qol_navigation) or {}
    navigation.__rescream_menu = "rescream_qol_menu"
    navigation.text = "Quality of Life"
    navigation.original_text = "Quality of Life"
    navigation.ignore_localization = true
    navigation.menu = "rescream_qol_menu"
    table.insert(rescream_menu.choices, 1, navigation)
    rescream_menu.choices_name["Quality of Life"] = navigation

    local installed = add_parent_choice(lag_settings)
    if legacy_settings ~= lag_settings and legacy_settings ~= nil then
        add_parent_choice(legacy_settings)
    end
    return installed
end

rawset(_G, "__rescream_install_fov_menu", install_menu)

local menu_ready = false
local menu_manager = managers_table.menu
local menu_class = rawget(_G, "MainMenuManager")
if type(menu_manager) == "table" and type(menu_class) == "table"
    and type(menu_class.setup_menues) == "function" then
    if rawget(menu_class, "__rescream_original_setup_menues") == nil then
        local original_setup_menues = menu_class.setup_menues
        rawset(menu_class, "__rescream_original_setup_menues",
               original_setup_menues)
        menu_class.setup_menues = function(self, ...)
            original_setup_menues(self, ...)
            local installer = rawget(_G, "__rescream_install_fov_menu")
            if type(installer) == "function" then
                installer(self)
            end
        end
    end
    menu_ready = install_menu(menu_manager)
end

local camera_ready = false
local player_camera = rawget(_G, "PlayerCamera")
if type(player_camera) == "table"
    and type(player_camera.camera_fov) == "function" then
    if rawget(player_camera, "__rescream_original_camera_fov") == nil then
        local original_camera_fov = player_camera.camera_fov
        rawset(player_camera, "__rescream_original_camera_fov",
               original_camera_fov)
        player_camera.camera_fov = function(self, ...)
            local camera_fov = original_camera_fov(self, ...)
            if type(camera_fov) ~= "number" then
                return camera_fov
            end

            local configured_fov = default_fov
            local current_global = rawget(_G, "Global")
            if type(current_global) == "table" then
                configured_fov = clamp_fov(
                    rawget(current_global, "rescream_fov"))
            end

            local adjusted_fov = camera_fov + configured_fov - default_fov
            if adjusted_fov < 1.01 then
                return 1.01
            end
            if adjusted_fov > 170 then
                return 170
            end
            return adjusted_fov
        end
    end
    camera_ready = true
end

rawset(_G, "__rescream_fov_menu", menu_ready)
rawset(_G, "__rescream_fov_camera_hook", camera_ready)
rawset(_G, "__rescream_fov_fix", menu_ready and camera_ready)
rawset(_G, "__rescream_fov_status",
       "menu=" .. (menu_ready and "ready" or "waiting")
       .. " camera=" .. (camera_ready and "ready" or "waiting"))
return menu_ready and camera_ready
