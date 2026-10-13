local callback_fn = rawget(_G, "callback")
if type(callback_fn) ~= "function" then
  return false
end

local function configure_server_browser_callbacks(self)
  local browser = self and self._lobby_browser
  if browser == nil or type(browser.setup_callbacks) ~= "function" then
    return
  end

  local refresh = callback_fn(self, self, "_lobby_browser_refresh_callback")
  browser:setup_callbacks(refresh, refresh)
end

local function install_server_browser_fix()
  local cls = rawget(_G, "MenuServerSelectionSteam")
  if type(cls) ~= "table" then
    return false
  end
  if rawget(cls, "__rescream_lobby_browser_fixed") then
    return true
  end

  local original_cancel = cls._cancel_pending_queries
  local original_search = cls.search_for_servers
  if type(original_cancel) ~= "function" or type(original_search) ~= "function" then
    return false
  end

  rawset(cls, "__rescream_original_cancel_pending_queries", original_cancel)
  rawset(cls, "__rescream_original_search_for_servers", original_search)

  cls._cancel_pending_queries = function(self, ...)
    local result = original_cancel(self, ...)

    if self.__rescream_lobby_search_starting then
      self:_setup_interest_keys()
      self:_setup_lobby_filters()
    end
    return result
  end

  cls.search_for_servers = function(self, ...)
    self.__rescream_lobby_search_starting = true
    local first, second, third = original_search(self, ...)
    self.__rescream_lobby_search_starting = nil

    configure_server_browser_callbacks(self)
    return first, second, third
  end

  rawset(cls, "__rescream_lobby_browser_fixed", true)
  return true
end

local function quick_match_ready(self)
  if self.__rescream_lobby_search_resolved then
    return
  end

  local browser = self and self._lobby_browser
  if browser == nil or type(browser.lobbies) ~= "function" then
    return
  end
  local lobbies = browser:lobbies()
  if type(lobbies) ~= "table" or next(lobbies) == nil then
    return
  end

  self.__rescream_lobby_search_resolved = true
  self:_lobby_browser_updated_callback()
end

local function configure_quick_match_callbacks(self)
  local browser = self and self._lobby_browser
  if browser == nil or type(browser.setup_callbacks) ~= "function" then
    return
  end

  local ready = callback_fn(self, self, "_rescream_lobby_data_ready")
  browser:setup_callbacks(ready, ready)
end

local function install_quick_match_fix()
  local cls = rawget(_G, "MenuQuickMatchSteam")
  if type(cls) ~= "table" then
    return false
  end
  if rawget(cls, "__rescream_lobby_browser_fixed") then
    return true
  end

  local original_filters = cls._setup_lobby_filters
  local original_search = cls._search_for_servers
  if type(original_filters) ~= "function" or type(original_search) ~= "function" or
     type(cls._lobby_browser_updated_callback) ~= "function" then
    return false
  end

  rawset(cls, "__rescream_original_setup_lobby_filters", original_filters)
  rawset(cls, "__rescream_original_search_for_servers", original_search)
  cls._rescream_lobby_data_ready = quick_match_ready

  cls._setup_lobby_filters = function(self, ...)
    local first, second, third = original_filters(self, ...)
    local browser = self and self._lobby_browser
    local td = rawget(_G, "tweak_data")
    local menu = type(td) == "table" and td.menu or nil
    local version = type(menu) == "table" and menu.version or nil
    local settings = type(menu) == "table" and menu.settings or nil

    if browser ~= nil and type(version) == "table" and
       type(version.server) == "string" then
      browser:set_lobby_filter("server_version", version.server)
      if type(settings) == "table" and type(settings.max_servers_steam) == "number" then
        browser:set_max_lobby_return_count(settings.max_servers_steam)
      end
      browser:set_lobby_filter_slots(0)
    end
    return first, second, third
  end

  cls._search_for_servers = function(self, ...)
    self.__rescream_lobby_search_resolved = false
    configure_quick_match_callbacks(self)
    return original_search(self, ...)
  end

  rawset(cls, "__rescream_lobby_browser_fixed", true)
  return true
end

local server_browser_fixed = install_server_browser_fix()
local quick_match_fixed = install_quick_match_fix()
if server_browser_fixed and quick_match_fixed then
  rawset(_G, "__rescream_lobby_browser_fix", true)
  return true
end
return false
