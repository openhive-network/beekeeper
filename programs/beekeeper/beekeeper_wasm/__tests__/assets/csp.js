import createBeekeeper from '/dist/bundle/web.js';

const runBeekeeper = async () => {
  const beekeeper = await createBeekeeper({ inMemory: true });
  const session = beekeeper.createSession('csp-salt');

  const { wallet: created } = await session.createWallet('w0', 'pass');
  const publicKey = await created.importKey('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n');

  const wallet = await created.lock().unlock('pass');

  const encrypted = await wallet.encryptData('csp memo', publicKey, publicKey);
  const decrypted = await wallet.decryptData(encrypted, publicKey, publicKey);
  const signature = await wallet.signDigest(publicKey, '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542');

  await beekeeper.delete();

  return { publicKey, encrypted, decrypted, signature };
};

window.beekeeperCspRun = runBeekeeper();
