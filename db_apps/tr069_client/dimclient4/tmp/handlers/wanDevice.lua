local function getValue(node, name)
	return node.value
end

local function setValue(node, name, value)
	node.value = value
	return 0;
end

return {
	['getValue'] = getValue,
	['setValue'] = setValue
}
