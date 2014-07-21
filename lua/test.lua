local ucl = require("ucl")

function test_simple()
  local expect =
    '['..
    '"float",1.5,'..
    '"integer",5,'..
    '"true",true,'..
    '"false",false,'..
    '"null",null,'..
    '"string","hello",'..
    '"array",[1,2],'..
    '"object",{"key":"value"}'..
    ']'

  -- Input to to_value matches the output of to_string:
  local parser = ucl.parser()
  local got = ucl.to_string(parser:parse_string(expect))
  if expect == got then
    return 0
  else
   print(expect .. " == " .. tostring(got))
   return 1
  end
end

test_simple()
