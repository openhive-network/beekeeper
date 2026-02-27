<script setup lang="ts">
const { data } = useFetch('/api/get-version');

const version = ref("loading...");

if (import.meta.client) {
  import("@hiveio/beekeeper").then(async ({ default: createBeekeeper }) => {
    const bk = await createBeekeeper();

    // Also test client-side key import
    const session = bk.createSession("ses");
    const { wallet } = await session.createWallet("w0");
    const pk = await wallet.importKey("5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT");
    console.log(pk);

    version.value = bk.getVersion();
  });

  window.beekeeperLoaded = true;
}
</script>

<template>
  <div>
    <h3>SSR Version:</h3>
    {{ data.version }}
    <h3>Client version:</h3>
    {{ version }}
  </div>
</template>
