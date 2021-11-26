--
-- Override configuration for Saturn
--
-- Copyright (C) 2020 Casa systems Inc.
--

return {
    DEFAULT_PAGE = "mlstatus.html",
    -- use builtin random number generator to generate random bytes instead of crypto secure random number generator.
    -- This should only be enabled on platforms that do not support the latter.
    fake_random_bytes = true,
}
