local cls = rawget(_G, "MenuControllerBindBtn")
if type(cls) ~= "table" or type(cls.init) ~= "function" then
  return false
end

if rawget(cls, "__rescream_original_init") == nil then
  local original = cls.init
  rawset(cls, "__rescream_original_init", original)
  cls.init = function(self, ...)
    original(self, ...)
    if self._btn_string == nil then
      self._btn_string = ""
    end
  end
end

rawset(_G, "__rescream_keybind_fix", true)
return true
