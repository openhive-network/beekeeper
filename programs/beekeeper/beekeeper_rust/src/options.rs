pub struct BeekeeperOptions {
    pub storage_root: String,
    pub unlock_timeout: u32,
    pub in_memory: bool,
}

impl BeekeeperOptions {
    pub fn new(storage_root: impl Into<String>) -> Self {
        Self {
            storage_root: storage_root.into(),
            unlock_timeout: 900,
            in_memory: false,
        }
    }

    pub fn unlock_timeout(mut self, seconds: u32) -> Self {
        self.unlock_timeout = seconds;
        self
    }

    pub fn in_memory(mut self, in_memory: bool) -> Self {
        self.in_memory = in_memory;
        self
    }
}
