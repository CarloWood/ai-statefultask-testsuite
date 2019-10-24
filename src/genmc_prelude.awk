/^ *\/\// { next }
/DEBUGGENMC/ { next }
/#ifdef/,/#endif/ { next }
/#if CW_DEBUG/,/#endif/ { next }
/#endif/ { next }
/^ *ASSERT\(/ { next }
/Dout\(/ { next }
/DoutEntering\(/ { next }
{ sub(/ *\/\/.*/, "") }
