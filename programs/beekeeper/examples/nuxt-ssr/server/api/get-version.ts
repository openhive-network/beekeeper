import createBeekeeper from "@hiveio/beekeeper";

const bkAsync = createBeekeeper({ inMemory: true });

export default defineEventHandler(async() => ({
  version: (await bkAsync).getVersion()
}));
