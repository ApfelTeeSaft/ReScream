local value = rawget(_G, "__rescream_display_version_override")
local td = rawget(_G, "tweak_data")
local page_class = rawget(_G, "BaseMenuPage")

if type(value) ~= "string" or value == "" then
  return false
end
if type(td) ~= "table" or type(td.menu) ~= "table" or
   type(td.menu.version) ~= "table" then
  return false
end
if type(page_class) ~= "table" or type(page_class.update) ~= "function" then
  return false
end

td.menu.version.client = value

if not page_class.__rescream_version_refresh_installed then
  local original_update = page_class.update

  page_class.update = function(self, time, delta_time)
    original_update(self, time, delta_time)

    local current = rawget(_G, "__rescream_display_version_override")
    local texts = type(self) == "table" and self._texts or nil
    local label = type(texts) == "table" and texts.version or nil
    local setter = label and label.set_text or nil

    if type(current) == "string" and current ~= "" and
       type(setter) == "function" and
       self.__rescream_display_version_text ~= current then
      setter(label, "v" .. current)
      self.__rescream_display_version_text = current
    end
  end

  page_class.__rescream_version_refresh_installed = true
end

rawset(_G, "__rescream_version_override", value)
return true
