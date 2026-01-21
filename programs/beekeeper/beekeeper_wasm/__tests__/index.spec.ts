// Run tests in a specfic order:

import "./detailed/base";
import "./detailed/factory";

// Note: beekeeper_api.ts and storage.ts are skipped because they test the old heavy WASM
// implementation with Emscripten FS. The minimal WASM only provides crypto primitives.
// The high-level TypeScript API is tested through factory.ts.
