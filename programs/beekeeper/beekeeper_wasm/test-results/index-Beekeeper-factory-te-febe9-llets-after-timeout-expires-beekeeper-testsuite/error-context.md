# Instructions

- Following Playwright test failed.
- Explain why, be concise, respect Playwright best practices.
- Provide a snippet of code with the fix, if possible.

# Test info

- Name: __tests__/index.spec.ts >> Beekeeper factory tests for Node.js >> Should auto-lock wallets after timeout expires
- Location: __tests__/detailed/factory.ts:365:3

# Error details

```
BeekeeperError: Wallet locked due to timeout
```

# Test source

```ts
  106 | ;return new Uint8Array(await crypto.subtle.encrypt({name:"AES-CBC",iv},cryptoKey,data))},
  107 | async aes256CbcDecrypt(key,iv,data){try{
  108 | const cryptoKey=await crypto.subtle.importKey("raw",key,"AES-CBC",!1,["decrypt"])
  109 | ;return new Uint8Array(await crypto.subtle.decrypt({name:"AES-CBC",iv},cryptoKey,data))}catch{
  110 | return null}},async pbkdf2HmacSha512(password,salt,iterations,dkLen){try{
  111 | const passwordKey=0===password.length?new Uint8Array(1):password,baseKey=await crypto.subtle.importKey("raw",passwordKey,"PBKDF2",!1,["deriveBits"]),bits=await crypto.subtle.deriveBits({
  112 | name:"PBKDF2",hash:"SHA-512",salt,iterations},baseKey,8*dkLen);return new Uint8Array(bits)}catch{
  113 | return null}},getRandomBytes(dest){crypto.getRandomValues(dest)}}}
  114 | const createBeekeeper=async(beekeeperContstructor,ModuleExt={},options,storage)=>{
  115 | const crypto=createCryptoCallbacks();storage??={save_fn:()=>{},load_fn:name=>{
  116 | throw new Error("No persistent storage: "+name)},list_dir_fn:()=>[]}
  117 | ;const beekeeperProvider=await safeAsyncWasmCall((()=>beekeeperContstructor(ModuleExt)),"Beekeeper WASM module loading")
  118 | ;return new BeekeeperApi(beekeeperProvider,options,storage,crypto)
  119 | },LEGACY_WALLET_EXT=".wallet",LEGACY_WALLET_DIR=".beekeeper",OBJECT_STORE_NAME="FILE_DATA",openDb=dbName=>new Promise(((resolve,reject)=>{
  120 | const req=indexedDB.open(dbName);req.onupgradeneeded=()=>req.result.createObjectStore("FILE_DATA"),
  121 | req.onsuccess=()=>{const db=req.result
  122 | ;if(db.objectStoreNames.contains("FILE_DATA"))resolve(db);else{db.close()
  123 | ;const upgradeReq=indexedDB.open(dbName,db.version+1)
  124 | ;upgradeReq.onupgradeneeded=()=>upgradeReq.result.createObjectStore("FILE_DATA"),
  125 | upgradeReq.onsuccess=()=>resolve(upgradeReq.result),upgradeReq.onerror=()=>reject(upgradeReq.error)}
  126 | },req.onerror=()=>reject(req.error)})),preloadAll=(db,prefix)=>new Promise(((resolve,reject)=>{
  127 | const cache=new Map,tx=db.transaction("FILE_DATA","readonly"),req=tx.objectStore("FILE_DATA").openCursor()
  128 | ;req.onsuccess=()=>{const cursor=req.result;if(cursor){const key=cursor.key
  129 | ;key.startsWith(prefix)&&key.endsWith(".wallet")&&cache.set(key,new Uint8Array(cursor.value.contents)),
  130 | cursor.continue()}},tx.oncomplete=()=>resolve(cache),tx.onerror=()=>reject(tx.error)
  131 | })),webStorageCreator=async dbName=>{
  132 | const db=await openDb(dbName),keyPrefix=`${dbName}/.beekeeper/`,cache=await preloadAll(db,keyPrefix),toKey=name=>`${keyPrefix}${name}.wallet`
  133 | ;let pendingWrites=[];return{save_fn:(name,data)=>{const copy=new Uint8Array(data)
  134 | ;cache.set(toKey(name),copy);const p=new Promise(((resolve,reject)=>{
  135 | const tx=db.transaction("FILE_DATA","readwrite");tx.objectStore("FILE_DATA").put({contents:copy
  136 | },toKey(name)),tx.oncomplete=()=>resolve(),tx.onerror=()=>reject(tx.error)}));pendingWrites.push(p)
  137 | },load_fn:name=>{const data=cache.get(toKey(name))
  138 | ;if(!data)throw new Error("Wallet not found: "+name);return data},list_dir_fn:()=>{const names=[]
  139 | ;for(const key of cache.keys())names.push(key.slice(keyPrefix.length,-7));return names},
  140 | sync:async()=>{const writes=pendingWrites;pendingWrites=[],await Promise.all(writes)},close:()=>{
  141 | db.close()}}}
  142 | ;export{BeekeeperError,createCryptoCallbacks,webStorageCreator as createWebStorage,createBeekeeper as default};
      |                                  ^ BeekeeperError: Wallet locked due to timeout
```